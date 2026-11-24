#ifndef PPG_WORD_MATH_HPP
#define PPG_WORD_MATH_HPP

#include "ppg_native_reader.hpp"

namespace ppg_reader {
inline i32 interpolate(i32 left, i32 right, u32 fraction) {
    // Q16 fractions fit the unsigned low-half product. The approved source
    // tables are bounded by 0.851, so the signed difference stays in range.
    const i32 difference = right - left;
    const u32 low = static_cast<u32>(difference) & 65535;
    const i32 high = difference >> 16;
    return left + high * static_cast<i32>(fraction) + static_cast<i32>((low * fraction + 32768u) >> 16);
}

// The hot Wave path uses Q16 weights below unity. Keep the complete signed
// product in the Blackfin register pair. Its floor and the half-bit give the
// same round-toward-positive-infinity-on-ties result as interpolate().
inline i32 interpolate_wave(i32 left, i32 right, u32 fraction) {
#ifdef __bfin__
    const i32 difference = right - left;
    u32 low;
    i32 high;
    asm("(R1:0) = %2 * %3 (IS,NS);"
        : "=q0"(low), "=q1"(high)
        : "d"(difference), "d"(fraction) : "ASTAT");
    const u32 floor = (static_cast<u32>(high) << 16) | (low >> 16);
    return left + static_cast<i32>(floor + ((low >> 15) & 1u));
#else
    return interpolate(left, right, fraction);
#endif
}
}

#endif
