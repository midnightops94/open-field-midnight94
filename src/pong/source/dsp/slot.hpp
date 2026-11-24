#pragma once
#include "slot_packed_pong.hpp"
// Caller must give one slot a single serialized audio owner across the ENTIRE
// block and ensure no overlapping calls, reset, destruction or arena reuse.
// These source contracts are not proven by stock callback entry alone.
namespace pong_slot {
using pong_slot_proto::Params;
struct Work {uint32_t frames=0,reads=0,writes=0,commits=0,clear_bytes=0,commit_index=UINT32_MAX,byte_initializations=0;};
class Slot {
 pong_slot_proto::State dsp_{};uint8_t* arena_=nullptr;size_t ring_=0;uint32_t size_=0;unsigned bits_=0,pending_=0;level_fx::Ramp transition_{};
 static bool valid_bits(unsigned b){return b==12||b==8||b==4;}
 // This is the established 12-bit depth, labelled 25% Politeness.
 static constexpr uint32_t politeness=8192u;
 static bool overlap(const void* p,size_t pn,const void* q,size_t qn){uintptr_t a=reinterpret_cast<uintptr_t>(p),b=reinterpret_cast<uintptr_t>(q),m=~uintptr_t(0);return a>m-pn||b>m-qn||(a<b+qn&&b<a+pn);}
public:
 Slot()=default;Slot(const Slot&)=delete;Slot& operator=(const Slot&)=delete;
 bool init(uint8_t* arena,size_t bytes,uint32_t size,Params p,unsigned initial=12){
  if(arena_||!arena||!valid_bits(initial)||size<2048||size>262144||(size&(size-1))||p.delay_l<1||p.delay_l>=size||p.delay_r<1||p.delay_r>=size)return false;
  size_t ring=0;if(!pong_packed_extent(size,12,&ring)||ring>SIZE_MAX/2||bytes<ring*2||overlap(arena,ring*2,this,sizeof(*this)))return false;
  p.duck=politeness;if(!pong_slot_proto::reset(dsp_,arena,arena+ring,size,ring,initial,p))return false;
  arena_=arena;ring_=ring;size_=size;bits_=pending_=initial;transition_.snap(65536);return true;
 }
 unsigned profile()const{return bits_;}uint32_t valid_samples()const{return dsp_.valid;}
 unsigned read_live_profile()const volatile{return dsp_.valid?bits_:0;}
 bool process_block(unsigned requested,const int32_t* l,const int32_t* r,int32_t* ol,int32_t* or_,uint32_t n,Params p,Work& result){
  if(!arena_||!valid_bits(requested)||!n||n>64||!l||!r||!ol||!or_||p.delay_l<1||p.delay_l>=size_||p.delay_r<1||p.delay_r>=size_)return false;
  if(overlap(&result,sizeof(result),this,sizeof(*this))||overlap(&result,sizeof(result),arena_,ring_*2))return false;
  size_t bytes=size_t(n)*sizeof(int32_t);const void* ptr[]={l,r,ol,or_};
  for(unsigned i=0;i<4;i++)if(overlap(ptr[i],bytes,arena_,ring_*2)||overlap(ptr[i],bytes,this,sizeof(*this))||overlap(ptr[i],bytes,&result,sizeof(result)))return false;
  if(overlap(ol,bytes,or_,bytes))return false;
  if((ol!=l&&overlap(ol,bytes,l,bytes))||(ol!=r&&overlap(ol,bytes,r,bytes))||(or_!=l&&overlap(or_,bytes,l,bytes))||(or_!=r&&overlap(or_,bytes,r,bytes)))return false;
  // A request may reverse an in-flight fade back to the current profile.
  pending_=requested;transition_.set(requested==bits_?65536:0);Work work{};
  if(!transition_.remaining&&transition_.value==65536){
   // Each output is disjoint from, or exactly aliases, each input. The
   // kernel captures the two samples before writing either output.
   p.duck=politeness;
   const uint32_t reads=dsp_.reads,writes=dsp_.writes,initializations=dsp_.byte_initializations;
   if(!pong_slot_proto::process(dsp_,l,r,ol,or_,n,p))return false;
   work.frames=n;
   work.reads=dsp_.reads-reads;
   work.writes=dsp_.writes-writes;
   work.byte_initializations=dsp_.byte_initializations-initializations;
   result=work;return true;
  }
  for(uint32_t i=0;i<n;i++){
   const int32_t dry_l=l[i],dry_r=r[i];
   p.duck=politeness;int32_t xl=0,xr=0;uint32_t oldr=dsp_.reads,oldw=dsp_.writes,oldi=dsp_.byte_initializations;
   if(!pong_slot_proto::process(dsp_,&dry_l,&dry_r,&xl,&xr,1,p))return false;
   work.reads+=dsp_.reads-oldr;work.writes+=dsp_.writes-oldw;work.byte_initializations+=dsp_.byte_initializations-oldi;
   const uint32_t w=uint32_t(transition_.tick());
   // Fade only the return while a ring changes format. The dry sample remains exact.
   ol[i]=dry_l+mod_fx::q16mul(xl-dry_l,w);
   or_[i]=dry_r+mod_fx::q16mul(xr-dry_r,w);
   ++work.frames;
   if(w==0&&bits_!=pending_){
    p.duck=politeness;if(!pong_slot_proto::reset(dsp_,arena_,arena_+ring_,size_,ring_,pending_,p))return false;
    bits_=pending_;transition_.set(65536);work.commit_index=i;++work.commits;
   }
  }
  result=work;return true;
 }
};
}
