#ifndef FIELD_LAB_SYRUP_CORE_HPP
#define FIELD_LAB_SYRUP_CORE_HPP

#include <stdint.h>

namespace syrup {

// Controls span 0..65535. Wet trim has an exact unity point at 32768.
struct Parameters {
    uint16_t cutoff;
    uint16_t resonance;
    uint16_t drive;
    uint16_t mix;
    uint16_t wet_trim;
};

constexpr Parameters default_parameters() noexcept {
    return {58269, 0, 0, 65535, 32768};
}

uint32_t cutoff_millihertz(uint16_t cutoff) noexcept;
uint16_t cutoff_from_millihertz(uint32_t frequency) noexcept;
uint32_t drive_millidecibels(uint16_t drive) noexcept;
int32_t wet_trim_millidecibels(uint16_t trim) noexcept;

struct State {
    static constexpr unsigned coefficient_count = 13;

    struct Section {
        int32_t integrator1;
        int32_t integrator2;
    };

    Section channel[2][3]{};
    int32_t current[coefficient_count]{};
    int32_t target[coefficient_count]{};
    int32_t step[coefficient_count]{};
    Parameters parameters{};
    uint32_t sample_rate = 44100;
    unsigned transition_remaining = 0;
    bool initialized = false;
    bool clipped = false;
};

static_assert(sizeof(State) <= 512, "Syrup exceeded its slot state budget");

// Supported rates are 44100 and 48000. An unsupported rate clears state and
// passes dry. Each owner calls reset after acquiring a fresh slot or audio rate.
void reset(State&, uint32_t sample_rate = 44100) noexcept;

// Planar Q31. Exact corresponding in-place buffers are supported. Output
// planes must not overlap each other; partial or cross-plane overlap is outside
// this interface. Control changes ramp over 64 frames. At a settled Mix zero
// the output is exactly dry. A zero-frame call changes neither state nor flags.
void process(State&, const Parameters&, const int32_t* in_l,
             const int32_t* in_r, int32_t* out_l,
             int32_t* out_r, uint32_t frames) noexcept;

// Reports input-knee crossings in the last nonempty process call. It is not an
// output clip detector or a sticky UI meter. Publish an owner-local snapshot
// through the adapter before reading it from another thread.
inline bool did_clip(const State& state) noexcept {
    return state.clipped;
}

} // namespace syrup

#endif
