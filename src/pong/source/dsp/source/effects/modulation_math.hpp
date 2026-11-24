#ifndef FIELD_LAB_MODULATION_MATH_HPP
#define FIELD_LAB_MODULATION_MATH_HPP
#include <stdint.h>
#include <limits.h>
#include "level.hpp"
// Original fixed-point Q31 utilities for host / BF707. No heap, floating
// point, runtime 64-bit division or DSP-unbounded library helpers.
namespace mod_fx {
using i32=int32_t;using u32=uint32_t;
constexpr u32 unity=65536;
inline i32 saturated_add(i32 a,i32 b){
#if defined(__bfin__)
    i32 sum;
    asm("%0 = %1 + %2 (S);" : "=d"(sum) : "d"(a), "d"(b) : "ASTAT");
    return sum;
#else
    const i32 result=i32(u32(a)+u32(b));
    if(a>0&&b>0&&result<0)return INT32_MAX;
    if(a<0&&b<0&&result>=0)return INT32_MIN;
    return result;
#endif
}
inline i32 difference_mix(i32 a,i32 b,u32 amount){
    if(amount==0)return a;
    if(amount>=unity)return b;
    return saturated_add(level_fx::gain(a,unity-amount),level_fx::gain(b,amount));
}
inline i32 q16mul(i32 sample,u32 amount){
    return level_fx::gain(sample,amount>unity?unity:amount);
}
inline u32 unit(int16_t v){return v<=0?0:v==32767?unity:u32(v)*2u;}
// Constant-iteration unsigned division to avoid a libgcc runtime dependency.
// Called for sample-rate setup and block-rate control coefficients only.
inline u32 divide_unsigned(u32 n,u32 d){
    if(!d)return 0;
    u32 q=0,rem=0;
    for(unsigned i=0;i<32;++i){
        const u32 carry=rem>>31;
        rem=(rem<<1)|(n>>31);n<<=1;q<<=1;
        if(carry||rem>=d){rem-=d;q|=1u;}
    }
    return q;
}
// Table generated once; single fractional interpolation is continuous.
extern const int16_t sine_q15[257];
inline i32 sine(u32 phase){
    const u32 k=phase>>24,frac=(phase>>8)&65535u;
    return i32(sine_q15[k])+((i32(sine_q15[k+1])-sine_q15[k])*i32(frac))/65536;
}
inline u32 hz_phase_step(u32 milli_hz,u32 sample_rate){
    // 0xffffffff / SR fits and the subsequent product fits for supported
    // 8k..96k sample rates and 0..6.7Hz modulation speeds.
    return divide_unsigned(milli_hz*divide_unsigned(0xffffffffu,sample_rate),1000u);
}
inline i32 delay_read(const i32*ring,u32 mask,u32 write,u32 delay_q16){
    const u32 whole=delay_q16>>16,frac=delay_q16&65535u;
    return difference_mix(ring[(write-whole)&mask],ring[(write-whole-1)&mask],frac);
}
}
#endif
