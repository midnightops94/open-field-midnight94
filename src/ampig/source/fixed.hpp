#pragma once
#include "curves.hpp"

namespace fixed {
// Q7.24 covers the largest driven positive signal and the ADAA band history.
// Products use the same high and low halves in portable and target builds.
constexpr int one=1<<24;
constexpr int q(double v) {return int(v*one+(v<0?-.5:.5));}
inline int clamp(int a,int lo,int hi){return a<lo?lo:a>hi?hi:a;}
inline int absq(int a){return a<0?-a:a;}
inline int mag_max(int a,int b){return a>b?a:b;}
template<unsigned shift> inline int product(int a,int b){
#if defined(__bfin__) || defined(__BFIN__)
    unsigned lo,hi;
    asm("(R1:0) = %2 * %3 (IS,NS);" : "=q0"(lo),"=q1"(hi) : "d"(a),"d"(b) : "ASTAT");
    return int((lo>>shift)|(hi<<(32-shift)));
#else
    return int((static_cast<long long>(a)*b)>>shift);
#endif
}
inline int mul(int a,int b){return product<24>(a,b);}
inline int blend(int a,int b,int t){return a+mul(b-a,t);}
inline int within(int x){return clamp(x,0,one);}
inline int q15_to_unit(int x) {int v=clamp(x,0,32767); return v*512+((v+32)>>6);}
inline int to_pcm(int x){ return x>=one?2147483647:x<=-one?(-2147483647-1):x*128; }
inline int from_pcm(int x){return x>>7;}
inline int control_table(const int* table,int x){
    if(x>=one)return table[256];
    const unsigned i=unsigned(x)>>16,f=unsigned(x)&65535;
    return table[i]+product<16>(table[i+1]-table[i],int(f));
}
// For denominator b in Q24, normalize it to [1,2) and linearly interpolate
// 257 Q30 reciprocal samples. The bounded numerator in each caller keeps the
// result representable. No integer divide or runtime math library is needed.
inline int ratio(int a,int b){
    if(!b) return 0;
    if(b<0){b=-b;a=-a;}
#if defined(__bfin__) || defined(__BFIN__)
    int signbits;
    asm("%0.L = SIGNBITS %1;" : "=d"(signbits): "d"(b));
    const int shift=short(signbits)-6;
#else
    const int shift=__builtin_clz(unsigned(b))-7;
#endif
    const unsigned norm=shift>=0?(unsigned(b)<<shift):(unsigned(b)>>-shift);
    const unsigned t=norm-(1u<<24),i=t>>16,f=t&65535;
    const int inv=curves::inverse[i]+product<16>(curves::inverse[i+1]-curves::inverse[i],int(f));
    // Normalize small numerator and denominator together before multiplying.
    if(shift>0) return product<30>(int(unsigned(a)<<shift),inv);
    return product<30>(a,inv)>>-shift;
}
}
