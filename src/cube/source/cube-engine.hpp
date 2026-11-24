#pragma once
#include "cube-word-math.hpp"
#include "cube-fixed.hpp"
#include "cube-tables.hpp"

namespace cube_field {

struct Controls {
    uint16_t shape, morph, reso, mix;
};

enum Coefficient : unsigned { B0, B2, A1, A2, Gain };
constexpr uint16_t unsettled = 0xffffu;

struct Engine {
    cube::State sample;
    int32_t coeff[3][5];
    int32_t targets[3][5];
    uint16_t direct, target_direct;
    uint32_t rate;
    uint16_t settled_shape, settled_morph, settled_reso;
    uint8_t phase;
    bool initialized;
};

static_assert(sizeof(Engine) == 184, "Cube instance layout");

static inline uint32_t clamp15(uint32_t x) {
    return x > 32767u ? 32767u : x;
}

static inline int32_t clamp_sym(int32_t x, int32_t limit) {
    return x > limit ? limit : x < -limit ? -limit : x;
}

static inline int32_t lerp(int32_t a, int32_t b, uint32_t t) {
    return step(a, b, t, 15);
}

// The post-filter sample bounds admit a direct signed Q15 interpolation.
static inline int32_t mix_sample(int32_t dry, int32_t wet, uint16_t mix) {
    const int32_t distance = wet - dry;
    return dry + (distance >> 15) * mix + ((distance & 32767) * mix >> 15);
}

static inline uint32_t lookup(const uint16_t* table, uint32_t q15) {
    const unsigned index = q15 >> 8;
    const unsigned fraction = q15 & 255u;
    return (uint32_t(table[index]) * (256u - fraction) + uint32_t(table[index + 1]) * fraction + 128u) >> 8;
}

static inline int32_t sinphase(uint32_t phase) {
    const unsigned quadrant = phase >> 30;
    uint32_t u = phase & 0x3fffffffu;
    if (quadrant & 1u) {
        u = 0x40000000u - u;
    }
    const unsigned index = u >> 23;
    const uint32_t fraction = u & 0x7fffffu;
    int32_t value = cube::sin_q30[index];
    if (index < 128u) {
        value = step(value, cube::sin_q30[index + 1], fraction, 23);
        const uint32_t t = fraction >> 7;
        const uint32_t curve = (t * (65536u - t)) >> 16;
        const uint32_t bend = right_word(multiply_words(uint32_t(value), 80853u), 30);
        value += int32_t((bend * curve) >> 16);
    }
    return quadrant >= 2u ? -value : value;
}

static inline void reset(Engine& e, uint32_t rate) {
    for (auto& channel : e.sample.z) {
        for (auto& band : channel) {
            band[0] = 0;
            band[1] = 0;
        }
    }
    for (auto& band : e.coeff) {
        for (int32_t& coefficient : band) {
            coefficient = 0;
        }
    }
    e.rate = rate;
    e.settled_shape = unsettled;
    e.phase = 0;
    e.initialized = false;
}

static inline void set_target(Engine& e, Controls p) {
    constexpr uint32_t one = 1u << 30;
    constexpr uint32_t phase_cap = 1803886264u;
    const uint32_t s = clamp15(p.shape);
    const uint32_t m = clamp15(p.morph);
    const uint32_t r = clamp15(p.reso);
    if (e.settled_shape == s && e.settled_morph == m && e.settled_reso == r) return;
    e.settled_shape = uint16_t(s);
    e.settled_morph = uint16_t(m);
    e.settled_reso = uint16_t(r);
    const uint32_t squared = (r * r) >> 15;
    const uint32_t rq = lookup(cube::q_q12, r) + 3u * squared;
    e.target_direct = uint16_t(7864 - ((r * 6062u) >> 15));
    const unsigned family = s >= 16384u ? 1u : 0u;
    const uint32_t shape = (s - family * 16384u) * 2u;
    // Fold wet makeup into the output coefficients. Broad bands overlap more,
    // so give them less makeup than narrow, high-resonance bands.
    static const int32_t weights[3][3] = {
        {32000, 27000, 13200}, {25500, 30700, 24600}, {17800, 28500, 33400},
    };
    // Shape chooses the resonator family. Morph stretches and retunes the
    // spaced modes within each family, with interpolation in log-frequency.
    static const int32_t log_frequencies[18] = {
        458752, 525754, 595730, 531057, 595730, 662129, 545386, 657731, 726367, 616654, 710255, 763681, 596593, 713804, 781796, 670356, 772103, 837100
    };
    const unsigned trim = 10000u + ((r * 38000u) >> 15); // Q14, .61–2.93

    for (unsigned band = 0; band < 3u; ++band) {
        const unsigned f0 = family * 6u + band;
        const unsigned f1 = (family + 1u) * 6u + band;
        const int32_t lo = lerp(log_frequencies[f0], log_frequencies[f0 + 3u], m);
        const int32_t hi = lerp(log_frequencies[f1], log_frequencies[f1 + 3u], m);
        const uint32_t log = uint32_t(lerp(lo, hi, shape));
        const unsigned octave = log >> 16;
        const unsigned index = (log & 65535u) >> 9;
        const unsigned remainder = log & 511u;
        const uint32_t exp = (uint32_t(cube::exp2_q14[index]) * (512u - remainder) +
                             uint32_t(cube::exp2_q14[index + 1u]) * remainder + 256u) >> 9;
        // The pinned frequency anchors keep octave at or below 13 and exp below 32768.
        const uint32_t hz256 = (exp << octave) >> 6;
        const uint32_t frequency_phase = divide_word(left_word(hz256, 24), e.rate);
        const uint32_t phase = frequency_phase > phase_cap ? phase_cap : frequency_phase;
        const uint32_t sine = uint32_t(sinphase(phase));
        const int32_t cosine = sinphase(phase + 0x40000000u);
        const uint32_t alpha = divide_word(left_word(sine, 11), rq);
        const uint32_t den = one + alpha;
        uint32_t b0_remainder;
        const int32_t b0 = int32_t(divide_word(left_word(alpha, 30), den, &b0_remainder));
        const uint32_t a1_abs = divide_word(left_word(abs_word(cosine), 31), den);
        const int32_t a1 = int32_t(cosine < 0 ? a1_abs : 0u - a1_abs);
        // A2 is 1 minus twice B0 before rounding. Convert B0's exact
        // remainder to the ceiling correction for A2's floor division.
        const unsigned correction = unsigned(b0_remainder != 0) + unsigned(b0_remainder > den - b0_remainder);
        const int32_t a2 = int32_t(one - 2u * uint32_t(b0) - correction);
        const int32_t family_gain = lerp(weights[family][band], weights[family + 1u][band], shape);
        const int32_t target_gain = (family_gain * int32_t(trim) + 16384) >> 15;
        auto& target = e.targets[band];
        target[B0] = b0;
        target[B2] = -b0;
        target[A1] = a1;
        target[A2] = a2;
        target[Gain] = target_gain;
    }
}

static inline int32_t approach(int32_t current, int32_t target) {
    const int32_t next = step(current, target, 8140u, 15);
    return next == current && current < target ? current + 1 : next;
}

static inline void update(Engine& e, Controls p) {
    set_target(e, p);
    if (!e.initialized) {
        for (unsigned band = 0; band < 3; ++band) {
            for (unsigned k = 0; k < 5; ++k) e.coeff[band][k] = e.targets[band][k];
        }
        e.direct = e.target_direct;
        e.initialized = true;
        return;
    }
    for (unsigned band = 0; band < 3; ++band) {
        auto& c = e.coeff[band];
        const auto& target = e.targets[band];
        c[B0] = approach(c[B0], target[B0]);
        c[B2] = -c[B0];
        c[A1] = approach(c[A1], target[A1]);
        c[A2] = approach(c[A2], target[A2]);
        c[Gain] = approach(c[Gain], target[Gain]);
    }
    e.direct = uint16_t(approach(e.direct, e.target_direct));
}

static inline int32_t contain_wet(int32_t wet) {
    constexpr uint32_t limit = (1u << 22) - 1u;
    constexpr uint32_t knee = 3u << 20;
    const uint32_t magnitude = abs_word(wet);
    if (magnitude <= knee) return wet;
    const uint32_t excess = magnitude - knee;
    const uint32_t softened = knee + (excess >> 2);
    const int32_t bounded = int32_t(softened < limit ? softened : limit);
    return wet < 0 ? -bounded : bounded;
}

} // namespace cube_field

