#pragma once

#include "contributions/organ-drive-smoothing/baseline/source/organ_control_product_20261109/tone.hpp"

namespace organ_drive {
using organ_fast::product_q15;

enum class Curve { off, first, second, both, continuous };
struct Plan {
    Curve curve;
    int grit;
    int wet;
    int gain;
};

// The approved compensation law, evaluated once for a voice block.
constexpr int gains[] = {32768, 32768, 32768, 32293, 31019, 29388, 27535, 25584,
    23636, 21759, 19993, 18362, 16869, 15516, 14294, 13195, 12207};

inline Plan prepare(unsigned raw) {
    switch (raw) {
    case 0: return {Curve::off, 0, 0, 32768};
    case 10922: return {Curve::first, 0, 8192, 23754};
    case 21845: return {Curve::second, 0, 16384, 12205};
    case 32767: return {Curve::both, 0, 8192, 14750};
    default:
        const int grit = organ_fixed::control(raw);
        const unsigned index = unsigned(grit) >> 11;
        const int gain = gains[index] + (((gains[index + 1] - gains[index]) * (grit & 2047)) >> 11);
        return {Curve::continuous, grit, product_q15(grit, 19661), gain};
    }
}

inline int saturate(int x) {
    const unsigned magnitude = unsigned(x < 0 ? -x : x);
    if (magnitude >= 8u * organ_fixed::one)
        return x < 0 ? -organ_fixed::saturation[256] : organ_fixed::saturation[256];
    const unsigned i = magnitude >> 18;
    const int a = organ_fixed::saturation[i];
    const int y = a + product_q15(organ_fixed::saturation[i + 1] - a, (magnitude & 262143) >> 3);
    return x < 0 ? -y : y;
}

inline int apply(const Plan& plan, int mono_q31) {
    // The complete Drive transfers are preserved at steady settings, here
    // applied to a single voice. Tone is represented in signed Q23 below.
    if (plan.curve == Curve::off) return mono_q31;
    const int clean = (mono_q31 >> 9) * 2;
    int output;
    switch (plan.curve) {
    case Curve::first:
        output = product_q15(clean, 24576) + (saturate(3 * clean) >> 2);
        break;
    case Curve::second:
        output = (clean >> 1) + (saturate(6 * clean) >> 1);
        break;
    case Curve::both:
        output = (clean >> 1) + (saturate(3 * clean) >> 2) + (saturate(6 * clean) >> 2);
        break;
    default:
        output = product_q15(clean, 32768 - plan.grit) +
                 product_q15(saturate(clean + 5 * product_q15(clean, plan.grit)), plan.wet);
        break;
    }
    return organ_fixed::clamp(product_q15(output, plan.gain), -organ_fixed::one, organ_fixed::one - 1) * 256;
}
}
