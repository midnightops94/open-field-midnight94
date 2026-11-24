#include <stdint.h>
#include <stddef.h>

namespace {

unsigned width32(uint32_t value) {
    unsigned bits = 0;
    if (value >= 0x10000u) { value >>= 16; bits += 16; }
    if (value >= 0x100u) { value >>= 8; bits += 8; }
    if (value >= 0x10u) { value >>= 4; bits += 4; }
    if (value >= 0x4u) { value >>= 2; bits += 2; }
    if (value >= 0x2u) { value >>= 1; ++bits; }
    return bits + value;
}

unsigned width64(uint64_t value) {
    const auto high = static_cast<uint32_t>(value >> 32);
    return high ? 32u + width32(high) : width32(static_cast<uint32_t>(value));
}

uint64_t divide_unsigned(uint64_t numerator, uint64_t denominator) {
    if (denominator < 3u) {
        return denominator == 2u ? numerator >> 1 : denominator == 1u ? numerator : 0u;
    }
    if (numerator < denominator) {
        return 0;
    }
    const unsigned bits = width64(denominator);
    if ((denominator & (denominator - 1)) == 0) {
        return numerator >> (bits - 1);
    }
    const unsigned offset = width64(numerator) - bits;
    uint64_t shifted = denominator << offset;
    uint64_t quotient = 0;
    for (unsigned remaining = offset + 1; remaining != 0; --remaining) {
        quotient <<= 1;
        if (numerator >= shifted) {
            numerator -= shifted;
            quotient |= 1;
        }
        shifted >>= 1;
    }
    return quotient;
}

uint64_t magnitude(int64_t value) {
    const auto raw = static_cast<uint64_t>(value);
    return value >= 0 ? raw : 0u - raw;
}

} // namespace

extern "C" uint64_t __muldi3(uint64_t left, uint64_t right) {
    const auto left_word = static_cast<uint32_t>(left);
    const auto right_word = static_cast<uint32_t>(right);
    const uint32_t a = left_word & 0xFFFFu;
    const uint32_t b = right_word & 0xFFFFu;
    const uint32_t c = left_word >> 16;
    const uint32_t d = right_word >> 16;
    const uint32_t low_product = a * b;
    const uint32_t left_cross = a * d;
    const uint32_t right_cross = c * b;
    const uint32_t middle = (low_product >> 16) + (left_cross & 0xFFFFu) + (right_cross & 0xFFFFu);
    const uint32_t low = (middle << 16) | (low_product & 0xFFFFu);
    const uint32_t high = c * d + (left_cross >> 16) + (right_cross >> 16) + (middle >> 16) +
                          left_word * static_cast<uint32_t>(right >> 32) +
                          right_word * static_cast<uint32_t>(left >> 32);
    return (static_cast<uint64_t>(high) << 32) | low;
}

extern "C" uint64_t __udivdi3(uint64_t numerator, uint64_t denominator) {
    return divide_unsigned(numerator, denominator);
}

extern "C" int64_t __divdi3(int64_t numerator, int64_t denominator) {
    const uint64_t result = divide_unsigned(magnitude(numerator), magnitude(denominator));
    return static_cast<int64_t>((numerator < 0) == (denominator < 0) ? result : 0u - result);
}

extern "C" uint32_t __udivsi3(uint32_t numerator, uint32_t denominator) {
    return static_cast<uint32_t>(divide_unsigned(numerator, denominator));
}

extern "C" int32_t __divsi3(int32_t numerator, int32_t denominator) {
    return static_cast<int32_t>(__divdi3(numerator, denominator));
}

#if defined(__bfin__) || defined(__BFIN__)
extern "C" void* memset(void* destination, int fill, size_t count) {
    auto* bytes = static_cast<volatile unsigned char*>(destination);
    while (count != 0) {
        *bytes++ = static_cast<unsigned char>(fill);
        --count;
    }
    return destination;
}
#endif
