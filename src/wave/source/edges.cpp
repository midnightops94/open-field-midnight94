#include "core/ppg_native_oscillator.hpp"
#include "core/ppg_word_math.hpp"
#include "core/ppg_duty.hpp"
#include "tables.hpp"

namespace edge_reader {
using namespace ppg_reader;
static inline int lerp(int a,int b,unsigned w) {
    return a+(((b-a)*static_cast<int>(w)+16384)>>15);
}
static inline int cell(unsigned packed,unsigned weight) {
    const int left=static_cast<short>(packed);
    #ifdef __bfin__
    int product;
    asm("%0 = %1.H * %2.L (IS);" : "=d"(product) : "d"(packed), "d"(weight));
#else
    const int product=(static_cast<int>(packed)>>16)*static_cast<int>(weight);
#endif
    return left+((product+16384)>>15);
}
static inline int q30(int s) {
    return s*32769+((s+16384+(s>>31))>>15);
}
static inline int frame(const unsigned* a,unsigned weight,unsigned morph) {
    const int first=cell(a[0],weight);
    return morph?lerp(first,cell(a[1],weight),morph):first;
}
static inline int bank(const unsigned* a,unsigned weight,unsigned morph,unsigned mip) {
    const int lower=frame(a,weight,morph);
    return q30(mip?lerp(lower,frame(a+16,weight,morph),mip):lower);
}
static inline int read(const unsigned* table,const BankFrame* pair,unsigned pitch,unsigned position,unsigned phase,unsigned grain) {
    const unsigned* base=table+(phase>>25)*96+((pitch>>16)<<4)+(position>>16);
    const unsigned weight=(((phase>>9)&65535u)*(65536-grain))>>17;
    const unsigned morph=(position&65535u)>>1,mip=(pitch&65535u)>>1;
    const int next=bank(base+pair->bank*12288u,weight,morph,mip);
    if (pair->mix_q16==65536) return next;
    return interpolate_wave(bank(base+pair->previous*12288u,weight,morph,mip),next,pair->mix_q16);
}
static inline int step(const unsigned* table,Voice* voice,const BankFrame* pair,const OscillatorFrame* control,const PulseDuty* duty) {
    const auto& p=*control;
    int sample=read(table,pair,p.mip_q16,p.position_q16,map_duty(voice->phase,*duty),p.grain_q16);
    if(p.twin_mix_q16) {
        const int twin=read(table,pair,p.twin_mip_q16,p.twin_position_q16,map_duty(voice->twin_phase,*duty),p.grain_q16);
        sample=interpolate_wave(sample,twin,p.twin_mix_q16);
    }
    const int quantized=(sample+(1<<18)) & ~((1<<19)-1);
    voice->phase+=p.step_q32;
    voice->twin_phase+=p.twin_step_q32;
    return interpolate_wave(sample,quantized,22938);
}
}
extern "C" int ppg_v2_read_fast_q30(unsigned bank,unsigned pitch,unsigned pos,unsigned phase,unsigned grain) {
    ppg_reader::BankFrame active{bank,bank,65536};
    const auto* table=wave_tables_ready();
    return table ? edge_reader::read(table,&active,pitch,pos,phase,grain) : 0;
}
#ifndef PPG_PACK_ENTRY_ONLY
extern "C" ppg_reader::i32 ppg_v2_read_banked_q30(const ppg_reader::BankFrame* bank,ppg_reader::u32 pitch,ppg_reader::u32 pos,ppg_reader::u32 phase,ppg_reader::u32 grain) {
    const auto* table=wave_tables_ready();
    return table ? edge_reader::read(table,bank,pitch,pos,phase,grain) : 0;
}
#endif
