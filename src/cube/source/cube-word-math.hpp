#pragma once
#include <stdint.h>

namespace cube_field {

struct WideWord {
    uint32_t low;
    uint32_t high;
};

#if defined(__bfin__)
static inline WideWord signed_product(uint32_t a, uint32_t b) {
    uint32_t low, high;
    asm("(R1:0) = %2 * %3 (IS,NS);" : "=q0"(low), "=q1"(high) : "d"(a), "d"(b) : "ASTAT");
    return {low, high};
}
#endif

static inline WideWord multiply_words(uint32_t a, uint32_t b) {
#if defined(__bfin__)
    WideWord product = signed_product(a,b);
    product.high += (uint32_t(int32_t(a)>>31) & b) + (uint32_t(int32_t(b)>>31) & a);
    return product;
#else
    const uint32_t a_low = a & 0xffffu;
    const uint32_t b_low = b & 0xffffu;
    const uint32_t low = a_low * b_low;
    const uint32_t a_cross = (a >> 16) * b_low;
    const uint32_t b_cross = a_low * (b >> 16);
    const uint32_t middle = (low >> 16) + (a_cross & 0xffffu) + (b_cross & 0xffffu);
    const uint32_t high = (a >> 16) * (b >> 16) + (a_cross >> 16) +
                          (b_cross >> 16) + (middle >> 16);
    return {(low & 0xffffu) | (middle << 16), high};
#endif
}

// The fractional widths in this kernel are all within 1..31.
static inline uint32_t right_word(WideWord value, unsigned bits) {
    return (value.low >> bits) | (value.high << (32 - bits));
}

static inline WideWord left_word(uint32_t value, unsigned bits) {
    return {value << bits, value >> (32 - bits)};
}

static inline uint32_t abs_word(int32_t value) {
    return value < 0 ? 0u - uint32_t(value) : uint32_t(value);
}

static inline int32_t product_q30(int32_t a, int32_t b) {
#if defined(__bfin__)
    const WideWord product = signed_product(a,b);
    // Round to nearest, resolving exact half-steps away from zero.
    const uint32_t bias = (1u<<29) - (product.high>>31);
    const uint32_t low = product.low + bias;
    return int32_t((low>>30) | ((product.high + uint32_t(low<product.low))<<2));
#else
    const uint32_t aa = abs_word(a), bb = abs_word(b);
    const uint32_t al = aa & 0xffffu, ah = aa >> 16;
    const uint32_t bl = bb & 0xffffu, bh = bb >> 16;
    const uint32_t cross_a = ah * bl, cross_b = al * bh;
    // Bits 0..15 are below the Q30 rounding bit, so only their carry matters.
    const uint32_t middle = ((al * bl) >> 16) + (cross_a & 0xffffu) +
                            (cross_b & 0xffffu) + (1u << 13);
    const uint32_t upper = ah * bh + (cross_a >> 16) + (cross_b >> 16);
    const uint32_t magnitude = (upper << 2) + (middle >> 14);
    return int32_t((a < 0) != (b < 0) ? 0u - magnitude : magnitude);
#endif
}

static inline int32_t gain_q15(int32_t value, uint16_t gain) {
#if defined(__bfin__)
    const WideWord product = signed_product(value, gain);
    const uint32_t bias = (1u << 14) - (product.high >> 31);
    const uint32_t rounded = product.low + bias;
    return int32_t((rounded >> 15) | ((product.high + uint32_t(rounded < product.low)) << 17));
#else
    const uint32_t amplitude = abs_word(value);
    const uint32_t low = (amplitude & 0xffffu) * gain + (1u << 14);
    const uint32_t high = (amplitude >> 16) * gain;
    const uint32_t magnitude = (high << 1) + (low >> 15);
    return int32_t(value < 0 ? 0u - magnitude : magnitude);
#endif
}

static inline int32_t step(int32_t from, int32_t to, uint32_t fraction, unsigned bits) {
    const bool decreasing = to < from;
    const uint32_t distance = decreasing ? uint32_t(from) - uint32_t(to)
                                         : uint32_t(to) - uint32_t(from);
    const WideWord product = multiply_words(distance, fraction);
    const uint32_t amount = right_word(product, bits);
    const bool truncated = (product.low & ((1u << bits) - 1u)) != 0u;
    return int32_t(decreasing ? uint32_t(from) - amount - truncated
                              : uint32_t(from) + amount);
}

// Coefficient creation only. Quotients with more than 32 bits are already beyond
// all admitted filter ranges. The sample loop does not perform division.
static inline uint32_t divide_word(WideWord numerator, uint32_t denominator, uint32_t* residue = nullptr) {
    if (numerator.high >= denominator) {
        if (residue) *residue = 0;
        return UINT32_MAX;
    }
    uint32_t remainder = numerator.high;
    uint32_t quotient = numerator.low;
    for (unsigned bit = 0; bit < 32; ++bit) {
        const bool carry = (remainder >> 31) != 0u;
        remainder = (remainder << 1) | (quotient >> 31);
        quotient <<= 1;
        if (carry || remainder >= denominator) {
            remainder -= denominator;
            quotient |= 1u;
        }
    }
    if (residue) *residue = remainder;
    return quotient;
}

}  // namespace cube_field
