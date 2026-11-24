#pragma once
#include <stdint.h>

namespace organ_rotor {

// One cabinet history per voice. The rotor clock lives on the audio thread.
struct Lane {
    int32_t bass;
    uint32_t last_tick;
};

struct Cursor {
    int32_t horn;
    int32_t drum;
    int32_t horn_step;
    int32_t drum_step;
};

// The display thread publishes the selected Organ sound's Spin, raw 0–32767.
void set_spin(unsigned raw);
uint32_t phase();

// Called once per 128-frame voice render. One iteration then handles two
// 88.2 kHz output frames, as in the approved dry voice paired-sample render.
Cursor begin_block(Lane& lane);

inline int32_t to_q31(int32_t value) {
    if (value > 131071) return INT32_MAX;
    if (value < -131072) return INT32_MIN;
    return value * 16384;
}

inline void stereo_pair(Lane& lane, Cursor& rotor, int32_t mono_q31, int& left, int& right) {
    const int32_t input = mono_q31 >> 14;
    // A one-pole crossover at about 690 Hz at the paired-sample rate.
    lane.bass += ((input - lane.bass) * 3) >> 5;
    const int32_t treble = input - lane.bass;
    const int32_t body = input - (treble >> 2);
    const int32_t width = ((treble * (rotor.horn >> 6)) >> 15)
                        + ((lane.bass * (rotor.drum >> 6)) >> 15);
    left = to_q31(body + width);
    right = to_q31(body - width);
    rotor.horn += rotor.horn_step;
    rotor.drum += rotor.drum_step;
}

}  // namespace organ_rotor