#include "cube-room.hpp"

namespace cube_field {

template<bool paired_coefficients=false>
static inline void process(Engine& e, const int32_t* l, const int32_t* r, int32_t* ol,
                    int32_t* or_, unsigned frames, uint16_t mix,
                    ResonanceRoom* room = nullptr, unsigned tail_level = 0, unsigned morph = 0) {
    // State bounds, pinned gains and Q30 coefficients keep every accumulator below 2^31.
    for (unsigned frame = 0; frame < frames; ++frame) {
        const bool tail = room && (tail_level || room->level);
        for (unsigned channel = 0; channel < 2u; ++channel) {
            const int32_t* input = channel ? r : l;
            int32_t* output = channel ? or_ : ol;
            const int32_t original = input[frame];
            const int32_t x = original >> 9;
            int32_t wet = mix ? gain_q15(x, e.direct) : 0;
            for (unsigned band = 0; band < 3u; ++band) {
                const int32_t* c = e.coeff[band];
                int32_t* z = e.sample.z[channel][band];
                const int32_t feed = product_q30(c[B0], x);
                const int32_t y = clamp_sym(feed + z[0], 1 << 29);
                z[0] = clamp_sym(z[1] - product_q30(c[A1], y), 1 << 29);
                // The paired feedforward products cancel exactly when smoothing
                // has preserved the coefficient signs. Otherwise round each term.
                const int32_t opposite = paired_coefficients || c[B2] == -c[B0] ? -feed : product_q30(c[B2], x);
                z[1] = clamp_sym(opposite - product_q30(c[A2], y), 1 << 29);
                if (mix) {
                    // The contribution reduction only triggers above eight times
                    // the full-scale sample. Bound the three-band accumulator.
                    wet += 2 * gain_q15(clamp_sym(y, 1 << 25), uint16_t(c[Gain]));
                }
            }
            if (tail) {
                // Bound the loudest broadband input before decimation.
                room->send[channel] += clamp_sym(x + (wet >> 1), 1 << 22);
                if (abs_word(x) > 1) room->driven |= uint8_t(1u << channel);
                wet += room_return(*room, channel);
            }
            if (mix) {
                constexpr int32_t limit = (1 << 22) - 1;
                const int32_t w = contain_wet(wet);
                const int32_t scaled = mix == 32767u ? w : clamp_sym(mix_sample(x, w, mix), limit);
                output[frame] = scaled * 512;
            } else if (output != input) {
                output[frame] = original;
            }
        }
        if (tail && ++room->phase == room_divisor) {
            room->phase = 0;
            advance_room(*room, tail_level, morph);
        }
    }
}

static inline bool run(Engine& e, Controls p, const int32_t* l, const int32_t* r,
                int32_t* ol, int32_t* or_, unsigned frames, ResonanceRoom* room = nullptr) {
    if (e.rate < 8000u || e.rate > 192000u || !l || !r || !ol || !or_) {
        return false;
    }
    unsigned offset = 0;
    while (offset < frames) {
        if (e.phase == 0u) {
            update(e, p);
        }
        unsigned chunk = 64u - e.phase;
        if (chunk > frames - offset) {
            chunk = frames - offset;
        }
        process(e, l + offset, r + offset, ol + offset, or_ + offset, chunk,
                uint16_t(clamp15(p.mix)), room, room_level(clamp15(p.reso)), clamp15(p.morph));
        offset += chunk;
        e.phase = uint8_t((e.phase + chunk) & 63u);
    }
    return true;
}

}  // namespace cube_field
