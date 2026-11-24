#pragma once
#include <stdint.h>
#include <stddef.h>
#include "recut.hpp"
// Standalone fixed-point V2 translation. No Field addresses, allocation or globals.
namespace splice {
struct Controls {
 uint32_t window,grain,speed,wu,gu,scatter,mix; // frames, frames, Q16 speed; remaining Q30
 uint8_t direction; bool hold,bypass;
};
struct Profile {uint32_t frames;uint8_t bits,decimation;};
constexpr Profile full{32768,16,1},packed{32768,8,1},half{16384,16,2},compact{16384,8,2};
inline size_t bytes(Profile p){return size_t(p.frames)*2*(p.bits/8);}
struct Head {int64_t logical;uint32_t pos,speed,left,length,age,fade,invfade,active;int32_t dir;};
struct Snapshot {uint32_t launches,max_heads,normalizations,refusals,bounds_faults,recut_starts,recut_repeats;};
class Engine {
 public:
 Engine()=default;Engine(const Engine&)=delete;Engine&operator=(const Engine&)=delete;
 static constexpr uint32_t one=1u<<30;
 bool bind(void*memory,size_t n,Profile p,uint32_t rate=44100);
 void reset();void controls(const Controls&c);
 // Supply once at the start of each audio callback with at most 64 frames.
 void recut_block(Recut mode,const ClockBlock&clock,unsigned frames);
 void tick(int32_t l,int32_t r,int32_t&ol,int32_t&orr);
 Snapshot snapshot()const{return stats_;}
 size_t buffer_bytes()const{return bytes(full);}unsigned heads()const{return count_;}
 private:
 uint8_t*ring_=nullptr;Controls c_{};
 uint32_t write_=0,valid_=0,rng_=0x51a1ce,mix_=0,count_=0,repeats_=0;
 uint64_t written_=0,saved_at_=0;uint32_t saved_age_=0,saved_speed_=0;bool saved_reverse_=false,saved_=false,was_hold_=false,was_bypass_=false;
 // Grain launch constraints depend on the current knob block, never on a sample.
 struct Launch { uint32_t speed,length,age; };
 Launch launch_[3]{};
 uint32_t random_threshold_=0;
 Head h_[2]{};Snapshot stats_{};
 struct Cut {
  uint64_t oldest;
  uint32_t length,speed,fade,active;
  bool reverse;
 };
 bool start_cut(const Cut&cut,uint32_t fade);
 uint32_t random();
 void launch(uint32_t fade=0);int32_t read(unsigned ch,uint32_t pos)const;void store(unsigned ch,int32_t x);
};
}
