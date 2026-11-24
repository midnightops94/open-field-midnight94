#pragma once
#include <stdint.h>

namespace splice {
enum class Recut : uint8_t { free, quarter, eighth, sixteenth };

static inline Recut next_recut(Recut current) {
    switch (current) {
    case Recut::free: return Recut::quarter;
    case Recut::quarter: return Recut::eighth;
    case Recut::eighth: return Recut::sixteenth;
    case Recut::sixteenth: return Recut::free;
    }
    return Recut::free;
}
static inline const char* recut_label(Recut mode) {
    switch (mode) {
    case Recut::quarter: return "1/4";
    case Recut::eighth: return "1/8";
    case Recut::sixteenth: return "1/16";
    default: return "FREE";
    }
}
// Audio-thread snapshot. These are normalized sample units, not device
// addresses. Phase is evaluated at the first sample of this same callback.
// Epoch changes on start, stop, seek, or a discontinuous clock retime.
struct ClockBlock {
    uint64_t quarter_phase_q16;
    uint32_t sixteenth_period_q16;
    uint32_t epoch;
    bool valid, running;
};
struct RecutPlan {
    bool active;
    uint32_t samples_to_grid;
};
static inline RecutPlan plan_recut(Recut mode, const ClockBlock& clock) {
    if (!clock.valid || !clock.running || clock.sixteenth_period_q16 < (256u<<16))
        return {};
    const uint64_t quarter = uint64_t(clock.sixteenth_period_q16)<<2;
    if (clock.quarter_phase_q16 >= quarter)
        return {};
    uint64_t period;
    switch (mode) {
    case Recut::quarter: period = quarter; break;
    case Recut::eighth: period = quarter>>1; break;
    case Recut::sixteenth: period = quarter>>2; break;
    default: return {};
    }
    uint64_t elapsed = clock.quarter_phase_q16;
    // A quarter contains at most four requested divisions, so no
    // variable-width divide or modulus is needed in the audio adapter.
    if (elapsed >= (period<<1)) elapsed -= period<<1;
    if (elapsed >= period) elapsed -= period;
    // A fractional line is processed by the first sample at or after it.
    if (elapsed < (1ull<<16))
        return {true,0};
    return {true,uint32_t((period-elapsed+65535)>>16)};
}
}
