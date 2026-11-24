#ifndef PPG_DUTY_HPP
#define PPG_DUTY_HPP

#include "ppg_native_reader.hpp"

namespace ppg_reader {
struct PulseDuty {
    u32 breakpoint, before, after, mip;
};

constexpr u32 duty_middle = 0x80000000u;

// ceil(2^30 / d) - 16384 at the 65 Q16 widths 16384 + 512*i.
extern const unsigned short pulse_reciprocal[65];

inline u32 pulse_slope(u32 width) {
    const u32 position = width - 16384;
    if (position == 32768) return 21846; // ceil(2^30 / 49152)
    const u32 index = position >> 9;
    const u32 fraction = position & 511;
    return 16384 + pulse_reciprocal[index] -
           (((pulse_reciprocal[index] - pulse_reciprocal[index + 1]) * fraction) >> 9);
}

inline PulseDuty make_duty(u32 value) {
    // Round Q30 to Q15, then map across 25–75% of a full oscillator period.
    const u32 width = 16384 + ((value + 16384) >> 15);
    const u32 distance = width < 32768 ? 32768 - width : width - 32768;
    if (!distance) return {duty_middle, 32768, 32768, 0};
    // log2(max slope) <= 4*distance in Q16 across this range.
    return {width << 16, pulse_slope(width), pulse_slope(65536 - width), 4 * distance};
}

inline u32 scale_duty_phase(u32 phase, u32 slope, u32 bias = 0) {
    // Exact 32x17 product >>15. All partial products fit 32 bits here.
    return (((phase >> 16) * slope) << 1) + ((((phase & 65535) * slope) + bias) >> 15);
}

inline u32 map_duty(u32 phase, const PulseDuty& duty) {
    if (!duty.mip) return phase;
    if (phase < duty.breakpoint) {
        const u32 warped = scale_duty_phase(phase, duty.before);
        return warped < duty_middle ? warped : duty_middle - 1;
    }
    const u32 remaining = scale_duty_phase(0u - phase, duty.after, 32767);
    return remaining < duty_middle ? 0u - remaining : duty_middle;
}

}

#endif
