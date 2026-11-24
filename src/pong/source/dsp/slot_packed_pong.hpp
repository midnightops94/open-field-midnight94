#pragma once
#include "source/experiments/squash_pong/pong.hpp"
#include "packed/packed_access.hpp"
#include "packed/codec.hpp"
#include "packed/tables.hpp"
#include "texture_levels.hpp"
#include <stdint.h>
// Caller supplies two disjoint equal power-of-two int16 buffers per instance.
// Integer delay lengths, 64-frame dual-read crossfades; a return to the
// original pair reverses the fade, other mid-fade changes queue.
namespace pong_slot_proto {
using namespace mod_fx;
using Params=pong_proto::Params;
struct State {uint8_t *line_l=nullptr,*line_r=nullptr;u32 size=0,write=0,valid=0,reads=0,writes=0,byte_initializations=0;unsigned bits=0;size_t bytes=0;i32 low_l=0,low_r=0;level_fx::Ramp input{},duck{};u32 delay_l=1,delay_r=1,next_l=1,next_r=1,fade=0,detector=0;
 level_fx::Ramp wall_ramp{},wall_loss_ramp{};bool wall_ramps_initialized=false;
};
// Approximate -60 dB after n feedback passes; perceptible repeats vary.
static inline u32 feedback_for_bounces(u32 n){
 static const uint16_t table[16]={66,2072,6554,11654,16462,20724,24429,27636,30419,32846,34975,36854,38522,40012,41350,42558};
 if(n<1)n=1;
 if(n>16)n=16;
 return table[n-1];
}
inline u32 valid_delay(u32 d,u32 size){return d<1?1:d>=size?size-1:d;}
// Owner supplies exclusive byte rings and must serialize reset against readers.
inline bool reset(State& s,uint8_t* l,uint8_t* r,u32 size,size_t bytes,unsigned bits,Params p){
 size_t need=0,reserved=0;if(size<2||size>262144||(size&(size-1))||!l||!r||p.delay_l<1||p.delay_l>=size||p.delay_r<1||p.delay_r>=size||
 !pong_packed_extent(size,bits,&need)||!pong_packed_extent(size,12,&reserved)||bytes<reserved)return false;
 const uintptr_t a=reinterpret_cast<uintptr_t>(l),b=reinterpret_cast<uintptr_t>(r),max=~uintptr_t(0);
 if(a>max-bytes||b>max-bytes||!(a+bytes<=b||b+bytes<=a))return false;
 s=State{};s.line_l=l;s.line_r=r;s.size=size;s.bits=bits;s.bytes=bytes;
 s.delay_l=s.next_l=valid_delay(p.delay_l,size);s.delay_r=s.next_r=valid_delay(p.delay_r,size);
 // No clear: valid=0 prevents reading any history from an earlier format.
 s.input.snap(i32(p.mix>unity?unity:p.mix));s.duck.snap(i32(p.duck>unity?unity:p.duck));return true;
}
#include "history_decode12.inc"
// The caller masks ring indices after validating the power-of-two extent.
template<unsigned Bits> inline i32 packed_get(State& s,uint8_t* line,u32 i){
 ++s.reads;
 if (Bits==12u){
  const u32 j=(i>>1)*3;
  const u32 code=(i&1u)?(u32(line[j+1])>>4)|(u32(line[j+2])<<4)
                       :u32(line[j])|((u32(line[j+1])&15u)<<8);
  return history_decode12[code];
 }
 if (Bits==4u) return pong_texture::level4[(line[i>>1]>>((i&1u)*4))&15];
 return pong_codec_tables::decode8[line[i]];
}
// Truncate toward zero. Arithmetic right shift floors negative fractions and
// sustains a -1 ring sample indefinitely in any nonzero feedback loop.
inline int16_t store(i32 x){
 const bool negative=x<0;
 const u32 magnitude=negative?u32(0)-u32(x):u32(x);
 const i32 whole=i32(magnitude>>16);
 return int16_t(negative?-whole:whole);
}
template<unsigned Bits> inline void packed_put(State& s,uint8_t* line,u32 i,i32 sample){
 ++s.writes;
 // The index is the already range-checked write cursor. Initialize nibble
 // pairs before accessing their neighbor for the first time.
 if(Bits==12u){
  const u32 code=u32(pong_texture::encode12(sample)) & 4095;const u32 j=(i>>1)*3;
  if(i&1u){line[j+1]=uint8_t((line[j+1]&15)|(code<<4));line[j+2]=uint8_t(code>>4);}
  else {
   const bool new_pair=s.valid<s.size;
   const u32 neighbor=new_pair?0u:(line[j+1]&240u);
   line[j]=uint8_t(code);line[j+1]=uint8_t(neighbor|(code>>8));s.byte_initializations+=unsigned(new_pair);
  }
 }else if(Bits==4u){
  const u32 code=u32(pong_texture::encode4(sample));const u32 j=i>>1;
  if(i&1u)line[j]=uint8_t((line[j]&15u)|(code<<4));
  else{const bool new_pair=s.valid<s.size;const u32 neighbor=new_pair?0u:(line[j]&240u);
       line[j]=uint8_t(neighbor|code);s.byte_initializations+=unsigned(new_pair);}
 }else{
  i32 code=0;pong_encode_halfword(store(sample),8,&code);line[i]=uint8_t(code);
 }

}
inline i32 load(int16_t x){return i32(x)*65536;}
template<unsigned Bits> inline bool process_texture(State& s,const i32* l,const i32* r,i32* ol,i32* or_,u32 n,Params p){
 if(!n)return true;
 if(!s.line_l||!s.line_r||!s.size||!l||!r||!ol||!or_)return false;
 if(p.delay_l<1||p.delay_l>=s.size||p.delay_r<1||p.delay_r>=s.size)return false;
 // The owner serializes the entire block. Qualify the same packed extent
 // once before the loop; View still checks the per-access index and code.
 size_t required=0;
 if(!pong_packed_extent(s.size,Bits,&required)||s.bytes<required||s.write>=s.size)return false;
 if(p.feedback>65000)p.feedback=65000;
 if(Bits!=8u&&p.feedback>pong_texture::native_max_feedback)
  p.feedback=pong_texture::native_max_feedback;
 if(p.wall>unity)p.wall=unity;
 if(p.wall_loss>unity)p.wall_loss=unity;
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
 // Ramp only the two Wall-owned factors; Rally and Players remain immediate.
 if(!s.wall_ramps_initialized){
  s.wall_ramp.snap(i32(p.wall));s.wall_loss_ramp.snap(i32(p.wall_loss));
  s.wall_ramps_initialized=true;
 }
 s.wall_ramp.set(i32(p.wall));s.wall_loss_ramp.set(i32(p.wall_loss));
 // The fourth packed-path control is Input. The shared legacy Params field
 // is named mix because the separate unpacked prototype still uses that mix.
 s.input.set(i32(p.mix));s.duck.set(i32(p.duck));const u32 mask=s.size-1;
 // Clean PCM can use quiet codes at default Rally without risking a clip at
 // higher feedback. Preserve the existing eight- and four-bit send scales.
 const u32 pcm12_send=pong_texture::clean_send_ceiling(p.feedback);
 for(u32 i=0;i<n;i++){
  const i32 a=l[i],b=r[i];
  if(!s.fade&&(s.delay_l!=p.delay_l||s.delay_r!=p.delay_r)){
   s.next_l=p.delay_l;s.next_r=p.delay_r;s.fade=64;
  }
  i32 dl=(s.delay_l<=s.valid?packed_get<Bits>(s,s.line_l,(s.write-s.delay_l)&mask):0);
  i32 dr=(s.delay_r<=s.valid?packed_get<Bits>(s,s.line_r,(s.write-s.delay_r)&mask):0);
  if(s.fade){
   const u32 amount=(64-s.fade)*1024u;
   dl=difference_mix(dl,(s.next_l<=s.valid?packed_get<Bits>(s,s.line_l,(s.write-s.next_l)&mask):0),amount);
   dr=difference_mix(dr,(s.next_r<=s.valid?packed_get<Bits>(s,s.line_r,(s.write-s.next_r)&mask):0),amount);
   if(!--s.fade){s.delay_l=s.next_l;s.delay_r=s.next_r;}
  }
  const u32 wall=u32(s.wall_ramp.tick());
  const u32 feedback=u32(q16mul(i32(p.feedback),u32(s.wall_loss_ramp.tick())));
  s.low_l=difference_mix(s.low_l,dl,wall);s.low_r=difference_mix(s.low_r,dr,wall);
  // Mid/side injection keeps mono as alternating echoes without discarding
  // opposite-polarity stereo. The wet spatial image is deliberately encoded.
  const i32 mid=level_fx::midpoint(a,b);
  const i32 side=level_fx::half_floor(a)-level_fx::half_floor(b);
  // Apply Input to the new signal and saturate the sum with existing feedback
  // before encoding. Closing Input has no effect on the feedback itself.
  const u32 input=u32(s.input.tick());
  const u32 send=Bits==12u?u32(q16mul(i32(input),pcm12_send)):Bits==8u?input>>2:input;
  packed_put<Bits>(s,s.line_l,s.write,saturated_add(q16mul(p.parallel?a:mid,send),q16mul(p.parallel?s.low_l:s.low_r,feedback)));
  packed_put<Bits>(s,s.line_r,s.write,saturated_add(q16mul(p.parallel?b:side,send),q16mul(p.parallel?s.low_r:s.low_l,feedback)));
  const u32 ma=a<0?u32(0)-u32(a):u32(a),mb=b<0?u32(0)-u32(b):u32(b);
  u32 peak=ma>mb?ma:mb;
  if(peak>0x7fffffffu)peak=0x7fffffffu;
  const i32 delta=i32(peak)-i32(s.detector);
  i32 step=q16mul(delta,delta>0?p.duck_attack:p.duck_release);
  if(!step&&delta>0&&p.duck_attack)step=1;
  s.detector=u32(i32(s.detector)+step);
  const u32 duck=u32(s.duck.tick());
  // Keep Politeness as the only ordinary ducking step. The shared stereo
  // output guard intervenes only if dry plus return would cross a rail.
#ifdef PONG_ZERO_DUCK_FAST_PATH
  // Keep detector and ramp live. At zero duck the wet gain is exactly unity,
  // so the two Q16 multiplies are identities and can be skipped.
  i32 wet_l=dl,wet_r=dr;
  if(duck){const u32 atten=unity-u32(q16mul(i32(s.detector>>15),duck));
   wet_l=q16mul(dl,atten);wet_r=q16mul(dr,atten);}
  pong_texture::mix_wet(a,b,wet_l,wet_r,ol[i],or_[i]);
#else
  const u32 atten=unity-u32(q16mul(i32(s.detector>>15),duck));
  pong_texture::mix_wet(a,b,q16mul(dl,atten),q16mul(dr,atten),ol[i],or_[i]);
#endif
  s.write=(s.write+1)&mask;if(s.valid<s.size)++s.valid;
 }
 return true;
}
// The texture is constant during this call. Dispatch outside the per-frame packed accesses.
inline bool process(State& s,const i32* l,const i32* r,i32* ol,i32* or_,u32 n,Params p) {
 switch(s.bits){
 case 12:return process_texture<12>(s,l,r,ol,or_,n,p);
 case 8:return process_texture<8>(s,l,r,ol,or_,n,p);
 case 4:return process_texture<4>(s,l,r,ol,or_,n,p);
 default:return false;
 }
}
}
