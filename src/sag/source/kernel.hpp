#pragma once
#include "fixed.hpp"

namespace sag_audio {
using namespace fixed;
constexpr int attack=172037, transition=25343;
constexpr int dc=19112;
inline int follow(int value,int target,int rate) {
    const int delta=target-value;
    if(delta==0)return value;
    const int step=mul(delta,rate);
    return value+(step?step:1);
}
inline int soft(int v){return ratio(v,one+absq(v));}
struct Kernel {
    int drive,supply,filter,release,stress;
    int driveTo,supplyTo,filterTo,releaseTo,stressTo;
    int valve,diode,valveTo,diodeTo;
    int mix,mixTo,mixStep,mixLeft;
    int charge,heat,level,overload,low[2],offset[2];
    bool started;
    void targets(const int controls[4],unsigned mode) {
        const int d=q15_to_unit(controls[0]),s=q15_to_unit(controls[1]),t=q15_to_unit(controls[2]);
        driveTo=control_table(curves::drive,d);
        supplyTo=control_table(curves::supply,s);
        filterTo=control_table(curves::tone,t);
        static const int recovery[]={3929,21122,352};
        releaseTo=recovery[(mode>>2)&3];
        stressTo=mul(mul(within(mul(d-q(.76),q(1/.24))),within(mul(s-q(.68),q(1/.32)))),
                     within(mul(t-q(.73),q(1/.27))));
        valveTo=(mode&3)==0?one:0;
        diodeTo=(mode&3)==1?one:0;
        const int target=q15_to_unit(controls[3]);
        if(!started){mix=mixTo=target;mixLeft=0;}
        else if(target!=mixTo) {
            mixTo=target;mixLeft=529;
            mixStep=product<30>(target-mix,2029758); // 1/529 in Q30
        }
    }
    void initialize(const int controls[4],unsigned mode) {
        // The approved worklet constructs on the default circuit, then applies
        // the supplied settings. Filters and circuit selection glide from it.
        *this={};
        static const int defaults[]={11141,15073,17367,32767};
        targets(defaults,0);
        drive=driveTo;supply=supplyTo;filter=filterTo;release=releaseTo;stress=stressTo;
        valve=one;targets(controls,mode);started=true;
    }
    int channel(unsigned c,int sample,int invVoltage,int amplitude,int presence,int gain,int tear,
                int valveBias,int valveZero,int starveBias,int starveZero) {
        const int signal=mul(sample,gain);
        int clean=0;
        if(valve) {
            const int shaped=mul(q(1.28),soft(signal+valveBias)-valveZero);
            clean=mul(valve,shaped);
        }
        if(diode) {
            const int shaped=mul(q(.85),clamp(mul(q(1.36),signal),-q(.87),q(.87)))+mul(q(.15),soft(signal));
            clean+=mul(diode,shaped);
        }
        const int starved=one-valve-diode;
        if(starved) {
            const int magnitude=absq(sample);
            const int gate=ratio(magnitude,magnitude+q(.028)+mul(q(.20),invVoltage));
            const int cone=soft(mul(q(4.2),signal)+starveBias)-starveZero;
            clean+=mul(starved,mul(mul(q(.9),gate),cone));
        }
        const int torn=clamp(mul(q(3.4),signal),-q(.68),q(.53));
        const int shaped=mul(amplitude,blend(clean,torn,tear));
        low[c]=blend(low[c],shaped,presence);
        offset[c]=blend(offset[c],low[c],dc);
        return low[c]-offset[c];
    }
    void process(int* left,int* right,unsigned frames) {
        int peak=0,lastOver=0;
        for(unsigned i=0;i<frames;++i) {
            drive=follow(drive,driveTo,transition);supply=follow(supply,supplyTo,transition);
            filter=follow(filter,filterTo,transition);release=follow(release,releaseTo,transition);
            stress=follow(stress,stressTo,transition);valve=follow(valve,valveTo,transition);
            diode=follow(diode,diodeTo,transition);
            if(mixLeft){mix+=mixStep;if(!--mixLeft)mix=mixTo;}
            const int l=from_pcm(left[i]),r=from_pcm(right[i]);
            const int input=mag_max(absq(l),absq(r));
            peak=mag_max(peak,input);
            const int demand=within(mul(input,q(1.18)+mul(q(.36),drive)));
            charge=blend(charge,demand,demand>charge?attack:release);
            lastOver=mul(stress,charge);
            const int invVoltage=mul(supply,charge),voltage=one-invVoltage;
            const int voltageGain=q(.42)+mul(q(.58),voltage);
            const int gain=mul(drive,voltageGain);
            const int amplitude=mul(q(.25)+mul(q(.75),mul(voltage,voltage)),
                                    control_table(curves::compensation,(drive-one)>>5));
            const int presence=mul(mul(filter,voltageGain),one-mul(q(.52),lastOver));
            const int bias=q(.09)+mul(q(.23),invVoltage), sbias=q(.13)+mul(q(.26),invVoltage);
            const int zero=valve?soft(bias):0,szero=one-valve-diode?soft(sbias):0;
            const int tear=mul(q(.46),lastOver);
            const int wl=channel(0,l,invVoltage,amplitude,presence,gain,tear,bias,zero,sbias,szero);
            const int wr=channel(1,r,invVoltage,amplitude,presence,gain,tear,bias,zero,sbias,szero);
            if(mix){left[i]=to_pcm(blend(l,wl,mix));right[i]=to_pcm(blend(r,wr,mix));}
        }
        overload=mul(lastOver,mix);
        // External worklet meter is updated once per render quantum.
        if(frames==64)heat=blend(heat,overload,overload>heat?q(.026042):q(.002588));
        level=peak;
    }
};
}
