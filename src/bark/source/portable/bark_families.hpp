#pragma once
#include "../reference/bark_ep_tables.hpp"
#include "../contact.hpp"

// Four original fixed-point keyboard prototypes. No device addresses or heap.
// Stock/host amplitude envelope owns release; these are excitation/body decays.
namespace bark_families {
using i32=int;using u32=unsigned;
constexpr unsigned frames=128;
enum Family {Rhodes=0,Wurli=1,Pianet=2,CP70=3};
inline i32 clamp(i32 x,i32 lo,i32 hi){return x<lo?lo:x>hi?hi:x;}
inline i32 mul(i32 a,i32 b){return (a*b)>>15;}
inline i32 mix(i32 a,i32 b,i32 t){return a+mul(b-a,t);}
inline u32 ratio(u32 p,u32 q){return (p>>12)*q+((p&4095)*q>>12);}
inline i32 sine(u32 p){unsigned i=p>>22;i32 f=(p>>6)&65535;return bark_ep::sine_table[i]+((bark_ep::sine_table[i+1]-bark_ep::sine_table[i])*f>>16);}
inline i32 control(i32 p){return p<=0?0:p>=2147418112?32768:static_cast<i32>(static_cast<u32>(p)>>16);}
struct Voice {
    u32 phase[3],wag,reserved; // Keep the established native voice extent.
    i32 body,hit;
    unsigned short controls[4]; // Proven closed range0..32768; compact per-voice smoothing state.
    unsigned char initialized,active;
};
static_assert(sizeof(Voice)==40,"Independent private voice extent");
inline void initialize(Voice&v){volatile unsigned char*p=reinterpret_cast<volatile unsigned char*>(&v);for(unsigned i=0;i<sizeof(v);++i)p[i]=0;}
inline void start(Voice&v,i32 velocity=25802){
    for(unsigned i=0;i<3;++i)v.phase[i]=0;
    v.body=32768;v.hit=8192+mul(24576,clamp(velocity,0,32768));v.reserved=0;
    for(unsigned i=0;i<4;++i)v.controls[i]=0;
    v.initialized=0;v.active=1;
}
struct Coefficients {
    i32 second,third,strike,pm,body_decay,hit_decay,level;
    u32 r1,r2;
};

inline Coefficients coefficients(unsigned family,const i32* p) {
    const i32 color=p[0],strike=p[1],bark=p[2];
    Coefficients k{};
    k.r1=8192;
    k.r2=12288;
    if(family==Rhodes) {
        // Keep the high tine mode and phase-modulated hammer contact.
        k.r1=4096;k.r2=28692;
        k.third=900+mul(color,4600)+mul(mul(color,color),3800);
        k.strike=mul(strike,8700);k.pm=700+mul(strike,7400);
        k.body_decay=32660+mul(bark,87);k.hit_decay=30400+mul(strike,1900);k.level=5400;
    } else if(family==Wurli) {
        k.second=1000+mul(color,6600);k.third=650+mul(color,4300);
        k.strike=mul(strike,5800);
        k.body_decay=32630+mul(bark,105);k.hit_decay=29900+mul(strike,950);k.level=5000;
    } else if(family==Pianet) {
        k.r1=12288;k.r2=20480;
        k.second=900+mul(color,4200);k.third=700+mul(color,3800);
        k.strike=mul(strike,9200);
        k.body_decay=32510+mul(bark,195);k.hit_decay=28600+mul(strike,1450);k.level=5400;
    } else {
        // Preserve the stretched strings, with more upper string at high Color.
        k.r1=8197+static_cast<u32>(mul(color,5));k.r2=16412+static_cast<u32>(mul(color,16));
        k.second=1500+mul(color,3100);k.third=350+mul(color,8400);
        k.strike=mul(strike,6400);
        k.body_decay=32670+mul(bark,76);k.hit_decay=30000+mul(strike,900);k.level=5200;
    }
    return k;
}

// Signed contact weights follow the three existing mechanical modes in order:
// fundamental, first family-specific partial, upper partial. Wood on a Pianet
// changes the release of its bent reed; the underlying pluck remains intact.
struct Contact {
    i32 low, middle, high, decay, phase_mod;
};
constexpr Contact contacts[4][3]={
    {{7800, 1200, 600, 31750, 1800}, {2600, 7400, 2900, 30950, 11700}, {1000, -2100, 14800, 27950, 15800}},
    {{8100, 3300, 300, 31600, 0}, {1800, 11800, 1800, 30600, 0}, {-1700, 7100, 12800, 27800, 0}},
    {{11000,-2400, 400, 30800, 0}, {10200,-8400, 1800, 29800, 0}, {4200,-11200, 9100, 27000, 0}},
    {{7800, 4300, 800, 31600, 0}, {1900, 9400,-1700, 30900, 0}, {-2400, 5200,14400, 27900, 0}}
};
// Rubber briefly unloads and recontacts. Interpolate the gain across each
// callback to avoid block edges. The maximum return occurs at ~14.5 ms.
inline int rubber_pressure(unsigned block){
    constexpr int curve[]={32768,28672,22600,16000,11100,8300,9200,12900,19500,27200,31900,30800,29300,28000,27800};
    return curve[block<14?block:14];
}

// No floating point, division, allocation, UI or native callbacks in audio.
// The portable pitch-vector path preserves within-block changes. The public
// Open-Field engine supplies one phase increment for the whole block.
inline u32 block_highest(const u32* pitch) {
    u32 highest=0;
    for(unsigned i=0;i<frames;++i)if(pitch[i]>highest)highest=pitch[i];
    return highest;
}
inline u32 block_highest(u32 step) { return step; }
inline u32 sample_step(const u32* pitch,unsigned i) { return pitch[i]; }
inline u32 sample_step(u32 step,unsigned) { return step; }
template<unsigned family, bool alternate=false, class Pitch>
__attribute__((always_inline)) inline void render_style(Voice&v,i32*out,Pitch pitch,const i32*parameters,unsigned material=bark_contact::Original){
    for(unsigned i=0;i<4;++i){i32 t=control(parameters[i]),d=t-v.controls[i];
        v.controls[i]=!v.initialized||(-4<=d&&d<=4)?t:v.controls[i]+d/4;}
    const i32 controls[4]={v.controls[0],v.controls[1],v.controls[2],v.controls[3]};
    auto k=coefficients(family,controls);
    Contact contact{};
    if(alternate){
        contact=contacts[family][material-1];
        k.hit_decay=mix(k.hit_decay,contact.decay,controls[1]);
        if(family==Rhodes)k.pm=700+mul(controls[1],contact.phase_mod);
    }
    const i32 color=v.controls[0],bark=v.controls[2],wag=v.controls[3];
    const i32 body1=mul(v.body,k.body_decay),hit1=mul(v.hit,k.hit_decay);
    int transient=v.hit, next_transient=hit1;
    if(alternate && material==bark_contact::Rubber){
        const int pressure=mix(32768,rubber_pressure(v.reserved),controls[1]);
        const int next_pressure=mix(32768,rubber_pressure(v.reserved+1),controls[1]);
        transient=mul(v.hit,pressure);next_transient=mul(hit1,next_pressure);
        if(v.reserved<14)++v.reserved;
    }
    i32 body=v.body*128,hit=transient*128;const i32 db=body1-v.body,dh=next_transient-transient;
    const u32 highest=block_highest(pitch);
    // Suppress complete modes before Nyquist; PM/pickup colour fades much
    // earlier. This is measured alias mitigation, not formal bandlimiting.
    const i32 baseband=highest>=0x78000000u?0:clamp(static_cast<i32>((0x78000000u-highest)>>13),0,32768);
    const i32 colourband=highest>=0x10000000u?0:clamp(static_cast<i32>((0x10000000u-highest)>>12),0,32768);
    auto modeband=[highest](u32 r)->i32{
        // All ratios <=7.01; use split fixed-point math and reject overflow first.
        if(highest>=0x10000000u&&r>8192)return 0;
        const u32 step=ratio(highest,r);
        return step>=0x70000000u?0:clamp(static_cast<i32>((0x70000000u-step)>>13),0,32768);
    };
    const i32 band1=modeband(k.r1),band2=modeband(k.r2);
    const i32 depth=mul(wag,20500),width=mul(wag,15500);
    // 48kHz audition coefficient assumption, retained separately from Field
    // physical timing. Wag0 freezes phase and guarantees equal stereo gains.
    const u32 step=wag?100000u+static_cast<u32>(mul(wag,22000))*12u:0;
    const u32 endwag=v.wag+step*128;
    const i32 w0=wag?sine(v.wag):0,w1=wag?sine(endwag):0;
    const i32 p0=wag?sine(v.wag+0x40000000u):0,p1=wag?sine(endwag+0x40000000u):0;
    const i32 a0=32768-(depth>>1)+mul(depth>>1,w0),a1=32768-(depth>>1)+mul(depth>>1,w1);
    const i32 l0=mul(a0,32768-mul(width,16384+(p0>>1))),l1=mul(a1,32768-mul(width,16384+(p1>>1)));
    const i32 r0=mul(a0,32768-mul(width,16384-(p0>>1))),r1=mul(a1,32768-mul(width,16384-(p1>>1)));
    // Body colour remains after the strike has decayed. Hold the oscillator
    // bands and fixed gains for the block to keep them out of the sample loop.
    const i32 second=mul(k.second,band1),third=mul(k.third,band2);
    // Allocate transient energy to each piano's own modes. Keep the
    // band limits independent so a suppressed upper mode stays silent.
    const i32 strike1=mul(k.strike,band1);
    const i32 strike2=mul(k.strike,band2);
    const i32 pluck=mul(k.strike,baseband);
    const i32 impact0=alternate?mul(mul(controls[1],contact.low),baseband):0;
    const i32 impact1=alternate?mul(mul(controls[1],contact.middle),band1):0;
    const i32 impact2=alternate?mul(mul(controls[1],contact.high),band2):0;
    const i32 pm_depth=mul(k.pm,colourband),level=mul(k.level,baseband);
    const i32 bright=mul(bark,colourband);
    const i32 drive=mul(bright,12000+mul(bright,20768));
    const i32 pickup=mul(drive,24576);
    const i32 clean_gain=21500-mul(color,3000)+mul(drive,4800);
    constexpr i32 knee_low=family==Rhodes?-22400:family==Wurli?-18400:family==Pianet?-23200:-21300;
    constexpr i32 knee_high=family==Rhodes?16600:family==Wurli?12400:family==Pianet?18700:16900;
    i32 left=l0*128,right=r0*128;
    for(unsigned i=0;i<128;++i){
        const i32 h=hit>>7,b=body>>7;
        const i32 s1=sine(v.phase[1]),s2=sine(v.phase[2]);
        const i32 pm=mul(pm_depth,h);
        const i32 fundamental=sine(v.phase[0]+(static_cast<u32>(mul(s1,pm))<<15));
        // A soft, asymmetric pickup knee adds new harmonics instead of raising
        // the profile's linear partial levels. Its make-up gain follows drive.
        const i32 excess=fundamental-clamp(fundamental,knee_low,knee_high);
        i32 x=mul(fundamental,clean_gain)-mul(excess,pickup);
        x+=mul(s1,second);
        if(alternate){
            const int impact=mul(fundamental,impact0)+mul(s1,impact1)+mul(s2,impact2);
            x+=mul(s2,third)+mul(impact,h);
        } else if (family==Rhodes) {
            // Preserve the Trial 11 tine and its original rounding.
            x+=mul(s2,third+mul(strike2,h));
        } else {
            x+=mul(s2,third);
            i32 contact;
            if (family==Wurli) {
                // Soft hammer on a reed: a brief, even-rich knock.
                contact=mul(s1,strike1)+mul(s2,strike2>>2);
            } else if (family==Pianet) {
                // A released, bent reed: a round fundamental front and
                // opposing odd partials for the short pad-release pop.
                contact=mul(fundamental,pluck)-mul(s1,strike1>>1)+mul(s2,strike2>>2);
            } else {
                // A hammer on short strings brings out both stretched
                // upper modes; their slight beating is already in the bank.
                contact=mul(s1,strike1>>1)+mul(s2,strike2);
            }
            x+=mul(contact,h);
        }
        x=mul(clamp(x,-32768,32767),mul(b,level));
        out[2*i]=mul(x,left>>7)*65536;out[2*i+1]=mul(x,right>>7)*65536;
        // Exact integer harmonic ratios reduce to an unsigned multiply.
        // Rhodes' upper tine and CP-70's stretched modes retain split Q12.
        const u32 increment=sample_step(pitch,i);
        v.phase[0]+=increment;
        v.phase[1]+=family==Rhodes?increment:family==Wurli?increment*2u:
            family==Pianet?increment*3u:ratio(increment,k.r1);
        v.phase[2]+=family==Wurli?increment*3u:family==Pianet?increment*5u:ratio(increment,k.r2);
        body+=db;hit+=dh;left+=l1-l0;right+=r1-r0;
    }
    v.body=body1;v.hit=hit1;v.wag=endwag;v.initialized=1;
    if(!body1)v.active=0;
}
template<bool alternate, class Pitch>
__attribute__((always_inline)) inline void by_family(Voice& voice,i32* output,Pitch pitch,const i32* controls,unsigned family,unsigned material){
    switch(family){
    case Rhodes:render_style<Rhodes,alternate>(voice,output,pitch,controls,material);break;
    case Wurli:render_style<Wurli,alternate>(voice,output,pitch,controls,material);break;
    case Pianet:render_style<Pianet,alternate>(voice,output,pitch,controls,material);break;
    case CP70:render_style<CP70,alternate>(voice,output,pitch,controls,material);break;
    }
}
template<class Pitch> __attribute__((always_inline)) inline void render(Voice& voice,i32* output,Pitch pitch,const i32* controls,unsigned family,unsigned material=bark_contact::Original) {
    if (!voice.active || family>CP70) {
        for (unsigned i=0;i<256;++i) output[i]=0;
        return;
    }
    if(material>=bark_contact::Felt && material<=bark_contact::Wood)
        by_family<true>(voice,output,pitch,controls,family,material);
    else
        by_family<false>(voice,output,pitch,controls,family,bark_contact::Original);
}
}
