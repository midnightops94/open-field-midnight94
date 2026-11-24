#pragma once
#include "fixed.hpp"
namespace split_audio {
using namespace fixed;
inline int average(int x,int y){return (x>>1)+(y>>1)+(x&y&1);}
inline int follow(int a,int b,int rate){
    int delta=b-a;
    if(!delta)return a;
    int step=mul(delta,rate);
    return a+(step?step:1);
}
inline int round_value(int x){
    x=clamp(x,-one,one);
    return mul(x,one-mul(mul(x,x),q(1./3)));
}
inline int round_area(int x){
    int a=absq(x);
    if(a>=one)return mul(q(2./3),a)-q(.25);
    int square=mul(a,a);
    return mul(square,q(.5)-mul(square,q(1./12)));
}
inline int clip_area(int x){int a=absq(x);return a>=one?a-q(.5):mul(a,a)>>1;}
inline int phase(int x){return int((unsigned(x)+one)&(4u*one-1))-one;}
inline int fold_value(int x){int p=phase(x);return p>one?2*one-p:p;}
inline int fold_area(int x){
    int p=phase(x),half_square=mul(p,p)>>1;
    return p>one?2*p-half_square-one:half_square;
}
template<bool high,bool area> inline int curve(int x,unsigned shape){
    if(high) {
        if(shape)return area?fold_area(x):fold_value(x);
        return area?clip_area(x):clamp(x,-one,one);
    }
    if(!shape)return area?round_area(x):round_value(x);
    const bool neg=x<0;
    const int amplitude=neg?q(.58):q(1.16);
    const int scaled=mul(x,neg?q(1/.58):q(1/1.16));
    return area?mul(neg?q(.58*.58):q(1.16*1.16),round_area(scaled)):mul(amplitude,round_value(scaled));
}
template<bool high,bool area> inline int shape(int x,int mode){
    if(mode<=q(.0001))return curve<high,area>(x,0);
    if(mode>=q(.9999))return curve<high,area>(x,1);
    return blend(curve<high,area>(x,0),curve<high,area>(x,1),mode);
}
template<bool high> inline int band(int now,int previous,int gain,int level,int mode) {
    const int x=mul(now,gain),last=mul(previous,gain),span=x-last;
    // Near a stationary sample the first derivative is more precise than
    // subtracting two nearly equal quantized antiderivatives.
    const int value=absq(span)<q(.003)?shape<high,false>(average(x,last),mode):
        ratio(shape<high,true>(x,mode)-shape<high,true>(last,mode),span);
    return mul(value,level)-average(now,previous);
}
inline int headroom(int dry,int colour){
    const int sum=dry+colour;
    if(absq(sum)<=q(.92) || !colour || ((sum^colour)<0))return sum;
    const int knee=mag_max(q(.92),sum<0?-dry:dry);
    const int available=one-knee;
    if(available<=0)return dry;
    const int excess=absq(sum)-knee;
    const int limited=knee+mul(excess,ratio(available,available+excess));
    return sum<0?-limited:limited;
}
// Both makeup denominators lie in [1,6.46]. The same reciprocal table used by
// ratio() therefore needs only three exponent cases and no numerator multiply.
inline int inverse_makeup(int den){
    const unsigned exponent=den>=4*one?2u:den>=2*one?1u:0u;
    const unsigned index=(unsigned(den)>>exponent)-one,i=index>>16;
    const int reciprocal=curves::inverse[i]+product<16>(curves::inverse[i+1]-curves::inverse[i],int(index&65535));
    return reciprocal>>(6+exponent);
}
struct Channel {int pole,second,prevLow,prevHigh,dcIn,dcOut;};
struct Kernel {
    int values[4],goal[4],modes[4],modeTo[4],coeff,coeffTo,level;
    Channel channels[2];
    void targets(const int raw[4],unsigned mode) {
        for(unsigned i=0;i<4;++i){goal[i]=q15_to_unit(raw[i]);modeTo[i]=(mode&(1u<<(i*2)))?one:0;}
        coeffTo=control_table(curves::crossover,goal[0]);
    }
    void initialize(const int raw[4],unsigned mode){
        *this={};targets(raw,mode);
        for(unsigned i=0;i<4;++i){values[i]=goal[i];modes[i]=modeTo[i];}
        coeff=coeffTo;
    }
    void process(int* left,int* right,unsigned frames){
        constexpr int smooth=25343,dcPole=16758104;
        int peak=0;
        const bool glideModes = modes[0]!=modeTo[0] || modes[1]!=modeTo[1] ||
                                modes[2]!=modeTo[2] || modes[3]!=modeTo[3];
        for(unsigned i=0;i<frames;++i){
            // Crossover itself is smoothed in coefficient space by the approved
            // kernel. Its raw control never participates in the audio path.
            for(unsigned k=1;k<4;++k)values[k]=follow(values[k],goal[k],smooth);
            if(glideModes)for(unsigned k=0;k<4;++k)modes[k]=follow(modes[k],modeTo[k],smooth);
            coeff=follow(coeff,coeffTo,smooth);
            const int la=values[1],ha=values[2],mix=values[3];
            const int lg=one+18*mul(la,la),hg=one+mul(22*one+20*modes[2],mul(ha,ha));
            const int ll=inverse_makeup(one+mul(lg-one,q(.12))),hl=inverse_makeup(one+mul(hg-one,q(.13)));
            const int focus=mul(modes[0],q(.8));
            int* out[2]={left+i,right+i};
            for(unsigned c=0;c<2;++c) {
                Channel& s=channels[c];
                int dry=from_pcm(*out[c]);peak=mag_max(peak,absq(dry));
                s.pole=blend(s.pole,dry,coeff);s.second=blend(s.second,s.pole,coeff);
                const int low=blend(s.pole,s.second,focus),high=dry-low;
                const int ld=la>q(.00001)?mul(la,band<false>(low,s.prevLow,lg,ll,modes[1])):0;
                const int hd=ha>q(.00001)?mul(ha,band<true>(high,s.prevHigh,hg,hl,modes[2])):0;
                const int delta=ld+hd;
                const int colour=delta-s.dcIn+mul(dcPole,s.dcOut);
                s.dcIn=delta;s.dcOut=colour;s.prevLow=low;s.prevHigh=high;
                // Preserve every Q31 input bit on both a dry mix and dry unity.
                if(mix && (colour || modes[3])){
                    const int normal=modes[3]<one?headroom(dry,colour):0;
                    const int wet=modes[3]?blend(normal,headroom(0,colour),modes[3]):normal;
                    *out[c]=to_pcm(blend(dry,wet,mix));
                }
            }
        }
        level=peak;
    }
};
}
