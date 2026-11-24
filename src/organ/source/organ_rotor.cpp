#include "build_r6e_card.h"
#include "organ_rotor.hpp"

namespace organ_rotor {
namespace {

struct Position {
    int32_t horn;
    int32_t drum;
};

struct Clock {
    uint32_t sequence;
    uint32_t horn_phase;
    uint32_t drum_phase;
    int32_t horn_speed;
    int32_t drum_speed;
    Position before;
    Position after;
};

Clock clock{};
volatile unsigned selected_spin;
volatile uint32_t shown_phase;

// Four quadrants share a quarter-wave table. Only the clock stage samples it.
constexpr int16_t sine_quarter[17] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787, 23170,
    25329, 27245, 28898, 30273, 31356, 32137, 32609, 32767
};

int32_t sine(uint32_t angle) {
    const unsigned offsets = angle >> 22;
    unsigned x = offsets & 255u;
    if (offsets & 256u) x = 256u - x;
    const unsigned index = x >> 4;
    int32_t value = sine_quarter[index];
    if (x & 15u) value += ((sine_quarter[index + 1] - value) * int32_t(x & 15u)) >> 4;
    return offsets & 512u ? -value : value;
}

int32_t ease_speed(int32_t speed, int32_t target, int32_t scale, unsigned divisor_shift) {
    const int32_t difference = target - speed;
    if (!difference) return speed;
    const int32_t step = (difference * scale) >> divisor_shift;
    return speed + (step ? step : 1);
}

int32_t gain(uint32_t angle, int32_t speed, int32_t maximum) {
    const int32_t moving = speed >> 7;
    const int32_t depth = (maximum * (moving > 32767 ? 32767 : moving)) >> 15;
    return (sine(angle) * depth) >> 15;
}

int32_t scale_q15(int32_t value, int32_t gain) {
    return (value >> 15) * gain + (((value & 32767) * gain) >> 15);
}

void tick() {
    const int32_t raw = int32_t(selected_spin);
    const int32_t spin = raw + (raw >> 14);
    const int32_t wanted = spin < 655 ? 0 : 5033165 + scale_q15(52848230, spin);
    // The approved model advances phase 128 times by (scale >> 7).
    // Keep its settled speed endpoints and ramp them once per block here.
    const int32_t horn_target = scale_q15(wanted, 24348) & ~127;
    const int32_t drum_target = scale_q15(scale_q15(wanted, 25559), 24348) & ~127;
    clock.horn_speed = ease_speed(clock.horn_speed, horn_target, 5,
                                  horn_target > clock.horn_speed ? 11u : 12u);
    clock.drum_speed = ease_speed(clock.drum_speed, drum_target, 7, 13u);
    clock.horn_phase += uint32_t(clock.horn_speed);
    clock.drum_phase += uint32_t(clock.drum_speed);
    clock.before = clock.after;
    clock.after = {gain(clock.horn_phase, clock.horn_speed, 7168),
                   gain(clock.drum_phase, clock.drum_speed, 3840)};
    ++clock.sequence;
    shown_phase = clock.horn_phase;
}

}  // namespace

void set_spin(unsigned raw) {
    selected_spin = raw < 32767u ? raw : 32767u;
}

uint32_t phase() {
    return shown_phase;
}

Cursor begin_block(Lane& lane) {
    const Position from = lane.last_tick == clock.sequence ? clock.after : clock.before;
    lane.last_tick = clock.sequence;
    return {from.horn * 64, from.drum * 64,
            clock.after.horn - from.horn, clock.after.drum - from.drum};
}

}  // namespace organ_rotor

extern "C" void organ_rotor_tick(const Op1fBlock*) OP1F_NAMED("organ_rotor_tick");
extern "C" void organ_rotor_tick(const Op1fBlock* block) {
    if (block && block->bytes >= 36u && block->point == OP1F_AT_MASTER_OUT &&
        block->host == 2u && block->frames == 64u) {
        // Audio is left untouched. Declaring the direct-buffer form avoids
        // the base's stereo copy that normally precedes a listen-only stage.
        organ_rotor::tick();
    }
}
