#pragma once
#include <stdint.h>
#include "source/effects/modulation_math.hpp"
// Owned input-snapshot arithmetic only. Not a native getter or tempo validator.
namespace pong_sync_word {
enum class Division : uint32_t { Sixteenth, Eighth, Quarter, Half, SixteenthTriplet, EighthTriplet, None };
struct Result {uint32_t division,base,left,right,worst_tap,minimum_power_of_two,v71_guard_ring,bytes_per_ring_12,two_slot_reserved_12,two_slot_stored_8,two_slot_stored_4;};
enum class Status:uint32_t {Ready,Invalid,UnsupportedCourtBoundary,InsufficientSlotCapacity};
inline Division division(uint32_t court_q15){
 if(court_q15>32767u)return Division::None;
 // Shortest to longest, with the factory Court value still on an eighth.
 if(court_q15<4096u)return Division::SixteenthTriplet;
 if(court_q15<8192u)return Division::Sixteenth;
 if(court_q15<11264u)return Division::EighthTriplet;
 if(court_q15<16384u)return Division::Eighth;
 if(court_q15<24576u)return Division::Quarter;
 return Division::Half;
}
inline bool convert(uint32_t timing_word,uint32_t court_q15,uint32_t spin_q15,Result& out){
 if(court_q15>32767u||spin_q15>32767u||!timing_word)return false;
 const auto setting=division(court_q15);
 const unsigned step=static_cast<unsigned>(setting);
 const unsigned shift=step<4u?16u-step:19u-step;
 const uint32_t rounded=(timing_word>>shift)+((timing_word>>(shift-1u))&1u);
 // The native word is the quarter-note period in units of 1/16384 sample.
 // A triplet period is round(word / (3 * 2^shift)); avoid overflowing the word.
 const uint32_t base=step<4u?rounded:mod_fx::divide_unsigned(rounded+1u,3u);
 if(!base)return false;
 const uint32_t spin=spin_q15==32767u?65536u:spin_q15*2u;
 const int32_t delta=int32_t(spin)-32768;
 const uint32_t twice=uint32_t(delta<0?-delta:delta)*2u;
 const uint32_t gain=(twice*43691u)>>17;
 const uint32_t offset=(base>>16)*gain+(((base&65535u)*gain)>>16);
 const uint32_t left=delta<0?base-offset:base+offset;
 const uint32_t right=delta<0?base+offset:base-offset;
 const uint32_t maxgain=21845u;
 const uint32_t worst=base+(base>>16)*maxgain+(((base&65535u)*maxgain)>>16);
 uint32_t ring=2;while(ring<=worst){if(ring>0x40000000u)return false;ring<<=1;}
 // Preserve the v71 full-SPIN guard, which is slightly more conservative
 // at integer boundaries than the exact current maximum tap calculation.
 uint32_t guarded=2;while(base>((guarded-1u)*3u)/4u)guarded<<=1;
 const uint32_t ringbytes=(guarded*3u)/2u;
 out={step,base,left,right,worst,ring,guarded,ringbytes,ringbytes*4u,guarded*4u,guarded*2u};return true;
}
inline Status prepare_v71_slot(uint32_t timing_word,uint32_t court_q15,uint32_t spin_q15,uint32_t actual_ring,Result& out){
 Result proposed{};if(!convert(timing_word,court_q15,spin_q15,proposed))return Status::Invalid;
 // Keep the slowest division safe at both Spin extremes in the existing ring.
 if(actual_ring<2048u||actual_ring>262144u||(actual_ring&(actual_ring-1u))||proposed.v71_guard_ring>actual_ring)return Status::InsufficientSlotCapacity;
 out=proposed;return Status::Ready;
}
}
