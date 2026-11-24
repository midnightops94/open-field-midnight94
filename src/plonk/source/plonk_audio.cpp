#include "build_r6_card.h"
#include "plonk_core.hpp"
#include "plonk_voice_spring.hpp"
#include "spring_setting.hpp"

extern "C" void plonk_render(const Op1fVoiceBlock*) OP1F_NAMED("plonk_render");

namespace {
struct Voice {
    plonk::Voice oscillator;
    plonk_spring::VoiceTank tank;
    int last_pickup;
    int mix;
    unsigned initialized;
    unsigned spring_age;
    bool released;
};

static_assert(sizeof(Voice) == 1900, "voice_bytes must contain the whole Plonk voice");
unsigned sequence;
struct WetMeter { volatile unsigned value, time; };
WetMeter meters[6];
volatile unsigned meter_clock;

void publish(unsigned voice, unsigned level) {
    if (voice >= 6) return;
    meters[voice].value = level;
    meters[voice].time = ++meter_clock;
}
// At the end of each decay, fade and retire the 16-bit delay lines before
// feedback-rounding residue can sustain a quiet tone on a held voice.
constexpr unsigned short_end = 73728, long_end = 327680;
int return_gain(plonk::SpringSetting setting, unsigned age) {
    if (setting == plonk::spring_off) return 0;
    const bool long_tail = setting == plonk::spring_long;
    const unsigned end = long_tail ? long_end : short_end;
    if (age >= end) return 0;
    const int gain = long_tail ? 22938 : 19661;
    const unsigned shift = long_tail ? 16 : 13;
    const unsigned left = end-age;
    return left >= (1u << shift) ? gain : int((left*unsigned(gain)) >> shift);
}
int scale(int value, int amount) {
    // The 8-point linear interpolator supplies a pickup in eighths of a Q15 sample.
    // Split the bottom two bits to convert exactly to Q31 without a wide multiply.
    return (value >> 2) * amount + (((value & 3) * amount) >> 2);
}
void add_stereo(int* stereo, int wet) {
    const int sum = stereo[0] + wet;
    stereo[0] = stereo[1] = sum;
}
// For a constant return gain, groups of four samples advance by an integral
// number of Q31 units. Expand the three intermediate quarters exactly.
void add_fixed_return(int* stereo, int previous, int difference, int gain) {
    int wet = 2 * previous * gain;
    const int step = difference * gain;
    const int quarter = step >> 2;
    const int half = step >> 1;
    const int three_quarters = step - quarter - ((step & 3) != 0);
    for (unsigned group = 0; group < 2; ++group, stereo += 8) {
        add_stereo(stereo, wet + quarter);
        add_stereo(stereo + 2, wet + half);
        add_stereo(stereo + 4, wet + three_quarters);
        wet += step;
        add_stereo(stereo + 6, wet);
    }
}
}

extern "C" unsigned plonk_wet_level() {
    const unsigned now = meter_clock;
    unsigned level = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned value = meters[i].value;
        if (now - meters[i].time < 8 && value > level) level = value;
    }
    return level;
}

extern "C" void plonk_render(const Op1fVoiceBlock* block) {
    auto& v = *static_cast<Voice*>(block->state);
    const int* values = block->values;
    const auto setting = plonk::spring_setting(values[5]);
    if (!v.initialized || (block->events & (OP1F_VOICE_FRESH | OP1F_VOICE_BEGINS))) v.spring_age = 0;
    const int requested = return_gain(setting, v.spring_age);
    if (v.spring_age < long_end) v.spring_age += block->frames;
    const int first_feedback = setting == plonk::spring_long ? 31326 : 21824;
    const int second_feedback = setting == plonk::spring_long ? 30802 : 19825;
    if (!v.initialized || (block->events & OP1F_VOICE_FRESH)) {
        v.initialized = 1;
        v.oscillator.active = false;
        v.released = false;
        v.mix = requested;
        v.tank.clear();
        v.last_pickup = 0;
    }
    const plonk::Controls p{values[0], values[1], values[2], values[3]};
    if (block->events & OP1F_VOICE_BEGINS) {
        unsigned velocity = unsigned(block->velocity) >> 24;
        velocity = velocity > 127 ? 127 : velocity ? velocity : 1;
        const unsigned mode = plonk::wave_mode(unsigned(values[4]) << 16);
        plonk::start(v.oscillator, p, mode, velocity, sequence, true);
        v.released = false;
    }
    if (block->events & OP1F_VOICE_ENDS) v.released = true;
    const unsigned age = v.oscillator.age;
    plonk::render(v.oscillator, p, block->step, block->out, block->frames, v.released);
    if (!requested && !v.mix) {
        publish(block->voice, 0);
        return;
    }
    unsigned meter = 0;
    for (unsigned i = 0; i < block->frames; i += 8) {
        int excitation = 0;
        const unsigned frame_age = age + i + 3;
        if (frame_age < plonk_spring::attack_frames) {
            int sum = 0;
            for (unsigned j = 0; j < 8; ++j) sum += block->out[2 * (i+j)] >> 16;
            excitation = plonk_spring::excitation(sum >> 3, 32767, frame_age);
        }
        const int pickup = v.tank.run(excitation, first_feedback, second_feedback);
        const int previous = v.last_pickup;
        v.last_pickup = plonk_spring::clip(pickup + (pickup >> 2));
        const int difference = v.last_pickup - previous;
        int* group = block->out + 2*i;
        if (v.mix == requested) {
            add_fixed_return(group, previous, difference, requested);
        } else {
            int wet = previous * 8;
            for (unsigned j = 0; j < 8; ++j) {
                wet += difference;
                const int delta = requested - v.mix;
                v.mix += delta > 128 ? 128 : delta < -128 ? -128 : delta;
                add_stereo(group + 2*j, scale(wet, v.mix));
            }
        }
        const int level = (v.last_pickup * v.mix) >> 15;
        const unsigned amplitude = unsigned(level < 0 ? -level : level);
        if (amplitude > meter) meter = amplitude;
    }
    if (!v.mix) { v.tank.clear(); v.last_pickup = 0; }
    publish(block->voice, meter);
}
