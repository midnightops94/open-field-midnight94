#ifndef FIELD_LAB_LEVEL_HPP
#define FIELD_LAB_LEVEL_HPP
#include <stdint.h>
// Portable host prototype. NOT a verified OP-1 Field effects ABI.
// Q16 controls use 65536 for exact unity. Audio is signed Q31, stereo interleaved.
namespace level_fx {
using i32=int32_t;
using u32=uint32_t;
static_assert(sizeof(i32)==4,"Require 32-bit audio words");
constexpr u32 one=65536;
constexpr u32 transition_frames=64;
struct Parameters {u32 level; i32 balance; u32 width; u32 mix; bool bypass;};
inline Parameters defaults(){return {one,0,one,one,false};}
inline u32 bounded(u32 x){return x>one?one:x;}
inline Parameters clamp(Parameters p){
    p.level=bounded(p.level);p.width=bounded(p.width);p.mix=bounded(p.mix);
    if(p.balance>i32(one))p.balance=i32(one);
    if(p.balance< -i32(one))p.balance= -i32(one);
    return p;
}
inline i32 gain(i32 x,u32 g){
    static_assert((-1 >> 16) == -1, "Arithmetic shift required");
    const i32 hi=x >> 16;
    return hi*i32(g)+i32(((u32(x)&65535u)*g)>>16);
}
inline i32 blend(i32 a,i32 b,u32 g){
    if(a==b)return a;
    return gain(a,one-g)+gain(b,g);
}
inline i32 half_floor(i32 x){return x>=0?x/2:-1-i32((~u32(x))>>1);}
inline i32 midpoint(i32 a,i32 b){
    return half_floor(a)+half_floor(b)+i32(((u32(a)&1u)+(u32(b)&1u))>>1);
}
struct Ramp {
    i32 value,target,step,sign;
    u32 remaining,remainder,error;
    void snap(i32 v){value=target=v;step=sign=0;remaining=remainder=error=0;}
    void set(i32 v){
        if(v==target)return;
        target=v;const i32 delta=v-value;
        step=delta/i32(transition_frames);
        const i32 rest=delta%i32(transition_frames);
        sign=rest<0?-1:rest>0?1:0;
        remainder=u32(rest<0?-rest:rest);remaining=transition_frames;error=0;
    }
    i32 tick(){
        if(remaining){
            value+=step;error+=remainder;
            if(error>=transition_frames){value+=sign;error-=transition_frames;}
            if(!--remaining)value=target;
        }
        return value;
    }
};
struct State {Ramp level,balance,width,mix;bool initialized;};
inline void reset(State&s,Parameters p=defaults()){
    p=clamp(p);s.level.snap(i32(p.level));s.balance.snap(p.balance);
    s.width.snap(i32(p.width));s.mix.snap(p.bypass?0:i32(p.mix));s.initialized=true;
}
inline void process(State&s,const i32*input,i32*output,u32 frames,Parameters p){
    if(!frames)return;
    p=clamp(p);if(!s.initialized)reset(s,p);
    s.level.set(i32(p.level));s.balance.set(p.balance);s.width.set(i32(p.width));
    s.mix.set(p.bypass?0:i32(p.mix));
    for(u32 frame=0;frame<frames;++frame){
        const i32 l=input[2*frame],r=input[2*frame+1];
        const u32 level=u32(s.level.tick()),width=u32(s.width.tick()),mix=u32(s.mix.tick());
        const i32 balance=s.balance.tick(),mid=midpoint(l,r);
        const u32 lg=balance>0?one-u32(balance):one;
        const u32 rg=balance<0?one-u32(-balance):one;
        const i32 wl=gain(gain(blend(mid,l,width),level),lg);
        const i32 wr=gain(gain(blend(mid,r,width),level),rg);
        output[2*frame]=blend(l,wl,mix);output[2*frame+1]=blend(r,wr,mix);
    }
}
}
#endif
