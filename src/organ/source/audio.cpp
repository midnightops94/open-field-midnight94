#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops")
#endif

#include "build_r6e_card.h"
#include "organ_drive.hpp"
#include "organ_rotor.hpp"
#include "waves.hpp"
#include "contributions/organ-drive-smoothing/baseline/source/organ_control_product_20261109/tone.hpp"

namespace {
constexpr unsigned render_frames = 128;
constexpr unsigned voice_bytes = 452;

struct Voice {
    organ_fast::Voice tone;
    organ_rotor::Lane cabinet;
    unsigned active;
    unsigned drive_word;
};
static_assert(sizeof(Voice) == voice_bytes, "Match Organ voice storage to recipe");

unsigned word(int value, unsigned high = 32767) {
    return value <= 0 ? 0 : unsigned(value) >= high ? high : unsigned(value);
}

void silence(volatile int* out, unsigned samples) {
    for (unsigned sample = 0; sample < samples; ++sample) out[sample] = 0;
}
}

extern "C" void organ_render(const Op1fVoiceBlock* block) OP1F_NAMED("organ_render");
extern "C" void organ_render(const Op1fVoiceBlock* block) {
    if (!block || block->bytes < sizeof(Op1fVoiceBlock) || !block->out) return;
    auto* voice = static_cast<Voice*>(block->state);
    if (!voice || block->frames != render_frames || block->rate != 88200 || !block->values) {
        const unsigned frames = block->frames < render_frames ? block->frames : render_frames;
        silence(block->out, 2 * frames);
        return;
    }
    if (block->events & OP1F_VOICE_FRESH) {
        organ_fast::initialize(voice->tone);
        voice->cabinet = {};
        voice->active = 0;
    }
    if (block->events & OP1F_VOICE_BEGINS) {
        organ_fast::start(voice->tone);
        voice->cabinet = {};
        voice->active = 1;
        voice->drive_word = word(block->values[5]);
    }
    const int* values = block->values;
    organ_rotor::set_spin(word(values[7]));
    const auto* rows = organ_waves::ready();
    if (!rows || !voice->active || !block->step || block->step >= 0x40000000u) {
        silence(block->out, 2 * render_frames);
        return;
    }
    // Convert the Field's one 88.2 kHz phase step to the approved
    // oscillator's 44.1 kHz step. The cabinet also processes one sample pair
    // at a time. The host keeps the envelope, glide and voice sum.
    organ::Controls controls;
    controls.value[organ::LOW] = word(values[1]);
    controls.value[organ::MIDDLE] = word(values[2]);
    controls.value[organ::HIGH] = word(values[3]);
    controls.value[organ::SPIN] = word(values[7]);
    controls.value[organ::TYPE] = word(values[4], 5);
    controls.value[organ::GRIT] = word(values[5]);
    controls.value[organ::PERC] = word(values[6]);
    controls.value[organ::VIBE] = word(values[0]);
    if (!organ_fast::render(voice->tone, block->out, block->step * 2, controls, rows)) return;

    const unsigned raw_drive = word(values[5]);
    const auto drive = organ_drive::prepare(raw_drive);
    const bool drive_changed = raw_drive != voice->drive_word;
    const auto previous = drive_changed ? organ_drive::prepare(voice->drive_word) : drive;
    voice->drive_word = raw_drive;
    auto cursor = organ_rotor::begin_block(voice->cabinet);
    for (unsigned sample = 0; sample < render_frames / 2; ++sample) {
        const unsigned n = 4 * sample;
        const int dry = block->out[n];
        int input = organ_drive::apply(drive_changed && sample < 32 ? previous : drive, dry);
        if (drive_changed && sample != 0 && sample != 63) {
            // Fade the old curve to dry and bring in the new one. One curve
            // per pair keeps the cost bounded when the rocker moves quickly.
            const int mix = sample < 32 ? 31 - int(sample) : int(sample) - 32;
            input = dry + (((input >> 5) - (dry >> 5)) * mix);
        }
        int left, right;
        organ_rotor::stereo_pair(voice->cabinet, cursor, input, left, right);
        block->out[n] = block->out[n + 2] = left;
        block->out[n + 1] = block->out[n + 3] = right;
    }
}
