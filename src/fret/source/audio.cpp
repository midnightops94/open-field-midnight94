#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3")
#endif
#include "build_r6e_card.h"

extern "C" void fret_render(const Op1fVoiceBlock*) OP1F_NAMED("fret_render");
extern "C" { volatile unsigned fret_level[8],fret_activity_stamp[8]; }
namespace {
#include "tables.inc"

constexpr int unity=8192;
int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
int mix(int a,int b,int t){return a+(((b-a)*t)>>15);}
int mul(int a,int b){return (a*b)>>13;}
int mode(int raw){return raw<8192?0:raw<24576?1:2;}
int sine(unsigned phase){return sines[phase>>21];}
inline int modalSum(const short* wave,const short* weights){
#if defined(__bfin__) && !defined(OP1F_HOST)
    // Four aligned pairs hold harmonics 2..8 and a zero coefficient.
    // Each hardware MAC accumulates one even and one odd partial.
    int sum;
    asm("I0=%1; I1=%2; R0=[I0++]; R1=[I1++];"
        " A1=R0.H*R1.H, A0=R0.L*R1.L (IS) || R0=[I0++] || R1=[I1++];"
        " A1+=R0.H*R1.H, A0+=R0.L*R1.L (IS) || R0=[I0++] || R1=[I1++];"
        " A1+=R0.H*R1.H, A0+=R0.L*R1.L (IS) || R0=[I0++] || R1=[I1++];"
        " A1+=R0.H*R1.H, A0+=R0.L*R1.L (IS); %0=(A0+=A1);"
        :"=d"(sum):"a"(wave),"a"(weights):"I0","I1","R0","R1","A0","A1","ASTAT","memory");
    return sum;
#else
    return wave[0]*weights[0]+wave[1]*weights[1]+wave[2]*weights[2]+
           wave[3]*weights[3]+wave[4]*weights[4]+wave[5]*weights[5]+wave[6]*weights[6];
#endif
}
int band(unsigned step,int multiplier){
    const unsigned at45=1932735283u, span=343597384u;
    // No 32-bit wrap when the partial is above the anti-alias transition.
    static const unsigned ceiling[]={0,1932735283u,966367641u,644245094u,483183820u,
        386547056u,322122547u,276105040u,241591910u,214748364u,193273528u};
    if(multiplier>10 || step>ceiling[multiplier])return 0;
    const unsigned rem=at45-step*unsigned(multiplier);
    return rem>=span?unity:int((rem>>10)*25u>>10);
}
unsigned midi(unsigned step){
    unsigned lo=0,hi=127;
    while(lo<hi){const unsigned middle=(lo+hi)>>1;
        if(step<=note_steps[middle]+((note_steps[middle+1]-note_steps[middle])>>1))hi=middle;
        else lo=middle+1;
    }
    return lo;
}
struct Coeff {
    alignas(4) short harmonic[8];
    int bodyLoss,biteLoss,releaseLoss,bodyLevel,noise,clank,clankRatio,invTransient,amp,drive[3];
};
struct Voice{
    unsigned phase,metal,rand,age,active,released,serial;
    int ring,bite,gate,lastNoise,velocity,dcIn,dcOut;
    Coeff current,destination;
    unsigned lastStep,easing;
    int registerLevel;
    int previous[8];
};
static_assert(sizeof(Voice)<=256,"voice allocation");
unsigned serial[8];
struct Register { unsigned turn,last[6]; int weight[6]; };
Register registers[8];
int targetLevel(int weight){
    const int w=clamp(weight-unity,0,5*unity);
    const unsigned i=unsigned(w)>>8;
    return i>=160?registerGain[160]:mix(registerGain[i],registerGain[i+1],(w&255)<<7);
}
const int hands[4][9]={
 {2130,614,229,106,41,25,0,451,98},{3686,1884,901,532,303,172,106,983,303},
 {4751,3359,2376,1638,1311,901,721,2048,639},{5407,5079,3932,3031,2376,1802,1311,3768,1884}};
const int bright[]={8192,4997,10650},attack[]={8192,5243,11141},stretch[]={5,1,12};
// Reciprocals for duration (in frames): index tracks hand plus the mute term.
int divide_pos(unsigned num,unsigned den){
    // Used at the control rate only; no run-time division library in a pack.
    unsigned result=0,bit=1;
    while(den<=num/2){den<<=1;bit<<=1;}
    while(bit){if(num>=den){num-=den;result|=bit;}den>>=1;bit>>=1;}
    return int(result);
}
void target(Coeff& c,const int* raw,unsigned step){
    const int hand=raw[0],pos=raw[1],mute=raw[2],amp=raw[3],s=mode(raw[4]),p=mode(raw[5]),d=mode(raw[6]),a=mode(raw[7]);
    const int ix=hand*3>>15,frac=(hand*3)&32767,pi=pos>>9,mi=mute>>9;
    c.harmonic[7]=0;
    for(int k=0;k<7;k++){
        const int h=mix(hands[ix][k],hands[ix+1][k],frac);
        const int spec=mix(spectral[s][pi][k],spectral[s][pi+1][k],(pos&511)<<6);
        const int coeff=(h*spec)>>12;
        c.harmonic[k]=mul((coeff*winding[k][p])>>14,band(step,k+2));
    }
    c.bodyLoss=mix(loss[s][d][mi][0],loss[s][d][mi+1][0],(mute&511)<<6);
    c.biteLoss=mix(loss[s][d][mi][1],loss[s][d][mi+1][1],(mute&511)<<6);
    const int releaseSpan=(d==0?5292:d==1?4057:2646);
    const int releaseFrames=1058+((releaseSpan*(32768-((mute*3)>>2)))>>15);
    c.releaseLoss=divide_pos(32u<<20,unsigned(releaseFrames));
    c.noise=mul(mul(mix(hands[ix][7],hands[ix+1][7],frac),mix(4833,9175,pos)),attack[s]);
    c.clank=mul(mul(mix(hands[ix][8],hands[ix+1][8],frac),mix(3932,8765,pos)),attack[s]);
    // mix(.94,1.08,brightness) intentionally extrapolates for Steel.
    const int ratio=mul(mix(41452,59310,hand),7700+mul(1147,bright[s]));
    c.clankRatio=mul(ratio,unity+stretch[s]);
    const int clankStep=int(((step>>16)*unsigned(ratio))>>13);
    c.clank=mul(c.clank,clamp((29491-clankStep)*25>>4,0,unity));
    const int duration=mul(mix(1058,2381,hand),unity-((mute*5489)>>15));
    c.invTransient=divide_pos(1u<<25,unsigned(duration));
    c.bodyLevel=mix(8520,7700,pos);
    c.amp=amp>>2;
    for(int i=0;i<3;i++)c.drive[i]=i==a?unity:0;
}
int smooth(int a,int b){
    const int delta=b-a;
    return a+(delta<0?-(((-delta)*838+32767)>>15):((delta*838+32767)>>15));
}
void ease(Coeff& c,const Coeff& to){
    for(int i=0;i<7;i++)c.harmonic[i]=smooth(c.harmonic[i],to.harmonic[i]);
    c.bodyLoss=smooth(c.bodyLoss,to.bodyLoss);c.biteLoss=smooth(c.biteLoss,to.biteLoss);
    c.releaseLoss=smooth(c.releaseLoss,to.releaseLoss);c.bodyLevel=smooth(c.bodyLevel,to.bodyLevel);
    c.noise=smooth(c.noise,to.noise);c.clank=smooth(c.clank,to.clank);c.clankRatio=smooth(c.clankRatio,to.clankRatio);
    c.invTransient=smooth(c.invTransient,to.invTransient);c.amp=smooth(c.amp,to.amp);
    for(int i=0;i<3;i++)c.drive[i]=smooth(c.drive[i],to.drive[i]);
}
int shape(int input,int gain,int fuzzLevel,int model){
    const int val=((input>>2)*gain)>>11;
    if(model==2)return mul(clamp(val,-6390,5325),fuzzLevel);
    const int a=val<0?-val:val,idx=clamp(a>>7,0,1023);
    const int v=shapes[model][idx]+((shapes[model][idx+1]-shapes[model][idx])*(a&127)>>7);
    return val<0?-v:v;
}
template<int model>
__attribute__((always_inline)) inline void settledDrive(int* output,int gain,int fuzzLevel,int wet){
    for(int i=0;i<32;i++){
        const int upper=output[i];
        const int coloured=shape(upper,gain,fuzzLevel,model);
        output[i]=upper+mul(coloured-upper,wet);
    }
}
int decay(int value,int loss){
    const int high=(value>>11)*loss;
    const int low=((value&2047)*loss)>>11;
    return value-((high+low+511)>>9);
}
unsigned phaseRatio(unsigned pitch,int ratio){return (pitch>>13)*unsigned(ratio)+(((pitch&8191u)*unsigned(ratio))>>13);}
unsigned seedNote(unsigned step,unsigned noteSerial){
    return (0x459c3f75u^((midi(step)+1u)*0x9e3779b9u)^(noteSerial*0x85ebca6bu))|1u;
}
}
extern "C" void fret_render(const Op1fVoiceBlock* b){
    auto& v=*static_cast<Voice*>(b->state);
    auto& bus=registers[b->slot];
    const unsigned voice=b->voice;
    ++bus.turn;
    if(b->events&OP1F_VOICE_FRESH){v.active=0;bus.weight[voice]=0;}
    const bool attackNote=(b->events&OP1F_VOICE_BEGINS)!=0;
    if(attackNote){
        v.phase=v.metal=v.age=v.released=0;v.lastNoise=v.dcIn=v.dcOut=0;v.active=1;v.serial=++serial[b->slot];v.rand=seedNote(b->step,v.serial);
        v.ring=v.bite=v.gate=1<<24;v.registerLevel=unity;
        v.velocity=1966+divide_pos(unsigned(b->velocity>>24)*6226u+63u,127);
    }
    if(b->events&OP1F_VOICE_ENDS)v.released=1;
    if(!v.active){bus.weight[voice]=0;for(unsigned i=0;i<b->frames;i++)b->out[i]=0;return;}
    // The SDK invokes each sounding voice once per block. Retire absent voices
    // after six other callbacks, bounding a stolen voice's contribution.
    int otherWeight=0;
    for(unsigned i=0;i<6;i++)if(i!=voice && bus.turn-bus.last[i]<=6)otherWeight+=bus.weight[i];
    bus.last[voice]=bus.turn;
    bool changed=attackNote || v.lastStep!=b->step;
    for(int i=0;i<8;i++){
        const int x=b->values[i]; changed|=v.previous[i]!=x;v.previous[i]=x;
    }
    if(changed){target(v.destination,b->values,b->step);v.lastStep=b->step;}
    if(attackNote){v.current=v.destination;v.easing=0;}
    else if(changed)v.easing=320;
    const Coeff& dest=v.destination;
    unsigned peak=0;
    for(unsigned group=0;group<b->frames;group+=32){
        if(v.easing){
            ease(v.current,dest);
            if(!--v.easing)v.current=dest;
        }
        const Coeff& c=v.current;
        int ring=v.ring,bite=v.bite,gate=v.gate;
        const int endRing=decay(ring,c.bodyLoss),endBite=decay(bite,c.biteLoss);
        const int endGate=v.released?decay(gate,c.releaseLoss):gate;
        const int weight=mul(mul(ring>>11,gate>>11),v.velocity);
        bus.weight[voice]=weight;
        const int goal=targetLevel(otherWeight+weight);
        int gain=v.registerLevel;
        const int nextGain=gain+(((goal-gain)*1923+16384)>>15);
        const int gainRamp=(nextGain-gain)*256;
        int fineGain=gain*8192;
        v.registerLevel=nextGain;
        const int dr=(endRing-ring)>>5, db=(endBite-bite)>>5,dg=(endGate-gate)>>5;
        const unsigned extra=phaseRatio(b->step,c.clankRatio);
        const int driveWet=mul(7700,c.amp),diGain=unity+mul(15565,c.amp),
                  tubeGain=unity+6*c.amp,fuzzGain=unity+8*c.amp,fuzzLevel=unity-mul(1966,c.amp);
        int* output=b->out+group;
        unsigned int oscillator=v.phase;
        for(int i=0;i<32;i++){
            const short* wave=partialWaves[(oscillator+(1u<<22))>>23];
            const int upper=modalSum(wave,c.harmonic);
            output[i]=mul(mul(upper>>13,bite>>11),v.velocity);
            bite+=db;oscillator+=b->step;
        }
        if(v.age<2400){
            unsigned rand=v.rand,metal=v.metal;int lastNoise=v.lastNoise;
            for(unsigned i=0;i<32;i++){
                int transient=clamp(unity-int((v.age+i)*c.invTransient>>12),0,unity);transient=mul(transient,transient);
                rand^=rand<<13;rand^=rand>>17;rand^=rand<<5;
                const int noise=int(rand>>18)-unity;
                const int excitation=mul(mul(noise-lastNoise,5079),c.noise)+mul(sine(metal),c.clank);
                output[i]+=mul(mul(excitation,transient),v.velocity);
                lastNoise=noise;metal+=extra;
            }
            v.rand=rand;v.metal=metal;v.lastNoise=lastNoise;
        }
        if(c.amp>0 && c.drive[0]==unity && !c.drive[1] && !c.drive[2]){
            settledDrive<0>(output,diGain,fuzzLevel,driveWet);
        }else if(c.amp>0 && !c.drive[0] && c.drive[1]==unity && !c.drive[2]){
            settledDrive<1>(output,tubeGain,fuzzLevel,driveWet);
        }else if(c.amp>0 && !c.drive[0] && !c.drive[1] && c.drive[2]==unity){
            settledDrive<2>(output,fuzzGain,fuzzLevel,driveWet);
        }else if(c.amp>0){
            // A live circuit transition can keep two or three weights moving.
            // Keep the original accumulation and rounding for that path.
            for(int i=0;i<32;i++){
                const int upper=output[i];int coloured=0;
                if(c.drive[0])coloured+=mul(shape(upper,diGain,fuzzLevel,0),c.drive[0]);
                if(c.drive[1])coloured+=mul(shape(upper,tubeGain,fuzzLevel,1),c.drive[1]);
                if(c.drive[2])coloured+=mul(shape(upper,fuzzGain,fuzzLevel,2),c.drive[2]);
                output[i]=upper+mul(coloured-upper,driveWet);
            }
        }
        unsigned ph=v.phase;int dcIn=v.dcIn,dcOut=v.dcOut;
        for(unsigned i=0;i<32;i++){
            const int bass=mul(mul(sine(ph),ring>>11),c.bodyLevel);
            const unsigned age=v.age+i;
            const int fade=age>=159?unity:int((age*52759u+512u)>>10);
            const int dry=mul(mul(mul(bass,6717)+mul(output[i],5079),2785),mul(fade,gate>>11));
            const int levelled=mul(dry,(fineGain+4096)>>13);
            fineGain+=gainRamp;
            dcOut=((levelled-dcIn)*4096)+dcOut-((dcOut+1024)>>11);dcIn=levelled;
            const int filtered=(dcOut+2048)>>12;
            output[i]=clamp(filtered,-8192,8191)*262144;
            const unsigned mag=unsigned(filtered<0?-filtered:filtered);if(mag>peak)peak=mag;
            ph+=b->step;ring+=dr;gate+=dg;
        }
        v.phase=ph;v.dcIn=dcIn;v.dcOut=dcOut;
        if(v.age<0x7fff0000u)v.age+=32;
        v.ring=endRing;v.bite=endBite;v.gate=endGate;
    }
    if((v.ring<670 && v.bite<670) || v.gate<670)v.active=0;
    const unsigned level=peak>=8192?32767:peak*4;
    const unsigned previous=fret_level[b->slot];
    const unsigned fading=previous-(previous>>5);
    fret_level[b->slot]=level>fading?level:fading;
    ++fret_activity_stamp[b->slot];
}
