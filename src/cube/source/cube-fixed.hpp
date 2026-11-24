#pragma once
#include <stdint.h>
#include <limits.h>
// Portable numeric experiment only. Coefficient production and Field ABI are NOT implemented.
namespace cube {
struct Coeff { int32_t c[3][5]; int32_t gain[3]; }; // c Q2.30, gain Q1.15
struct State { int32_t z[2][3][2]; };
static inline int32_t bound(int64_t x,int32_t lim){return x>lim?lim:x< -int64_t(lim)?-lim:int32_t(x);}
static inline int32_t mul(int32_t a,int32_t b,unsigned shift){return int32_t((int64_t(a)*b)>>shift);}
static inline void reset(State& s){s=State{};}
// Caller supplies one already-smoothed stable coefficient bank for a block.
// Ownership, serialization, aliasing, rate, bypass and coefficient producer are host contracts only.
static inline void process(State& s,const Coeff& p,const int32_t* l,const int32_t* r,int32_t* ol,int32_t* or_,unsigned frames,uint16_t mix){
 for(unsigned n=0;n<frames;n++)for(unsigned ch=0;ch<2;ch++){
   int32_t original=ch?r[n]:l[n];int32_t x=original>>9;int64_t wet=mul(x,819,15);
   for(unsigned b=0;b<3;b++){const int32_t* c=p.c[b];int32_t* z=s.z[ch][b];int32_t y=bound(int64_t(mul(c[0],x,30))+z[0],1<<29);int32_t z0=bound(int64_t(mul(c[1],x,30))-mul(c[3],y,30)+z[1],1<<29);int32_t z1=bound(int64_t(mul(c[2],x,30))-mul(c[4],y,30),1<<29);z[0]=z0;z[1]=z1;wet+=mul(p.gain[b],y,15);}
   int32_t w=bound(wet,(1<<22)-1);int32_t scaled=mix==32767?w:bound((int64_t(x)*(32768-mix)+int64_t(w)*mix)>>15,(1<<22)-1);int32_t output=mix?int32_t(int64_t(scaled)*512):original;
   (ch?or_:ol)[n]=output;
 }
}
}
