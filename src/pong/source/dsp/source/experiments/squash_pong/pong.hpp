#pragma once
#include "../../effects/modulation_math.hpp"
#include <stdint.h>
// Caller supplies two disjoint equal power-of-two int16 buffers per instance.
// Integer delay lengths, 64-frame dual-read crossfades; a return to the
// original pair reverses the fade, other mid-fade changes queue.
namespace pong_proto {
using namespace mod_fx;
struct Params {u32 delay_l,delay_r,feedback,wall,mix;bool parallel;u32 duck=0,duck_attack=149,duck_release=7;u32 wall_loss=unity;};
struct State {int16_t *line_l=nullptr,*line_r=nullptr;u32 size=0,write=0;i32 low_l=0,low_r=0;level_fx::Ramp wet{},duck{};u32 delay_l=1,delay_r=1,next_l=1,next_r=1,fade=0,detector=0;
#ifdef PONG_SMOOTH_TAP_TRANSITIONS
 level_fx::Ramp wall_ramp{},feedback_ramp{},players_ramp{};bool tap_ramps_initialized=false;
#endif
};
// Approximate -60 dB after n feedback passes; perceptible repeats vary.
static inline u32 feedback_for_bounces(u32 n){
 static const uint16_t table[16]={66,2072,6554,11654,16462,20724,24429,27636,30419,32846,34975,36854,38522,40012,41350,42558};
 if(n<1)n=1;
 if(n>16)n=16;
 return table[n-1];
}
inline u32 valid_delay(u32 d,u32 size){return d<1?1:d>=size?size-1:d;}
// Validate byte ranges before clearing either caller-owned history. This
// cannot establish allocation extent, lifetime, ownership or quiescence.
inline bool disjoint_lines(const int16_t* l,const int16_t* r,u32 size){
 const uintptr_t a=reinterpret_cast<uintptr_t>(l),b=reinterpret_cast<uintptr_t>(r);
 const uintptr_t max=~uintptr_t(0);
 if(!a||!b||(a%alignof(int16_t))||(b%alignof(int16_t))||size>max/sizeof(int16_t))return false;
 const uintptr_t bytes=uintptr_t(size)*sizeof(int16_t);
 if(a>max-bytes||b>max-bytes)return false;
 return a+bytes<=b||b+bytes<=a;
}
inline bool reset(State& s,int16_t* l,int16_t* r,u32 size,Params p){
 if(size<2||(size&(size-1))||!disjoint_lines(l,r,size))return false;
 s=State{};s.line_l=l;s.line_r=r;s.size=size;
 s.delay_l=s.next_l=valid_delay(p.delay_l,size);s.delay_r=s.next_r=valid_delay(p.delay_r,size);
 for(u32 i=0;i<size;i++){l[i]=0;r[i]=0;}
 s.wet.snap(i32(p.mix>unity?unity:p.mix));s.duck.snap(i32(p.duck>unity?unity:p.duck));return true;
}
// Truncate toward zero. Arithmetic right shift floors negative fractions and
// sustains a -1 ring sample indefinitely in any nonzero feedback loop.
inline int16_t store(i32 x){
 const bool negative=x<0;
 const u32 magnitude=negative?u32(0)-u32(x):u32(x);
 const i32 whole=i32(magnitude>>16);
 return int16_t(negative?-whole:whole);
}
inline i32 load(int16_t x){return i32(x)*65536;}
inline bool process(State& s,const i32* l,const i32* r,i32* ol,i32* or_,u32 n,Params p){
 if(!n)return true;
 if(!s.line_l||!s.line_r||!s.size||!l||!r||!ol||!or_)return false;
 p.delay_l=valid_delay(p.delay_l,s.size);p.delay_r=valid_delay(p.delay_r,s.size);
 if(p.feedback>65000)p.feedback=65000;
 if(p.wall>unity)p.wall=unity;
 if(p.mix>unity)p.mix=unity;
 if(p.duck>unity)p.duck=unity;
 if(p.duck_attack>unity)p.duck_attack=unity;
 if(p.duck_release>unity)p.duck_release=unity;
 // Returning both controls to the original tap pair should not visit the
 // abandoned pair first. Swapping endpoints and complementing the remaining
 // samples continues from the current blend without extra ring reads or state.
 if(s.fade&&p.delay_l==s.delay_l&&p.delay_r==s.delay_r){
  u32 t=s.delay_l;s.delay_l=s.next_l;s.next_l=t;
  t=s.delay_r;s.delay_r=s.next_r;s.next_r=t;
  s.fade=64-s.fade;
 }
#ifdef PONG_SMOOTH_TAP_TRANSITIONS
 // First callback inherits actual parameters, not a guessed preset. Later
 // changes use the existing 64-frame ramp; no allocation or ring reset.
 if(!s.tap_ramps_initialized){
  s.wall_ramp.snap(i32(p.wall));s.feedback_ramp.snap(i32(p.feedback));
  s.players_ramp.snap(p.parallel?i32(unity):0);s.tap_ramps_initialized=true;
 }
 s.wall_ramp.set(i32(p.wall));s.feedback_ramp.set(i32(p.feedback));
 s.players_ramp.set(p.parallel?i32(unity):0);
#endif
 s.wet.set(i32(p.mix));s.duck.set(i32(p.duck));const u32 mask=s.size-1;
 for(u32 i=0;i<n;i++){
  const i32 a=l[i],b=r[i];
  if(!s.fade&&(s.delay_l!=p.delay_l||s.delay_r!=p.delay_r)){
   s.next_l=p.delay_l;s.next_r=p.delay_r;s.fade=64;
  }
  i32 dl=load(s.line_l[(s.write-s.delay_l)&mask]);
  i32 dr=load(s.line_r[(s.write-s.delay_r)&mask]);
  if(s.fade){
   const u32 amount=(64-s.fade)*1024u;
   dl=difference_mix(dl,load(s.line_l[(s.write-s.next_l)&mask]),amount);
   dr=difference_mix(dr,load(s.line_r[(s.write-s.next_r)&mask]),amount);
   if(!--s.fade){s.delay_l=s.next_l;s.delay_r=s.next_r;}
  }
#ifdef PONG_SMOOTH_TAP_TRANSITIONS
  const u32 wall=u32(s.wall_ramp.tick()),feedback=u32(s.feedback_ramp.tick()),players=u32(s.players_ramp.tick());
#else
  const u32 wall=p.wall,feedback=p.feedback;
#endif
  s.low_l=difference_mix(s.low_l,dl,wall);s.low_r=difference_mix(s.low_r,dr,wall);
  // Mid/side injection keeps mono as alternating echoes without discarding
  // opposite-polarity stereo. The wet spatial image is deliberately encoded.
  const i32 mid=level_fx::midpoint(a,b);
  const i32 side=level_fx::half_floor(a)-level_fx::half_floor(b);
#ifdef PONG_SMOOTH_TAP_TRANSITIONS
  const i32 inject_l=difference_mix(mid,a,players),inject_r=difference_mix(side,b,players);
  const i32 fb_l=difference_mix(s.low_r,s.low_l,players),fb_r=difference_mix(s.low_l,s.low_r,players);
  s.line_l[s.write]=store(saturated_add(inject_l,q16mul(fb_l,feedback)));
  s.line_r[s.write]=store(saturated_add(inject_r,q16mul(fb_r,feedback)));
#else
  s.line_l[s.write]=store(saturated_add(p.parallel?a:mid,q16mul(p.parallel?s.low_l:s.low_r,feedback)));
  s.line_r[s.write]=store(saturated_add(p.parallel?b:side,q16mul(p.parallel?s.low_r:s.low_l,feedback)));
#endif
  const u32 ma=a<0?u32(0)-u32(a):u32(a),mb=b<0?u32(0)-u32(b):u32(b);
  u32 peak=ma>mb?ma:mb;
  if(peak>0x7fffffffu)peak=0x7fffffffu;
  const i32 delta=i32(peak)-i32(s.detector);
  i32 step=q16mul(delta,delta>0?p.duck_attack:p.duck_release);
  if(!step&&delta>0&&p.duck_attack)step=1;
  s.detector=u32(i32(s.detector)+step);
  const u32 duck=u32(s.duck.tick()),mix=u32(s.wet.tick());
#ifdef PONG_ZERO_DUCK_FAST_PATH
  // Keep detector and ramp live. At zero duck the wet gain is exactly unity,
  // so the two Q16 multiplies are identities and can be skipped.
  i32 wet_l=dl,wet_r=dr;
  if(duck){const u32 atten=unity-u32(q16mul(i32(s.detector>>15),duck));
   wet_l=q16mul(dl,atten);wet_r=q16mul(dr,atten);}
  ol[i]=difference_mix(a,wet_l,mix);or_[i]=difference_mix(b,wet_r,mix);
#else
  const u32 atten=unity-u32(q16mul(i32(s.detector>>15),duck));
  ol[i]=difference_mix(a,q16mul(dl,atten),mix);or_[i]=difference_mix(b,q16mul(dr,atten),mix);
#endif
  s.write=(s.write+1)&mask;
 }
 return true;
}
}
