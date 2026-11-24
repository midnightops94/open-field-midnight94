#include "plonk_core.hpp"

namespace plonk {
#include "plonk_tables.inc"

namespace {
constexpr u32 decay_limit = 12u << 28;
constexpr u32 release_step = 67633;
constexpr u32 release_limit = 2040351718u; // -ln(.0005) in Q28.
constexpr u32 attack_step = 47554;

inline i32 clamp(i32 x, i32 low, i32 high) {
    return x < low ? low : x > high ? high : x;
}
inline i32 q15(i32 a, i32 b) { return (a * b) >> 15; }
inline i32 q12(i32 a, i32 b) { return (a * b) >> 12; }
inline i32 lane(i32 raw) { return clamp((raw >> 16) + ((raw & 0xffff) >= 32768), 0, unity); }

// Blend an 8-bit fraction between bounded table neighbors. The table units
// are defined by each caller, and coefficients fit the signed intermediate.
template<class Value, unsigned N>
inline i32 lookup(const Value (&table)[N], u32 cell, unsigned fraction) {
    const i32 first = table[cell], next = table[cell + 1];
    return first + (((next - first) * static_cast<i32>(fraction) + 128) >> 8);
}

template<class Value, unsigned N>
inline i32 control_lookup(const Value (&table)[N], unsigned control) {
    if (control == unity) return table[N-1];
    return lookup(table, control >> 7, (control & 127u) << 1);
}

inline i32 exp_negative(u32 phase) {
    if (phase >= decay_limit) return 0;
    return lookup(decay_curve, phase >> 23, (phase >> 15) & 255u);
}

inline i32 exp_two(i32 phase) {
    const i32 whole = phase >> 15;
    const u32 fraction = static_cast<u32>(phase) & 32767u;
    const i32 value = control_lookup(time_curve, fraction);
    return whole < 0 ? value >> -whole : value << whole;
}

inline i32 random_signed(u32& counter) {
    u32 word = 1234567u + (++counter * 0x9e3779b9u);
    word ^= word >> 16;
    word *= 0x7feb352du;
    word ^= word >> 15;
    word *= 0x846ca68bu;
    word ^= word >> 16;
    return static_cast<i32>(word >> 16) - unity;
}

// Used for the two rates derived per block. Both operands are positive.
u32 divide(u32 numerator, u32 divisor) {
    u32 marker = 1;
    while (divisor <= numerator >> 1) {
        divisor <<= 1;
        marker <<= 1;
    }
    u32 quotient = 0;
    while (marker) {
        if (numerator >= divisor) {
            numerator -= divisor;
            quotient |= marker;
        }
        marker >>= 1;
        divisor >>= 1;
    }
    return quotient;
}

u32 ratio_q20(unsigned control) {
    control = control > unity ? unity : control;
    const u32 normalized = control * 100;
    constexpr u32 knee = 33u * unity;
    const bool upper = normalized > knee;
    const u32 numerator = ((upper ? normalized - knee : normalized) << 8);
    const u32 position = divide(numerator, upper ? 67 : 33);
    const auto& table = upper ? upper_ratios : lower_ratios;
    if (position >= 256u * unity) return table[256];
    const u32 cell = position >> 15;
    const u32 fraction = position & 32767u;
    const u32 linear = table[cell] + (((table[cell + 1] - table[cell]) * fraction + 16384u) >> 15);
    // The two ratio curves are exponential. Remove their measured local
    // curvature from the chord between table points, once per audio block.
    const u32 center = cell ? cell : 1;
    const u32 curvature = table[center - 1] + table[center + 1] - 2u * table[center];
    const u32 weight = (fraction * (unity - fraction) + 16384u) >> 15;
    return linear - ((curvature * weight + 32768u) >> 16);
}

inline u32 scale_phase(u32 phase, u32 ratio_q20) {
    const u32 ratio = ratio_q20 >> 4;
    const u32 fraction = ratio & 65535u;
    return phase * (ratio >> 16) + (phase >> 16) * fraction +
           (((phase & 65535u) * fraction) >> 16) +
           (((phase >> 4) * (ratio_q20 & 15u)) >> 16);
}

inline i32 sin_phase(u32 phase) { return lookup(sine, phase >> 24, (phase >> 16) & 255u); }
inline i32 triangle(u32 phase) {
    const i32 centered = static_cast<i32>((phase + 0x40000000u) >> 16) - 32768;
    return 32768 - 2 * __builtin_abs(centered);
}
inline i32 carrier(u32 phase, unsigned wave) {
    if (wave == 1) return triangle(phase);
    const i32 base = sin_phase(phase);
    if (wave != 2) return base;
    const i32 magnitude = control_lookup(square, base < 0 ? -base : base);
    return base < 0 ? -magnitude : magnitude;
}
}

unsigned wave_mode(unsigned raw) {
    const i32 control = static_cast<i32>(raw) >> 16;
    return control < 8192 ? 0 : control < 24576 ? 1 : 2;
}

Controls controls(const i32* raw) { return {lane(raw[0]), lane(raw[1]), lane(raw[2]), lane(raw[3])}; }

void start(Voice& v, Controls params, unsigned wave, unsigned velocity, u32& counter, bool stock_velocity) {
    v.phase = v.mod_phase = v.age = v.attack_phase = v.release_phase = 0;
    v.filter = 0;
    const i32 chance = control_lookup(chance_curve, params.chance);
    const i32 fold_random = random_signed(counter);
    const i32 time_random = random_signed(counter);
    const i32 bright_random = random_signed(counter);
    const i32 gain_random = random_signed(counter);
    v.fold_variation = 4096 + q15(1843, q15(chance, fold_random));
    v.time_variation = 4096 + q15(2253, q15(chance, time_random));
    v.brightness_variation = 4096 + q15(2253, q15(chance, bright_random));
    v.gain_variation = unity + q15(3932, q15(chance, gain_random));
    v.attack_step = attack_step;
    v.mod_ratio = unity;
    v.mod_variation = 4096;
    v.wave = wave;
    v.stock_velocity = stock_velocity;
    // 32768/127 is represented by the 19-bit rounded reciprocal.
    v.velocity = ((velocity * 264208u) + 512u) >> 10;
    v.cached_ratio_control = -1;
    v.cached_decay_samples = 0;
    v.active = true;
    if (params.chance <= unity / 2) return;
    const i32 blend = 2 * params.chance - unity;
    const i32 weight = q15(blend, blend);
    v.fold_variation = clamp(q12(v.fold_variation, exp_two(2 * q15(weight, fold_random))), 614, 16384);
    v.time_variation = clamp(q12(v.time_variation, exp_two(2 * q15(weight, time_random))), 614, 16384);
    v.brightness_variation = clamp(q12(v.brightness_variation, exp_two(2 * q15(weight, bright_random))), 614, 16384);
    v.attack_step = q12(attack_step, exp_two(weight * (gain_random > 0 ? -4 : 2)));
    const i32 harmonic = bright_random < -16384 ? -16384 : bright_random < 0 ? 0 : bright_random < 16384 ? 16384 : 32768;
    v.mod_ratio += q15(weight, harmonic);
    const i32 displacement = q15(weight, fold_random);
    v.mod_variation = exp_two(displacement + (displacement >> 1));
}

void render(Voice& v, Controls params, u32 step, i32* __restrict out, unsigned count, bool released) {
    if (!v.active) {
        for (unsigned i = 0; i < 2*count; ++i) out[i] = 0;
        return;
    }
    const i32 strike2 = q15(params.strike, params.strike);
    const i32 decay_time = q12(287 + q15(7086, strike2), v.time_variation);
    const u32 decay_samples = (static_cast<u32>(decay_time) * 22050u + 512) >> 10;
    if (v.cached_decay_samples != decay_samples) {
        v.cached_decay_samples = decay_samples;
        v.cached_decay_step = divide((1u << 28) + (decay_samples >> 1), decay_samples);
    }
    const u32 decay_step = v.cached_decay_step;
    u32 decay_phase = v.age >= 12 * decay_samples ? decay_limit : v.age * decay_step;
    if (params.ratio != v.cached_ratio_control) {
        v.cached_ratio_control = params.ratio;
        v.cached_ratio = ratio_q20(params.ratio);
    }
    const u32 ratio = v.cached_ratio;
    const u32 articulation_ratio = static_cast<u32>(v.mod_ratio);
    const u32 mod_ratio = (ratio >> 15) * articulation_ratio +
                          (((ratio & 32767u) * articulation_ratio) >> 15);
    const i32 modulation = q12(834 + q15(8344, params.fold), v.mod_variation);
    const i32 drive = clamp(4096 + q12(q15(17203, q15(params.fold, params.fold)), v.fold_variation), 4096, 32768);
    const i32 fold_blend = q15(23593, params.fold);
    const i32 tone_gain = q15(q12(1800 + q15(6500, params.strike), v.brightness_variation),
                              18022 + q15(14746, v.velocity));
    const i32 velocity_gain = v.stock_velocity ? unity : 9175 + q15(23593, v.velocity);
    const i32 output_gain = q15(q15(3441, v.gain_variation), velocity_gain);
    // Open-Field hands one pitch for this entire block.
    const u32 mod_step = scale_phase(step, mod_ratio);
    for (unsigned i = 0; i < count; ++i) {
        v.phase += step;
        v.mod_phase += mod_step;
        const i32 env = q15(static_cast<i32>(v.attack_phase >> 9), exp_negative(decay_phase));
        decay_phase += decay_step;
        if (v.attack_phase < (1u << 24)) {
            v.attack_phase += v.attack_step;
            if (v.attack_phase > (1u << 24)) v.attack_phase = 1u << 24;
        }
        const i32 offset = q15(sin_phase(v.mod_phase), modulation);
        const i32 wave = carrier(v.phase + (static_cast<u32>(offset) << 16), v.wave);
        const i32 folded = triangle(static_cast<u32>(wave * drive) << 3);
        const i32 signal = wave + q15(folded - wave, fold_blend);
        const i32 cutoff = clamp(300 + q15(env, tone_gain), 100, 17640);
        const i32 alpha = lookup(filter_curve, static_cast<u32>(cutoff) >> 6, (cutoff & 63) << 2);
        v.filter += q15(signal - v.filter, alpha);
        if (released && v.release_phase < release_limit) v.release_phase += release_step;
        const i32 level = v.release_phase ? q15(env, exp_negative(v.release_phase)) : env;
        const i32 sample = q15(v.filter, q15(level, output_gain)) * 65536;
        out[2*i] = sample;
        out[2*i+1] = sample;
    }
    v.age += count;
    if (v.age >= 12u*rate || v.release_phase >= release_limit) v.active = false;
}

extern "C" unsigned plonk_ratio_q16(unsigned q15_control) {
    return (ratio_q20(q15_control) + 8) >> 4;
}
}
