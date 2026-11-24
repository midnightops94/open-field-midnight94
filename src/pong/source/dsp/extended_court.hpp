#pragma once

#include "scalable/field_controls_173.hpp"

namespace pong_extended_court {

using pong_field173_candidate::field_unit;
using pong_field173_candidate::min_court;
using pong_field173_candidate::sample_rate;

constexpr uint32_t maximum_samples(unsigned bits) {
    return bits == 12u ? sample_rate :
           bits == 8u ? sample_rate * 3u / 2u :
           bits == 4u ? sample_rate * 3u : 0u;
}

constexpr uint16_t maximum_position(unsigned bits) {
    return bits == 12u ? 24576u : bits == 8u ? 28672u : bits == 4u ? 32767u : 0u;
}

inline uint16_t texture_change_stop(unsigned from_bits, unsigned to_bits) {
    const uint16_t from = maximum_position(from_bits);
    const uint16_t to = maximum_position(to_bits);
    return from < to ? from : to;
}

// All textures use the same absolute scale. Keep the original 20–510 ms
// scale in the lower half and put the three limits on exact stop positions.
inline uint32_t requested_samples(int32_t court_word) {
    const uint32_t midpoint = 32768u;
    const uint32_t stop_12 = 2u * maximum_position(12u);
    const uint32_t stop_8 = 2u * maximum_position(8u);
    const uint32_t middle = (min_court + maximum_samples(12u)) / 2u;
    const uint32_t position = field_unit(court_word);
    if (position <= midpoint)
        return min_court + (((middle - min_court) * position) >> 15);
    if (position <= stop_12)
        return middle + (((maximum_samples(12u) - middle) * (position - midpoint)) >> 14);
    if (position <= stop_8)
        return maximum_samples(12u) + (((maximum_samples(8u) - maximum_samples(12u)) *
                   (position - stop_12)) >> 13);
    return maximum_samples(8u) + (((maximum_samples(4u) - maximum_samples(8u)) *
               (position - stop_8)) >> 13);
}

inline uint32_t fitted_samples(uint32_t requested, uint32_t limit) {
    return requested < limit ? requested : limit;
}

// Court displays the later tap. Preserve the original signed Spin direction
// and stereo ratio, rounded to whole samples. The maximum product below fits
// uint32_t. There is one integer division per block and no per-sample work.
inline bool map(int32_t court_word, int32_t spin_word, unsigned bits, uint32_t& left, uint32_t& right) {
    const uint32_t limit = maximum_samples(bits);
    if (!limit || (uint32_t(court_word) >> 16) > 32767u || (uint32_t(spin_word) >> 16) > 32767u) {
        return false;
    }
    const uint32_t later = fitted_samples(requested_samples(court_word), limit);
    const uint32_t spin = field_unit(spin_word);
    const uint32_t magnitude = pong_field173_candidate::spin_magnitude(spin);
    const uint32_t offset = mod_fx::divide_unsigned(later * magnitude, 65536u + magnitude);
    const uint32_t earlier = later - 2u * offset;
    left = spin < 32768u ? earlier : later;
    right = spin < 32768u ? later : earlier;
    return true;
}

inline uint32_t milliseconds(uint32_t samples) {
    return mod_fx::divide_unsigned(samples * 1000u + sample_rate / 2u, sample_rate);
}

static_assert(maximum_samples(4u) < pong_field173_candidate::largest_ring, "Court must fit the existing rings");
static_assert(maximum_samples(4u) <= UINT32_MAX / 21845u, "Spin offset product must fit");

}  // namespace pong_extended_court
