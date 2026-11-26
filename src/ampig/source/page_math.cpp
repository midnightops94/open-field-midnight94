// Portable integer division for the native page coordinate calculations.
#include <stdint.h>

namespace {
struct Quotient { unsigned whole,remainder; };

Quotient divide(unsigned number,unsigned divisor) noexcept {
    if (!divisor) return {0,number};
    unsigned multiple=1;
    while (divisor<=(number>>1)) { divisor<<=1; multiple<<=1; }
    unsigned result=0;
    do {
        if (number>=divisor) {number-=divisor;result|=multiple;}
        divisor>>=1;
        multiple>>=1;
    } while (multiple);
    return {result,number};
}

unsigned magnitude(int signed_value) noexcept {
    const unsigned word=static_cast<unsigned>(signed_value);
    return signed_value<0?0u-word:word;
}
}

extern "C" int __divsi3(int number,int divisor) noexcept {
    const unsigned value=divide(magnitude(number),magnitude(divisor)).whole;
    const bool negative=(number<0)!=(divisor<0);
    return static_cast<int>(negative?0u-value:value);
}

// Blackfin GCC can lower a signed division with nonnegative operands to this
// helper. Keep it local just like the signed display-coordinate operations.
extern "C" unsigned __udivsi3(unsigned number, unsigned divisor) noexcept {
    return divide(number, divisor).whole;
}

extern "C" int __modsi3(int number,int divisor) noexcept {
    const unsigned value=divide(magnitude(number),magnitude(divisor)).remainder;
    return static_cast<int>(number<0?0u-value:value);
}
