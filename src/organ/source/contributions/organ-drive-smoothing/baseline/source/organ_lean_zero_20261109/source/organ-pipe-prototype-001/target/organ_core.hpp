#ifndef ORGAN_CORE_HPP
#define ORGAN_CORE_HPP
// Original host-first ORGAN DSP. No Field addresses, allocation or I/O.
// Float arithmetic is intentional for audition; BF707 cost is NOT qualified.
#include <stdint.h>
namespace organ {
constexpr unsigned rate=48000, partials=48;
enum Parameter { LOW, MIDDLE, HIGH, SPIN, TYPE, GRIT, PERC, VIBE, COUNT };
static const char* const labels[COUNT]={"LOW","MIDDLE","HIGH","SPIN","TYPE","GRIT","PERC","VIBE"};
struct Controls {
    uint16_t value[COUNT]={27000,18000,10000,6000,0,3500,10000,4000};
    bool set(unsigned i,int v) {
        if(i>=COUNT || v<0 || v>(i==TYPE?5:32767)) return false;
        value[i]=uint16_t(v); return true;
    }
    bool turn(unsigned encoder,bool shift,int delta) {
        if(encoder>=4) return false;
        unsigned i=encoder+(shift?4:0); const int max=i==TYPE?5:32767;
        int64_t next=int64_t(value[i])+delta;
        return set(i,int(next<0?0:next>max?max:next));
    }
};
inline uint32_t crc(const uint8_t* p,unsigned n) {
    uint32_t c=~0u;
    for(unsigned i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}
    return ~c;
}
// Host record, deliberately not a Field preset/knob-array encoding.
inline void save(const Controls& c,uint8_t out[24]) {
    out[0]='O';out[1]='R';out[2]='G';out[3]=1;
    for(unsigned i=0;i<COUNT;++i){out[4+i*2]=uint8_t(c.value[i]);out[5+i*2]=uint8_t(c.value[i]>>8);}
    uint32_t sum=crc(out,20);for(unsigned i=0;i<4;++i)out[20+i]=uint8_t(sum>>(8*i));
}
inline bool restore(Controls& c,const uint8_t* in,unsigned n) {
    if(n!=24 || in[0]!='O'||in[1]!='R'||in[2]!='G'||in[3]!=1) return false;
    uint32_t sum=0;for(unsigned i=0;i<4;++i)sum|=uint32_t(in[20+i])<<(8*i);
    if(sum!=crc(in,20)) return false;
    Controls tmp;
    for(unsigned i=0;i<COUNT;++i)if(!tmp.set(i,in[4+2*i]|(unsigned(in[5+2*i])<<8)))return false;
    c=tmp;return true;
}
static const int16_t sine_table[257]={
#include "../src/sine_table.inc"
};
inline float sine(uint32_t p) {
    unsigned i=p>>24;float f=float(p&0xffffffu)*(1.0f/16777216.0f);
    return (sine_table[i]+(sine_table[i+1]-sine_table[i])*f)*(1.0f/32768.0f);
}
inline float absf(float x){return x<0?-x:x;}
inline float saturate(float x){return x/(1.0f+absf(x));}
static const unsigned footage[9]={1,3,2,4,6,8,10,12,16}; // multiples of 16-foot base
static const float weights[6][9]={
 {0.85f,0.55f,1.0f,0.85f,0.65f,0.55f,0.40f,0.32f,0.28f},
 {0.55f,0.35f,1.0f,0.80f,0.70f,0.70f,0.45f,0.55f,0.40f},
 {0.65f,0.50f,1.0f,0.95f,0.65f,0.75f,0.65f,0.50f,0.45f},
 {0.35f,0.25f,1.0f,0.65f,0.45f,0.55f,0.40f,0.35f,0.30f},
 {0.80f,0.45f,1.0f,0.90f,0.55f,0.65f,0.40f,0.35f,0.35f},
 {0.80f,0.25f,1.0f,0.55f,0.25f,0.30f,0.12f,0.12f,0.06f}
};
// Distinct generators, before drawbar summation: near-sine, odd square,
// alternating saw/reed, narrow pulse, and mellow even-harmonic electric.
inline float component(unsigned type,unsigned h) {
    if(h==1)return 1.0f;
    if(type==0)return h==3?0.018f:0.0f;
    if(type==1)return (h&1)?0.72f/float(h):0.0f;
    if(type==2)return (h&1?0.8f:-0.65f)/float(h);
    if(type==5)return h==2?0.20f:h==3?0.08f:h==4?0.025f:0.0f;
    if(type==3)return sine(uint32_t(uint64_t(h)*0x38000000u))*0.65f/float(h);
    return h==2?0.32f:h==3?0.12f:h==4?0.045f:0.0f;
}
struct Voice {
    uint32_t phase=0,increment=0,seed=1;
    float spectrum[partials+1]={},target[partials+1]={};
    float envelope=0,percussion=0,click=0,last=0,stolen=0;
    int note=-1; bool held=false; uint32_t age=0;
};
inline void coefficients(Voice& v,const Controls& c) {
    unsigned type=c.value[TYPE];
    for(unsigned n=0;n<=partials;++n)v.target[n]=0;
    for(unsigned d=0;d<9;++d) {
        float g=float(c.value[d/3])/32767.0f;
        g=g*g*weights[type][d];
        for(unsigned h=1;h<=7 && h*footage[d]<=partials;++h)
            v.target[h*footage[d]]+=g*component(type,h);
    }
    // Fixed gain per family, independent of registration. Turning one group
    // cannot rescale another. Suppress partials before Nyquist, including vibe.
    const float normal[6]={0.19f,0.14f,0.12f,0.17f,0.16f,0.18f};
    for(unsigned n=1;n<=partials;++n) {
        float f=float(v.increment)*(float(n)/4294967296.0f);
        float fade=f<0.42f?1.0f:f>=0.47f?0.0f:(0.47f-f)*20.0f;
        v.target[n]*=normal[type]*fade;
    }
}
inline float voice_sample(Voice& v,float grit,float perc,float vibe,float vibrato,unsigned type) {
    if(v.note<0)return 0;
    float x=0;
    for(unsigned n=1;n<=partials;++n) {
        v.spectrum[n]+=(v.target[n]-v.spectrum[n])*0.003f;
        x+=sine(v.phase*n)*v.spectrum[n];
    }
    unsigned ph=perc<0.5f?4:6;
    if(uint64_t(v.increment)*ph<0x78000000u)
        x+=sine(v.phase*ph)*v.percussion*perc*0.20f;
    v.percussion*=perc<0.5f?0.99982f:0.99965f;
    v.seed=v.seed*1664525u+1013904223u;
    x+=(float(v.seed>>16)/32768.0f-1.0f)*v.click*grit*(type==0?0.09f:0.055f);
    v.click*=0.975f;
    // Coupled leakage follows the registration so all drawbars down stays silent.
    x+=sine(v.phase*7u)*v.spectrum[2]*grit*0.018f;
    v.phase+=uint32_t(double(v.increment)*(1.0+double(vibrato*vibe)*0.0025));
    if(v.held)v.envelope+=(1.0f-v.envelope)*0.012f;
    else v.envelope*=0.9983f;
    x*=v.envelope;
    x+=v.stolen;v.stolen*=0.98f;
    v.last=x;
    if(!v.held && v.envelope<0.00001f){v.note=-1;v.envelope=0;v.last=0;}
    return x;
}
struct Movement {
    float line[2048]={};unsigned write=0;
    float bass=0,speed_h=0,speed_b=0;
    uint32_t horn=0,drum=0,vibrato=0;
    float tap(float delay)const {
        unsigned whole=unsigned(delay);float f=delay-whole;
        float a=line[(write-whole)&2047u],b=line[(write-whole-1)&2047u];return a+(b-a)*f;
    }
    void process(float input,float spin,float vibe,float& left,float& right) {
        float wanted=spin<0.02f?0.0f:0.6f+spin*6.3f;
        speed_h+=(wanted-speed_h)*(wanted>speed_h?0.000035f:0.000018f);
        float db=wanted*0.78f;speed_b+=(db-speed_b)*0.000012f;
        horn+=uint32_t(speed_h*(4294967296.0/48000.0));
        drum+=uint32_t(speed_b*(4294967296.0/48000.0));
        vibrato+=uint32_t(5.8*(4294967296.0/48000.0));
        bass+=(input-bass)*0.09f;
        line[write]=input-bass;
        float motion=spin<0.02f?speed_h/0.6f:1.0f;
        if(motion>1)motion=1;
        float h=sine(horn)*motion,d=sine(drum)*motion;
        float l=tap(40+18*h),r=tap(40-18*h);
        float chorus_l=tap(400+90*sine(vibrato));
        float chorus_r=tap(490+90*sine(vibrato+0x40000000u));
        left=(bass*(1+0.15f*d)+l*(1+0.28f*h))*(1-0.25f*vibe)+chorus_l*0.25f*vibe;
        right=(bass*(1-0.15f*d)+r*(1-0.28f*h))*(1-0.25f*vibe)+chorus_r*0.25f*vibe;
        write=(write+1)&2047u;
    }
};
} // namespace organ
#endif
