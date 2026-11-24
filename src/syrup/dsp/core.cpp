#include "core.hpp"

namespace syrup {
namespace {

using i32 = int32_t;
using u16 = uint16_t;
using u32 = uint32_t;

#include "tables.inc"

constexpr i32 sample_unity = 1 << 27;
constexpr i32 section_limit = sample_unity * 4;
constexpr i32 output_knee = sample_unity * 7 / 8;
constexpr unsigned section_count = 3;
constexpr unsigned values_per_section = 3;
constexpr unsigned morph_frames = 64;
constexpr unsigned ramp_bits = 8;
constexpr unsigned pre_gain_index = 9;
constexpr unsigned compensation_index = 10;
constexpr unsigned wet_gain_index = 11;
constexpr unsigned mix_index = 12;

// Each supported compiler uses two's-complement arithmetic right shifts. The
// split multiply has no wide intermediate, including when the gain is unity.
static_assert((-1 >> 1) == -1, "Syrup requires arithmetic signed right shifts");

template<unsigned FractionBits, bool SignedCoefficient = false>
i32 multiply(i32 sample, u32 coefficient) noexcept {
    constexpr u32 mask = (1u << FractionBits) - 1;
    const u32 bias = static_cast<u32>(sample >> 31) & mask;
#if defined(__bfin__)
    if constexpr (FractionBits != 16) {
        // All current non-mix coefficients are representable unsigned 16-bit.
        // MAC1 uses signed high sample × unsigned coefficient and MAC0 unsigned low × unsigned.
        if constexpr (SignedCoefficient) {
            // The three filter coefficients are strictly below 32768. The
            // fractional high-word product supplies the required factor of 2.
            i32 result;
            asm("R3 = %2.L * %1.L (M), R2 = %2.L * %1.H;\n\t"
                "R3 = R3 + %3;\n\t"
                "R3 >>= %4;\n\t"
                "%0 = R2 + R3;"
                : "=d"(result) : "d"(sample), "d"(coefficient), "d"(bias),
                  "n"(FractionBits) : "R2", "R3", "CC");
            return result;
        }
        i32 result;
        asm("R3 = %1.H * %2.L (M), R2 = %1.L * %2.L (FU);\n\t"
            "R2 = R2 + %3;\n\t"
            "R2 >>= %4;\n\t"
            "R3 <<= %5;\n\t"
            "%0 = R2 + R3;"
            : "=d"(result) : "d"(sample), "d"(coefficient), "d"(bias),
              "n"(FractionBits), "n"(16 - FractionBits) : "R2", "R3", "CC");
        return result;
    }
#endif
    const i32 high = sample >> FractionBits;
    const u32 low_product = (static_cast<u32>(sample) & mask) * coefficient;
    return high * static_cast<i32>(coefficient) + static_cast<i32>((low_product + bias) >> FractionBits);
}

struct TablePosition {
    unsigned index;
    u32 fraction;
};

TablePosition cutoff_position(u16 control) noexcept {
    // The breakpoints contain 16%, 66%, and 18% of the control's travel.
    constexpr u32 mid_low = 10486;
    constexpr u32 mid_high = 53739;
    if (control == 65535) return {112, 0};
    if (control >= mid_high) {
        const u32 position = (((static_cast<u32>(control) - mid_high) << 16) / (65535 - mid_high)) << 5;
        return {80 + (position >> 16), position & 65535u};
    }
    if (control >= mid_low) {
        // Divide first to keep the long middle zone inside 32 bits.
        const u32 position = (((static_cast<u32>(control) - mid_low) << 16) / (mid_high - mid_low)) << 6;
        return {16 + (position >> 16), position & 65535u};
    }
    const u32 position = ((static_cast<u32>(control) << 16) / mid_low) << 4;
    return {position >> 16, position & 65535u};
}

TablePosition gain_position(u16 control) noexcept {
    if (control == 65535) return {64, 0};
    const u32 position = static_cast<u32>(control) << 6;
    return {position >> 16, position & 65535u};
}

u32 interpolate_distance(u32 difference, u32 fraction) noexcept {
    return (difference >> 16) * fraction + (((difference & 65535u) * fraction + 32768u) >> 16);
}

template<class Word>
u32 table_value(const Word* entries, TablePosition position) noexcept {
    const u32 first = entries[position.index];
    if (position.fraction == 0) return first;
    const u32 next = entries[position.index + 1];
    if (next >= first) return first + interpolate_distance(next - first, position.fraction);
    return first - interpolate_distance(first - next, position.fraction);
}

bool same_controls(const Parameters& a, const Parameters& b) noexcept {
    return a.cutoff == b.cutoff && a.resonance == b.resonance && a.drive == b.drive
        && a.mix == b.mix && a.wet_trim == b.wet_trim;
}

void set_section(i32* steps, u32 g, u32 damping) noexcept {
    // g is tan(pi f/fs) in Q14 and damping is 1/Q in Q14. The largest
    // g*(g+damping) at 17 kHz and 44.1 kHz is below 2^32.
    const u32 denominator = 16384 + ((g * (g + damping) + 8192) >> 14);
    const u32 twice = denominator >> 1;
    const u32 a2 = ((g << 15) + twice) / denominator;
    const u32 a3 = (a2 * g + 8192) >> 14;
    const u32 a1 = ((1u << 29) + twice) / denominator;
    // Keep a1+a3 strictly below one after integer rounding.
    steps[0] = static_cast<i32>(a1 + a3 >= 32768 ? 32767 - a3 : a1);
    steps[1] = static_cast<i32>(a2);
    steps[2] = static_cast<i32>(a3);
}

void configure(State& state, const Parameters& parameters) noexcept {
    if (state.initialized && same_controls(state.parameters, parameters)) return;

    i32 next[State::coefficient_count];
    for (unsigned i = 0; i < State::coefficient_count; ++i) next[i] = state.target[i] >> ramp_bits;
    if (!state.initialized || state.parameters.cutoff != parameters.cutoff ||
        state.parameters.resonance != parameters.resonance) {
        const auto position = cutoff_position(parameters.cutoff);
        const auto* tangent = state.sample_rate == 48000 ? cutoff_tangent_48000 : cutoff_tangent_44100;
        const u32 g = table_value(tangent, position);
        // Remaining q values are 0.517638, 0.707107, 1.931852 at Res zero.
        // Only the last section's damping changes. Q=16 is a finite hot ring.
        const u32 resonance = parameters.resonance == 65535 ? 65536u : parameters.resonance;
        const u32 curve = resonance == 65536u ? 65536u : (resonance * resonance) >> 16;
        const u32 last_damping = 8481 - ((7457u * curve) >> 16);
        set_section(next, g, 31651);
        set_section(next + values_per_section, g, 23170);
        set_section(next + 2 * values_per_section, g, last_damping);
    }
    const auto drive_position = gain_position(parameters.drive);
    next[pre_gain_index] = static_cast<i32>(table_value(input_gain, drive_position));
    next[compensation_index] = static_cast<i32>(table_value(drive_compensation, drive_position));
    next[wet_gain_index] = static_cast<i32>(table_value(trim_gain, gain_position(parameters.wet_trim)));
    next[mix_index] = parameters.mix == 65535 ? 65536 : parameters.mix;

    for (unsigned i = 0; i < State::coefficient_count; ++i) {
        state.target[i] = next[i] * (1 << ramp_bits);
        if (!state.initialized) state.current[i] = state.target[i];
        state.step[i] = (state.target[i] - state.current[i]) / static_cast<i32>(morph_frames);
    }
    state.transition_remaining = state.initialized ? morph_frames : 0;
    state.parameters = parameters;
    state.initialized = true;
}

void advance(State& state) noexcept {
    if (state.transition_remaining == 0) return;
    --state.transition_remaining;
    if (state.transition_remaining != 0) {
        state.current[0] += state.step[0];
        state.current[1] += state.step[1];
        state.current[2] += state.step[2];
        state.current[3] += state.step[3];
        state.current[4] += state.step[4];
        state.current[5] += state.step[5];
        state.current[6] += state.step[6];
        state.current[7] += state.step[7];
        state.current[8] += state.step[8];
        state.current[9] += state.step[9];
        state.current[10] += state.step[10];
        state.current[11] += state.step[11];
        state.current[12] += state.step[12];
    } else {
        state.current[0] = state.target[0];
        state.current[1] = state.target[1];
        state.current[2] = state.target[2];
        state.current[3] = state.target[3];
        state.current[4] = state.target[4];
        state.current[5] = state.target[5];
        state.current[6] = state.target[6];
        state.current[7] = state.target[7];
        state.current[8] = state.target[8];
        state.current[9] = state.target[9];
        state.current[10] = state.target[10];
        state.current[11] = state.target[11];
        state.current[12] = state.target[12];
    }
}

i32 bound(i32 value, i32 limit) noexcept {
    return value < -limit ? -limit : value > limit ? limit : value;
}

i32 magnitude_of(i32 sample) noexcept { return sample < 0 ? -sample : sample; }

i32 compress(i32 sample, i32 knee, unsigned slope_shift, i32 maximum) noexcept {
    const i32 magnitude = magnitude_of(sample);
    if (magnitude <= knee) return sample;
    const i32 compressed = knee + ((magnitude - knee) >> slope_shift);
    const i32 limited = compressed < maximum ? compressed : maximum;
    const i32 sign = sample >> 31;
    return (limited ^ sign) - sign;
}

i32 low_pass(i32 input, State::Section& section, const i32* current) noexcept {
    const u32 a1 = static_cast<u32>(current[0] >> ramp_bits);
    const u32 a2 = static_cast<u32>(current[1] >> ramp_bits);
    const u32 a3 = static_cast<u32>(current[2] >> ramp_bits);
    const i32 difference = input - section.integrator2;
    const i32 v1 = bound(multiply<15, true>(section.integrator1, a1) + multiply<15, true>(difference, a2), section_limit);
    const i32 v2 = bound(section.integrator2 + multiply<15, true>(section.integrator1, a2) +
                         multiply<15, true>(difference, a3), section_limit);
    section.integrator1 = bound(2 * v1 - section.integrator1, section_limit);
    section.integrator2 = bound(2 * v2 - section.integrator2, section_limit);
    // Below roughly -96 dBFS, with no input, clear a possible integer limit
    // cycle rather than keeping a silent slot's filter awake indefinitely.
    constexpr i32 quiet = 2048;
    if (input == 0 && (magnitude_of(section.integrator1) | magnitude_of(section.integrator2)) < quiet) {
        section = {};
        return 0;
    }
    return v2;
}

i32 process_channel(State& state, unsigned side, i32 input) noexcept {
    const i32 dry = input / 16;
    const u32 gain = static_cast<u32>(state.current[pre_gain_index] >> ramp_bits);
    i32 sample = dry;
    if (gain != 4096) {
        const i32 raised = multiply<12>(sample, gain);
        // Bring the knee in continuously from +0 to +6 dB of Drive. The
        // +0 dB path stays transparent even for a loud unfiltered input.
        const u32 knee_progress = gain >= 8192 ? 4096 : gain - 4096;
        const i32 knee = sample_unity - static_cast<i32>(knee_progress << 14);
        state.clipped |= magnitude_of(raised) > knee;
        const i32 shaped = compress(raised, knee, 3, knee + sample_unity / 8);
        sample = multiply<15>(shaped, static_cast<u32>(state.current[compensation_index] >> ramp_bits));
    }
    for (unsigned i = 0; i < section_count; ++i) {
        sample = low_pass(sample, state.channel[side][i], state.current + i * values_per_section);
    }
    const u32 mix = static_cast<u32>(state.current[mix_index] >> ramp_bits);
    if (mix == 0) return input;

    const i32 trimmed = multiply<14>(sample, static_cast<u32>(state.current[wet_gain_index] >> ramp_bits));
    const i32 wet = compress(trimmed, output_knee, 2, sample_unity - 1);
    return (dry + multiply<16>(wet - dry, mix)) * 16;
}

} // namespace

u32 cutoff_millihertz(u16 cutoff) noexcept {
    return table_value(cutoff_hz_milli, cutoff_position(cutoff));
}

u16 cutoff_from_millihertz(u32 frequency) noexcept {
    u32 first = 0;
    u32 last = 65535;
    while (first < last) {
        const u32 middle = (first + last) / 2;
        if (cutoff_millihertz(static_cast<u16>(middle)) < frequency) first = middle + 1;
        else last = middle;
    }
    if (first == 0 || cutoff_millihertz(static_cast<u16>(first)) < frequency) return static_cast<u16>(first);
    const u32 lower = cutoff_millihertz(static_cast<u16>(first - 1));
    const u32 upper = cutoff_millihertz(static_cast<u16>(first));
    return static_cast<u16>(frequency - lower <= upper - frequency ? first - 1 : first);
}

u32 drive_millidecibels(u16 drive) noexcept {
    return (18000u * drive + 32767u) / 65535u;
}

i32 wet_trim_millidecibels(u16 trim) noexcept {
    if (trim < 32768) return -static_cast<i32>((6000u * (32768 - trim) + 16384) / 32768);
    return static_cast<i32>((6000u * (trim - 32768) + 16383) / 32767);
}

void reset(State& state, u32 sample_rate) noexcept {
    state = {};
    state.sample_rate = sample_rate == 44100 || sample_rate == 48000 ? sample_rate : 0;
}

void process(State& state, const Parameters& parameters, const i32* in_l, const i32* in_r,
             i32* out_l, i32* out_r, u32 frames) noexcept {
    if (frames == 0) return;
    state.clipped = false;
    if (state.sample_rate == 0) {
        for (u32 i = 0; i < frames; ++i) {
            const i32 left = in_l[i];
            const i32 right = in_r[i];
            out_l[i] = left;
            out_r[i] = right;
        }
        return;
    }
    configure(state, parameters);
    // A settled zero-input block is exactly dry only when every filter
    // integrator is also zero. The normal prologue cleared the clip flag.
    bool silent = state.transition_remaining == 0;
    for (const auto& channel : state.channel)
        for (const auto& section : channel)
            silent &= section.integrator1 == 0 && section.integrator2 == 0;
    if (silent) {
        u32 i = 0;
        while (i < frames && (in_l[i] | in_r[i]) == 0) ++i;
        if (i == frames) {
            if (out_l != in_l || out_r != in_r)
                for (i = 0; i < frames; ++i) out_l[i] = out_r[i] = 0;
            return;
        }
    }
    for (u32 i = 0; i < frames; ++i) {
        const i32 left = in_l[i];
        const i32 right = in_r[i];
        advance(state);
        out_l[i] = process_channel(state, 0, left);
        out_r[i] = process_channel(state, 1, right);
    }
}

} // namespace syrup
