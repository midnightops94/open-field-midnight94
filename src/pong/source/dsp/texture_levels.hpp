#pragma once

#include "source/effects/modulation_math.hpp"

namespace pong_texture {
using mod_fx::i32;
using mod_fx::u32;

constexpr u32 native_max_feedback=49145u;

inline u32 magnitude(i32 sample){
 return sample<0?0u-u32(sample):u32(sample);
}

// Signed 12-bit phase-independent quantization. Eight magnitude segments
// allocate more steps to the quiet part of the existing Q31 wet range.
// No dither, history bits, new gain stage, or side memory is required.
constexpr unsigned first_shift12=15u;
constexpr u32 first_step12=1u<<first_shift12;
constexpr u32 bias12=256u*first_step12;

inline i32 encode12(i32 sample){
 const u32 absolute=magnitude(sample);
 if(absolute<first_step12)return 0;
 u32 range=absolute+bias12;
 unsigned segment=0;
 if(range>=16u*bias12){range>>=4;segment+=4;}
 if(range>=4u*bias12){range>>=2;segment+=2;}
 if(range>=2u*bias12)++segment;
 const u32 step=first_step12<<segment;
 const u32 base=(bias12<<segment)-bias12;
 const u32 code=256u*segment+((absolute-base+step/2u)>>(first_shift12+segment));
 const u32 limit=sample<0?2048u:2047u;
 const i32 retained=i32(code<limit?code:limit);
 return sample<0?-retained:retained;
}

inline i32 decode12(i32 code){
 if(code==-2048)return INT32_MIN;
 const u32 absolute=magnitude(code);
 const u32 interval=(256u+(absolute&255u))<<(first_shift12+(absolute>>8));
 const i32 sample=i32(interval-bias12);
 return code<0?-sample:sample;
}

// Keep the signed nibble and its three loudest reconstruction values. Give
// quiet input three levels with usable returns. Every level's re-entry
// threshold exceeds its reconstruction multiplied by maximum Rally.
constexpr uint16_t threshold4[]={0,222,478,1000,2900,9000,19560,32768};
constexpr i32 level4[]={
 0,288*65536,600*65536,1280*65536,3800*65536,754248512,1281855104,INT32_MAX,
 0,-288*65536,-600*65536,-1280*65536,-3800*65536,-754248512,-1281855104,INT32_MIN,
};
inline i32 encode4(i32 sample){
 const u32 half=magnitude(sample)>>16;
 unsigned code=0;
 for(unsigned step=4u;step;step>>=1u){
  const unsigned upper=code+step;
  if(half>=threshold4[upper])code=upper;
 }
 return i32(code|((sample<0)?8u:0u));
}

inline i32 decode4(i32 code){
 return level4[u32(code)&15u];
}

inline u32 clean_send_ceiling(u32 feedback){
 constexpr u32 ceiling=49152u;
 const u32 room=mod_fx::unity-feedback;
 return room<ceiling?room:ceiling;
}

// Called only on imminent output overflow. With available less than
// required (at most 2^31), doubling the remainder fits in 32 bits.
inline __attribute__((noinline)) u32 rail_fraction(u32 available,u32 required){
 u32 remainder=available;
 u32 ratio=0;
 for(unsigned i=0;i<12u;++i){
  remainder<<=1;
  ratio<<=1;
  if(remainder>=required){remainder-=required;ratio|=1u;}
 }
 return ratio<<4;
}

inline __attribute__((always_inline)) u32 wet_gain(i32 dry,i32 wet){
 const u32 available=wet<0?u32(dry)-0x80000000u:0x7fffffffu-u32(dry);
 const u32 required=magnitude(wet);
 if(required<=available)return mod_fx::unity;
 return available?rail_fraction(available,required):0u;
}

inline void mix_wet(i32 dry_l,i32 dry_r,i32 wet_l,i32 wet_r,i32& left,i32& right){
 const u32 a=wet_gain(dry_l,wet_l),b=wet_gain(dry_r,wet_r);
 const u32 gain=a<b?a:b;
 if(gain<mod_fx::unity){
  wet_l=mod_fx::q16mul(wet_l,gain);
  wet_r=mod_fx::q16mul(wet_r,gain);
 }
 left=mod_fx::saturated_add(dry_l,wet_l);
 right=mod_fx::saturated_add(dry_r,wet_r);
}
}
