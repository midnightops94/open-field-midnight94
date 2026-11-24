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

extern "C" unsigned __udivsi3(unsigned number,unsigned divisor) noexcept {
    return divide(number,divisor).whole;
}

extern "C" int __divsi3(int number,int divisor) noexcept {
    const unsigned value=divide(magnitude(number),magnitude(divisor)).whole;
    const bool negative=(number<0)!=(divisor<0);
    return static_cast<int>(negative?0u-value:value);
}

extern "C" int __modsi3(int number,int divisor) noexcept {
    const unsigned value=divide(magnitude(number),magnitude(divisor)).remainder;
    return static_cast<int>(number<0?0u-value:value);
}

extern "C" unsigned __umulsi3_highpart(unsigned a,unsigned b) noexcept {
    const unsigned lo_a=a&65535u,hi_a=a>>16,lo_b=b&65535u,hi_b=b>>16;
    const unsigned low=lo_a*lo_b,left=lo_a*hi_b,right=hi_a*lo_b;
    const unsigned middle=(low>>16)+(left&65535u)+(right&65535u);
    return hi_a*hi_b+(left>>16)+(right>>16)+(middle>>16);
}
