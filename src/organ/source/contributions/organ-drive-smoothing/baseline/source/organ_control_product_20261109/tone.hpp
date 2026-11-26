#pragma once
#include "candidate_fixed.hpp"

// Constant-pitch Open-Field block adapter for the approved dry Organ renderer.
// The 256-word output is used as temporary storage before stereo duplication.
namespace organ_fast {
using organ_fixed::control;
// BF707 has a full 32-by-32 product in one instruction. Retain both
// halves so each Q15 product rounds towards negative infinity exactly.
__attribute__((always_inline)) inline int product_q15(int a,int b){
#if defined(__bfin__) || defined(__BFIN__)
    unsigned lo,hi;
    asm("(R1:0) = %2 * %3 (IS,NS); %0 = ALIGN16(R1,R0);" : "=q0"(lo),"=q1"(hi) : "d"(a),"d"(b*2) : "ASTAT");
    return int(lo);
#else
    return organ_fixed::mul(a,b);
#endif
}
using organ_fixed::one;

inline const int16_t* row_at(const int16_t* rows, unsigned row) {
    return rows + (row << 9);
}

struct Group {
    int weight, target;
    const int16_t* a;
    const int16_t* b;
    int fraction;
};

struct Voice {
    Group groups[18];
    unsigned phase, vibrato, seed;
    unsigned last_step, ready_tables;
    unsigned last_lower, last_upper, last_character;
    bool registered;
    int click, percussion, low, high, grit_smooth, perc_smooth, vibe_smooth;
    int harmonic, harmonic_target;
    unsigned age;
};

inline void initialize(Voice& v) {
    for(auto& g:v.groups){g.weight=0;g.target=0;}
    v.phase=0;v.vibrato=0;v.last_step=0;v.ready_tables=0;v.registered=false;
    v.click=0;v.percussion=0;v.low=0;v.high=0;v.age=0;
    v.harmonic=0;v.harmonic_target=0;v.vibe_smooth=0;
    const organ::Controls defaults{};
    v.grit_smooth=control(defaults.value[organ::GRIT])*256;
    v.perc_smooth=control(defaults.value[organ::PERC])*256;
    v.seed=1;
}
inline void start(Voice& v) {
    for(auto& g:v.groups) { g.weight=0; g.target=0; }
    v.phase=0; v.registered=false; v.seed=1;
    v.click=one; v.percussion=one; v.low=one; v.high=one; v.age=0;
    v.harmonic=0; v.harmonic_target=0;
}
inline void registration(Voice& v,unsigned step,const organ::Controls& c,const int16_t* rows) {
    const auto* k=c.value;
    const unsigned model=k[organ::TYPE];
    const unsigned lower=(unsigned(k[1])<<16)|k[0];
    const unsigned upper=(model<<16)|k[2];
    const unsigned character_word=k[organ::VIBE];
    if(v.registered && v.last_step==step && v.last_lower==lower &&
       v.last_upper==upper && v.last_character==character_word)return;
    v.last_lower=lower;v.last_upper=upper;v.last_character=character_word;
    if(!v.registered || v.last_step!=step)v.ready_tables=0;
    v.last_step=step; v.registered=true;
    const int character=control(k[organ::VIBE]);
    const unsigned low=(k[0]*unsigned(37683-product_q15(9830,character)))>>15;
    const unsigned high=(k[2]*unsigned(18022+product_q15(24576,character)))>>15;
    const int gain_control[]={control(low>32767?32767:low),control(k[1]),control(high>32767?32767:high)};
    int gains[3];
    for(unsigned i=0;i<3;++i) gains[i]=product_q15(gain_control[i],gain_control[i]);
    for(unsigned i=0;i<18;++i) {
        auto& g=v.groups[i];
        const unsigned first=model*3;
        g.target=i>=first && i<first+3?gains[i-first]*256:0;
        if((!g.target && !g.weight) || (v.ready_tables & (1u<<i)))continue;
        v.ready_tables|=1u<<i;
        const auto* knots=organ_tables::knots[i];
        const unsigned count=organ_tables::counts[i];
        unsigned j=0;
        while(j+1<count && step>=knots[j+1].increment)++j;
        const auto& lower=knots[j];
        g.a=row_at(rows,lower.wave);
        g.b=g.a;
        unsigned fraction=0;
        if(j+1<count){
            g.b=row_at(rows,knots[j+1].wave);
            fraction=(((step-lower.increment)>>lower.shift)*lower.multiplier)>>16;
        }
        if(fraction>=32768){g.a=g.b;fraction=0;}
        g.fraction=fraction;
    }
    const unsigned freq=step>0x7851eb85u/2?0x7851eb85u:step*2;
    const int fade=freq<=0x6b851eb8u?32768:freq>=0x7851eb85u?0:int(((0x7851eb85u-freq)*20u)>>17);
    int target=0;
    for(unsigned j=0;j<3;++j)target+=product_q15(organ_fixed::registration[model][j][2],gains[j]);
    v.harmonic_target=product_q15(target,fade);
}

inline int lookup(const int16_t* row,unsigned index,int fraction,int sign) {
    const int left=row[index],right=row[index+1];
    // Adjacent entries differ by at most 1,039 in the authored 205 tables.
    const int wave=left+((int(int16_t(right-left))*int(int16_t(fraction)))>>15);
    return (wave^sign)-sign;
}

inline int dual_wave(const int16_t* a,const int16_t* b,unsigned index,int f,int sign,int band){
#if defined(__bfin__) || defined(__BFIN__)
    const int a0=a[index],b0=b[index],a1=a[index+1],b1=b[index+1];
    int bases,adjacent;
    asm("%0 = PACK(%1.L,%2.L);" : "=d"(bases) : "d"(b0),"d"(a0));
    asm("%0 = PACK(%1.L,%2.L);" : "=d"(adjacent) : "d"(b1),"d"(a1));
    asm("%0 = %0 -|- %1; %0.H = %0.H * %2.L, %0.L = %0.L * %2.L (T); %0 = %0 +|+ %1;"
        : "+&d"(adjacent) : "d"(bases),"d"(f) : "ASTAT");
    const int signed_pair=adjacent^sign;
    const int first=int(int16_t(signed_pair));
    const int second=signed_pair>>16;
    return (first-sign)*256+(((second-first)*band)>>7);
#else
    const int first=lookup(a,index,f,sign);
    return first*256+(((lookup(b,index,f,sign)-first)*band)>>7);
#endif
}

template<bool dual,bool fading> inline void group(Voice& __restrict v,unsigned number,int* __restrict scratch) {
    auto& g=v.groups[number];
    const int target=g.target;
    int weight=g.weight;
    const auto* a=g.a;
    const auto* b=g.b;
    const int band=g.fraction;
    for(unsigned i=0;i<64;++i){
        if(fading) {
            weight=product_q15(weight,32670);
        } else {
            const int d=target-weight;
#if defined(__bfin__) || defined(__BFIN__)
            int change=d;
            asm("%0 *= %1;" : "+d"(change) : "d"(98) : "ASTAT");
            weight+=(change>>15)+int(unsigned(d-1)<334u);
#else
            weight+=((d*98)>>15)+int(unsigned(d-1)<334u);
#endif
        }
        const unsigned index=unsigned(scratch[64+i]);
        const int fraction=scratch[128+i],sign=fraction>>16;
        if(dual){
            scratch[i]+=product_q15(dual_wave(a,b,index,fraction,sign,band),weight>>8);
        } else {
            scratch[i]+=(lookup(a,index,fraction,sign)*(weight>>8))>>7;
        }
    }
    g.weight=weight;
}

template<unsigned amount> inline int decay_positive(int x) {
    // For x>=1 the decrement is at most x. Zero must stay zero.
    return x ? x-(amount<=255?int((x*amount)>>15):product_q15(x,amount))-1 : 0;
}
inline int ease33(int x,int target){
    const int d=target-x;
    return x+((d*33)>>15)+int(unsigned(d-1)<992u);
}
inline bool render(Voice& __restrict v,int* __restrict out,unsigned step,const organ::Controls& __restrict c,
                   const int16_t* rows) {
    if(!out) return false;
    const unsigned model=c.value[organ::TYPE],percussion=c.value[organ::PERC];
    if(model>5){
        volatile int* cleared=out;
        for(unsigned i=0;i<256;++i)cleared[i]=0;
        return false;
    }
    registration(v,step,c,rows);
    const int grit_goal=control(c.value[organ::GRIT])*256,perc_goal=control(percussion)*256;
    const unsigned vibe_raw=c.value[organ::VIBE];
    const int vibe_goal=vibe_raw<=16384?0:vibe_raw>=32767?one:int(vibe_raw-16384)*512;
    const bool keyed=percussion==10922 || percussion==21845 || percussion==32767;
    const bool low_on=keyed && percussion!=21845 && step<0x1e000000u;
    const bool high_on=keyed && percussion!=10922 && step<0x14000000u;
    const int low_gain=percussion==32767?2949:4915,high_gain=percussion==32767?3277:5243;
    for(unsigned i=0;i<64;++i){
        const unsigned p=v.phase;
        const int sign=int(p)>>31;
        const unsigned folded=(p^unsigned(sign))-unsigned(sign);
        // Exact half-cycle falls back to the zero at the start of every row.
        out[64+i]=int((folded>>22)&511u);
        out[128+i]=int(((folded>>7)&32767u) | (unsigned(sign)<<16));
        out[192+i]=int(p);
        v.vibe_smooth=ease33(v.vibe_smooth,vibe_goal);
        const int vibe=v.vibe_smooth>>8;
        if(vibe){
            const int depth=product_q15(product_q15(20972,organ_fixed::sine(v.vibrato)),vibe);
            v.phase+=step+unsigned(product_q15(int(step),depth)>>8);
        } else v.phase+=step;
        v.vibrato+=518975u;
    }
    for(unsigned i=0;i<64;++i){
        const unsigned p=unsigned(out[192+i]);
        v.grit_smooth=ease33(v.grit_smooth,grit_goal);
        const int grit=v.grit_smooth>>8;
        const int hd=v.harmonic_target-v.harmonic;
        const int hs=product_q15(hd,98);
        v.harmonic+=hs+int(unsigned(hd-1)<334u);
        int sum=0;
        if(v.click) {
            v.seed=v.seed*1664525u+1013904223u;
            sum=product_q15(product_q15(product_q15(v.click,grit),model==0?2949:1802),int(v.seed>>16)-32768);
            v.click=decay_positive<819>(v.click);
        }
        if(v.harmonic && grit)sum+=product_q15(product_q15(product_q15(v.harmonic,grit),590),organ_fixed::sine(p*7u));
        out[i]=sum;
    }
    for(unsigned i=0;i<64;++i){
        const unsigned p=unsigned(out[192+i]);
        v.perc_smooth=ease33(v.perc_smooth,perc_goal);
        const int perc=keyed?0:v.perc_smooth>>8;
        const unsigned harmonic=perc<16384?4:6;
        int sum=0;
        if(perc && v.percussion && step<(harmonic==4?0x1e000000u:0x14000000u))
            sum+=product_q15(product_q15(product_q15(v.percussion,perc),6554),organ_fixed::sine(p*harmonic));
        v.percussion=perc<16384?decay_positive<6>(v.percussion):decay_positive<11>(v.percussion);
        const unsigned age=v.age+i;
        if(low_on && v.low){
            int amp=product_q15(v.low,low_gain);
            if(age<256)amp=product_q15(amp,int(age*128));
            sum+=product_q15(amp,organ_fixed::sine(p*4u));
        }
        if(high_on && v.high){
            int amp=product_q15(v.high,high_gain);
            if(age<64)amp=product_q15(amp,int(age*512));
            sum+=product_q15(amp,organ_fixed::sine(p*6u));
        }
        v.low=decay_positive<6>(v.low);v.high=decay_positive<11>(v.high);
        out[i]+=sum;
    }
    if(v.age<256)v.age+=64;
    for(unsigned j=0;j<18;++j)if(v.groups[j].weight || v.groups[j].target){
        const auto& g=v.groups[j];
        const bool interpolated=g.a!=g.b && g.fraction;
        if((!interpolated && g.a==row_at(rows,5)) || (!g.target && g.weight<256)){
            // Row 5 is silent throughout the authored table. A sub-Q15
            // fading weight also contributes exactly zero and falls one
            // Q23 unit per sample for the remainder of its lifetime.
            int w=g.weight;
            if(!g.target && w<256)w=w>64?w-64:0;
            else if(!g.target) {
                for(unsigned k=0;k<64;++k)w=product_q15(w,32670);
            } else {
                for(unsigned k=0;k<64;++k){
                    const int delta=g.target-w;
                    w=g.target+product_q15(-delta,32670)+int(unsigned(delta-1)<334u);
                }
            }
            v.groups[j].weight=w;
        } else if(!g.target){
            if(interpolated)group<true,true>(v,j,out);
            else group<false,true>(v,j,out);
        } else {
            if(interpolated)group<true,false>(v,j,out);
            else group<false,false>(v,j,out);
        }
    }
    for(int i=63;i>=0;--i){
        const int sample=organ_fixed::clamp(product_q15(out[i],5243),-one,one-1)*256;
        out[4*i]=out[4*i+1]=out[4*i+2]=out[4*i+3]=sample;
    }
    return true;
}
}
