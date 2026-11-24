#pragma once

#include <stdint.h>

namespace pong_rally {

constexpr uint32_t legacy_knee = 24576;
constexpr uint32_t maximum_repeats = 24;

// Preserve the original 1–12 steps and open up the upper quarter of the knob.
constexpr uint32_t repeats(uint32_t q15) {
    return q15 <= legacy_knee
        ? 1u + ((15u * q15 + 16384u) >> 15)
        : 12u + ((12u * (q15 - legacy_knee) + 4096u) >> 13);
}

// Nominal number of feedback passes to -60 dB. The wall, ducking and codec
// determine how many real reflections remain audible for a given signal.
static inline uint32_t feedback(uint32_t count) {
    static const uint16_t q16[maximum_repeats] = {
        66, 2072, 6554, 11654, 16462, 20724, 24429, 27636,
        30419, 32846, 34975, 36854, 38522, 40012, 41350, 42558,
        43653, 44649, 45560, 46396, 47165, 47876, 48534, 49145,
    };
    return q16[count - 1];
}

} // namespace pong_rally
