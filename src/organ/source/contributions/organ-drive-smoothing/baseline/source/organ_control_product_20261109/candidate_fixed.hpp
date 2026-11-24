// Isolated experiment based on the pinned ORGAN source; not a firmware replacement.
#ifndef ORGAN_FIXED_HPP
#define ORGAN_FIXED_HPP
#include "../organ_lean_zero_20261109/source/organ-pipe-prototype-001/target/organ_core.hpp" // shared control schema and sine table; float code is not called
#include "../organ_lean_zero_20261109/source/organ-pipe-prototype-001/target/organ_fixed_tables.hpp"
#include "../organ_lean_zero_20261109/source/organ-pipe-prototype-001/target/group_tables.hpp"
// Integer implementation of the reference model, not yet a Field callback.
// Audio/coefficient/delay state Q23; modulation Q15; phase unsigned Q32.
// No allocation, division, float or 64-bit operations in the sample path.
namespace organ_fixed {
using organ::Controls;
constexpr int32_t one=8388608;
inline int32_t clamp(int32_t x,int32_t lo,int32_t hi){return x<lo?lo:x>hi?hi:x;}
__attribute__((always_inline)) inline int32_t mul(int32_t a,int32_t q15){
    // Exact floor(a*q15/32768), without a 64-bit product. Inputs in this
    // processor are bounded below +/-2^30, q15 below +/-32768.
    return (a>>15)*q15+(((a&32767)*q15)>>15);
}
__attribute__((always_inline)) inline int32_t sine(uint32_t p){
    unsigned i=p>>24;int32_t f=int32_t((p>>9)&32767);
    return organ::sine_table[i]+(((organ::sine_table[i+1]-organ::sine_table[i])*f)>>15);
}
__attribute__((always_inline)) inline int32_t decay(int32_t x,int32_t amount){
    if(!x)return 0;
    int32_t magnitude=x<0?-x:x;
    int32_t step=mul(magnitude,amount)+1;
    magnitude-=step>magnitude?magnitude:step;
    return x<0?-magnitude:magnitude;
}
__attribute__((always_inline)) inline int32_t ease(int32_t x,int32_t target,int32_t amount){
    int32_t delta=target-x,step=mul(delta,amount);
    // A positive sub-LSB step must still converge (including negative partials
    // ramping to zero); otherwise a residual harmonic can remain indefinitely.
    if(!step&&delta>0)step=1;
    return x+step;
}
// Specialization only for control smoothers with valid state and targets in
// [0, 8388608]. For amounts 19 and 33, delta*amount fits signed32.
// Preserve floor rounding and positive sub-LSB convergence exactly.
template<int32_t amount> __attribute__((always_inline)) inline int32_t ease_control(int32_t x,int32_t target){
    static_assert(amount>0 && amount<=255,"control delta product must fit signed32");
    int32_t delta=target-x,step=(delta*amount)>>15;
    if(!step&&delta>0)step=1;
    return x+step;
}
inline int32_t control(unsigned x){return int32_t(x)+(x>>14);}
__attribute__((always_inline)) inline int32_t saturate(int32_t x){
    bool neg=x<0;uint32_t u=uint32_t(neg?-x:x);
    if(u>=8u*one)return neg?-saturation[256]:saturation[256];
    unsigned i=u>>18;int32_t y=saturation[i]+mul(saturation[i+1]-saturation[i],int32_t((u&262143)>>3));
    return neg?-y:y;
}
struct Voice {
    uint32_t phase=0,increment=0,seed=1;
    int32_t spectrum[49]={},target[49]={};
    int32_t percussion=0,click=0;
    // The bitset excludes exactly zero partials, including after their ramps.
    uint32_t active_lo=0,active_hi=0;
    // Each model retains its own group weight during interrupted transitions.
    int32_t group[18]={},group_target[18]={};
    uint16_t table_a[18]={},table_b[18]={},band_fraction[18]={};
    uint32_t groups_active=0;
};
// Source-only grouped wavetable approximation. Pitch-band interpolation follows
// the current block's highest increment; it does not retain old pitch spectra.
inline void coefficients(Voice& v,const Controls& c){
    int32_t gain[3];
    for(unsigned g=0;g<3;++g){int32_t x=control(c.value[g]);gain[g]=mul(x,x);}
    const unsigned model=c.value[organ::TYPE];
    v.groups_active=0;
    unsigned model_index=0,group_index=0;
    for(unsigned i=0;i<18;++i){
        v.group_target[i]=model_index==model?gain[group_index]*256:0;
        if(++group_index==3){group_index=0;++model_index;}
        if(v.group_target[i]||v.group[i]){
            v.groups_active|=1u<<i;
            const auto* knots=organ_tables::knots[i];
            const unsigned count=organ_tables::counts[i];unsigned k=0;
            while(k+1<count&&v.increment>=knots[k+1].increment)++k;
            v.table_a[i]=knots[k].wave;
            v.table_b[i]=k+1<count?knots[k+1].wave:knots[k].wave;
            uint32_t f=0;
            if(k+1<count){f=(((v.increment-knots[k].increment)>>knots[k].shift)*knots[k].multiplier)>>16;if(f>32768)f=32768;}
            v.band_fraction[i]=uint16_t(f);
        }
    }
    // The independent grit branch requires harmonic 2's original state.
    uint32_t frequency=v.increment>0x7851eb85u/2?0x7851eb85u:v.increment*2u;
    int32_t t=0;for(unsigned g=0;g<3;++g)t+=mul(registration[model][g][2],gain[g]);
    int32_t fade=frequency<=0x6b851eb8u?32768:frequency>=0x7851eb85u?0:int32_t(((0x7851eb85u-frequency)*20u)>>17);
    v.target[2]=mul(t,fade);
}
__attribute__((always_inline)) inline int32_t wave_lookup_q15(unsigned table,uint32_t phase){
    bool negative=(phase&0x80000000u)!=0;if(negative)phase=0u-phase;
    unsigned i=phase>>22;int32_t f=int32_t((phase>>7)&32767);
    if(i==organ_tables::half)return 0;
    const int16_t* t=organ_tables::wave[table];
    int32_t a=t[i],b=t[i+1];int32_t y=a+(((b-a)*f)>>15);
    return negative?-y:y;
}
__attribute__((always_inline)) inline int32_t wave_lookup(unsigned table,uint32_t phase){
    return wave_lookup_q15(table,phase)*256;
}
// Group weights and targets stay in [0, one]. Their difference times 98
// is bounded by 822083584, so this signed32 product is exact. Keep the
// original floor rounding and positive sub-LSB convergence unchanged.
__attribute__((always_inline)) inline int32_t ease_group(int32_t x,int32_t target){
    int32_t delta=target-x,step=(delta*98)>>15;
    if(!step&&delta>0)step=1;
    return x+step;
}
inline int32_t tone(Voice& v,int32_t grit,int32_t perc,int32_t vibe,int32_t vibrato,unsigned type){
    int32_t x=0;uint32_t bits=v.groups_active;unsigned first=0;
    // Skip only leading empty model groups. No added state, table pointers,
    // intrinsic helper, reordered active group, or truncated fade.
    if((bits&7u)==0){
        if((bits&511u)==0){bits>>=9;first=9;}
        if((bits&7u)==0){bits>>=3;first+=3;}
        if((bits&7u)==0){bits>>=3;first+=3;}
    }
    for(unsigned i=first;i<18&&bits;++i,bits>>=1){
        if(bits&1u){
            if(v.group[i]!=v.group_target[i])v.group[i]=ease_group(v.group[i],v.group_target[i]);
            const int32_t first=wave_lookup_q15(v.table_a[i],v.phase);
            int32_t y=first*256;
            // Both lookup values are in [-32767,32767] for the pinned tables;
            // band_fraction is in [0,32768]. Difference*band_fraction fits
            // signed32, and division by 128 retains the original Q23 floor.
            if(v.table_a[i]!=v.table_b[i])
                y+=((wave_lookup_q15(v.table_b[i],v.phase)-first)*int32_t(v.band_fraction[i]))>>7;
            x+=mul(y,v.group[i]>>8);
        }
    }
    if(v.spectrum[2]!=v.target[2])v.spectrum[2]=ease(v.spectrum[2],v.target[2],98);
    unsigned harmonic=perc<16384?4:6;
    if(perc && v.percussion && v.increment<(0x78000000u/(harmonic==4?4:6)))
        x+=mul(mul(mul(v.percussion,perc),6554),sine(v.phase*harmonic));
    v.percussion=decay(v.percussion,perc<16384?6:11);
    v.seed=v.seed*1664525u+1013904223u;
    if(v.click && grit)x+=mul(mul(mul(v.click,grit),type==0?2949:1802),int32_t(v.seed>>16)-32768);
    v.click=decay(v.click,819);
    if(v.spectrum[2] && grit)x+=mul(mul(mul(v.spectrum[2],grit),590),sine(v.phase*7u));
    // Split multiply preserves low pitch bits without double conversion.
    // Exact: for vibe==0 both fixed-point products and bend are zero.
    if(vibe){
        int32_t depth=mul(mul(20972,vibrato),vibe);
        int32_t bend=mul(int32_t(v.increment),depth)>>8;
        v.phase+=v.increment+uint32_t(bend);
    } else v.phase+=v.increment;
    return x;
}
struct Movement {
    int32_t line[2048]={};unsigned write=0;
    int32_t bass=0;
    // Rotor speeds in Hz Q23; remainder carries sub-unit deceleration.
    uint32_t speed_h=0,speed_b=0;
    int32_t remainder_h=0,remainder_b=0;
    uint32_t horn=0,drum=0,vibrato=0;
    __attribute__((always_inline)) inline int32_t tap(int32_t delay_q15)const{
        unsigned n=unsigned(delay_q15)>>15;int32_t f=delay_q15&32767;
        int32_t a=line[(write-n)&2047u];return a+mul(line[(write-n-1)&2047u]-a,f);
    }
    void process(int32_t input,int32_t spin,int32_t vibe,int32_t& left,int32_t& right){
        // Speeds represented in Hz Q23 (maximum 6.9 Hz fits signed 32-bit).
        int32_t wanted=spin<655?0:5033165+mul(52848230,spin);
        // Q23 coefficient with split products: 0.000035 / 0.000018 / .000012.
        auto advance=[](int32_t value,int32_t target,unsigned k,int32_t& remainder) __attribute__((always_inline)) {
            int32_t d=target-value;
            int32_t step=mul(d,int32_t(k))+remainder;
            remainder=step&255;
            return value+(step>>8);
        };
        speed_h=uint32_t(advance(int32_t(speed_h),wanted,wanted>int32_t(speed_h)?294:151,remainder_h));
        speed_b=uint32_t(advance(int32_t(speed_b),mul(wanted,25559),101,remainder_b));
        // 512/48000 approximated to 17 ppm; all intermediate products bounded.
        horn+=uint32_t(mul(int32_t(speed_h),22370)>>6);
        drum+=uint32_t(mul(int32_t(speed_b),22370)>>6);
        vibrato+=518975u;
        bass=ease(bass,input,2949);line[write]=input-bass;
        int32_t motion=spin<655?clamp(mul(int32_t(speed_h),213),0,32768):32768;
        // When stopped and fully settled, horn and drum contributions are
        // exactly zero, and both channels read the same unmodulated tap.
        // Keep all rotor, vibrato, line and speed state updates above intact.
        int32_t h=0,d=0;
        if(motion){h=mul(sine(horn),motion);d=mul(sine(drum),motion);}
        int32_t l=tap(40*32768+18*h),r=motion?tap(40*32768-18*h):l;
        int32_t blend=vibe>>2;
        int32_t cl=0,cr=0;
        // Both taps contribute exactly zero at zero blend. They do not mutate
        // state; rotor, vibrato and delay writes continue on every sample.
        if(blend){
            cl=tap(400*32768+90*sine(vibrato));
            cr=tap(490*32768+90*sine(vibrato+0x40000000u));
        }
        left=mul(bass+mul(bass,mul(d,4915))+l+mul(l,mul(h,9175)),32768-blend)+mul(cl,blend);
        right=mul(bass-mul(bass,mul(d,4915))+r-mul(r,mul(h,9175)),32768-blend)+mul(cr,blend);
        write=(write+1)&2047u;
    }
};
// Fixed six-voice gain followed by the original dry/wet preamp.
__attribute__((always_inline)) inline int32_t preamp(int32_t sum,int32_t grit){
    int32_t x=mul(sum,5243),driven=x+5*mul(x,grit);
    return mul(x,32768-grit)+mul(saturate(driven),mul(grit,19661));
}
}
#endif
