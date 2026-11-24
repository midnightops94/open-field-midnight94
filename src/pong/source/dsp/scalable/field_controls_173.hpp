#pragma once
#include "../source/experiments/squash_pong/pong.hpp"
#include "../rally.hpp"
// Compile-only control mapping for the historical custom-FX convention.
// No native callbacks, ownership, taps, sync, persistence or registration.
namespace pong_field173_candidate {
using namespace mod_fx;
constexpr u32 sample_rate=44100;
constexpr u32 min_court=882; // 20 ms
constexpr u32 largest_ring=262144;
// Initial one-second COURT target; actual native reservation remains unqualified.
constexpr u32 initial_ring_samples=65536;
inline u32 field_unit(i32 word){
 const u32 high=u32(word)>>16;
 return high>32767?0:unit(int16_t(high));
}
inline u32 spin_magnitude(u32 spin){
 const i32 delta=i32(spin)-32768;
 const u32 doubled=u32(delta<0?-delta:delta)*2u;
 return (doubled*43691u)>>17;
}
// Keep the existing Q16 SPIN mapping shared between free and synced time.
// Callers must validate the base and ring capacity before applying it.
inline void spin_delays(u32 base,u32 spin,u32& left,u32& right){
 const u32 offset=u32(q16mul(i32(base),spin_magnitude(spin)));
 left=spin<32768u?base-offset:base+offset;
 right=spin<32768u?base+offset:base-offset;
}
// The maximum displayed COURT derives from available ring capacity, while
// retaining room for a +1/3 SPIN leg. No clipping at extreme SPIN.
inline bool map(const i32* words,u32 ring_size,pong_proto::Params& out){
 if(!words||ring_size<2048||ring_size>largest_ring||(ring_size&(ring_size-1)))return false;
 u32 max_samples=((ring_size-1u)*3u)/4u;
 if(max_samples>sample_rate)max_samples=sample_rate;
 if(max_samples<min_court)return false;
 const u32 court=field_unit(words[0]);
 const u32 spin=field_unit(words[2]),mix=field_unit(words[3]);
 const u32 base=min_court+(((max_samples-min_court)*court)>>16);
 const u32 rally_word=u32(words[1])>>16;
 const u32 count=pong_rally::repeats(rally_word<=32767u?rally_word:0u);
 spin_delays(base,spin,out.delay_l,out.delay_r);
 out.feedback=pong_rally::feedback(count);out.wall=unity;out.mix=mix;
 out.parallel=false;out.duck=0;out.duck_attack=149;out.duck_release=7;
 return true;
}
}
