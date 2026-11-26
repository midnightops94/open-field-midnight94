#pragma once
#include "fixed.hpp"

namespace sag_audio {
using namespace fixed;
constexpr int attack=172037, transition=25343;
constexpr int dc=19112, overloadThreshold=q(.48);
inline int follow(int value,int target,int rate) {
    const int delta=target-value;
    if(delta==0)return value;
    const int step=mul(delta,rate);
    return value+(step?step:1);
}
inline int soft(int v){return ratio(v,one+absq(v));}

inline int circuit_drive(int drive) {
    // Keep the v7 lower curve and leave demand and output makeup on their
    // existing drive mapping. The new knee lifts the last quarter further.
    const int upper=mag_max(0,drive-q(8));
    const int maximum=mag_max(0,drive-q(11));
    return drive+2*upper+3*maximum;
}

inline int diode_clip(int signal) {
    // Find the first sample that would reach the existing clip using exact
    // fixed-point coefficients. Bound the input before the product can wrap.
    constexpr int limit=q(.87);
    constexpr int slope=q(1.36);
    constexpr int edge=1+int((static_cast<long long>(limit)*one-1)/slope);
    if(signal>=edge) return limit;
    if(signal<=-edge) return -limit;
    return mul(slope,signal);
}

inline int starved_shape(int signal,int bias) {
    // Preserve the v7 full and half-scale calculations. Use quarter-scale
    // coordinates only when the new gain can exceed half-scale headroom.
    const int magnitude=absq(signal);
    const int shift=(magnitude>q(29))+(magnitude>q(59));
    const int shaped=mul(q(4.2)>>shift,signal)+(bias>>shift);
    return ratio(shaped,(one>>shift)+absq(shaped));
}

inline int torn_shape(int signal) {
    return 4*clamp(product<26>(q(3.4),signal),q(-.17),q(.1325));
}
struct Kernel {
    int drive,supply,filter,release,stress;
    int driveTo,supplyTo,filterTo,releaseTo,stressTo;
    int valve,diode,valveTo,diodeTo;
    int mix,mixTo,mixStep,mixLeft;
    int charge,heat,level,overload,low[2],offset[2];
    int deepSag,deepSagTo,retryCharge,retryFade;
    bool started,tripped;
    int protection(int overload) {
        // Resettable supply protection. The wet channel retries under
        // sustained overload, with Sag's recovery choice setting dead time.
        const int depth=clamp(2*(overload-overloadThreshold),0,q(.97));
        if(depth) {
            retryCharge+=tripped?-(4608+(release>>1)):5600+(depth>>11);
            if(retryCharge>=one) {
                retryCharge=one;
                tripped=true;
            } else if(retryCharge<=0) {
                retryCharge=0;
                tripped=false;
            }
        } else {
            retryCharge=0;
            tripped=false;
        }
        retryFade=blend(retryFade,tripped?depth:0,q(.007));
        return one-retryFade;
    }

    void targets(const int controls[4],unsigned mode) {
        const int d=q15_to_unit(controls[0]),s=q15_to_unit(controls[1]),t=q15_to_unit(controls[2]);
        driveTo=control_table(curves::drive,d);
        supplyTo=control_table(curves::supply,s);
        filterTo=control_table(curves::tone,t);
        static const int recovery[]={3929,21122,352};
        releaseTo=recovery[(mode>>2)&3];
        const int highSag=within(mul(s-q(.64),q(1/.36)));
        const int highDrive=within(mul(d-q(.72),q(1/.28)));
        deepSagTo=mul(q(.78),highSag);
        stressTo=mul(highDrive,highSag);
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
            const int shaped=mul(q(.85),diode_clip(signal))+mul(q(.15),soft(signal));
            clean+=mul(diode,shaped);
        }
        const int starved=one-valve-diode;
        if(starved) {
            const int magnitude=absq(sample);
            const int gate=ratio(magnitude,magnitude+q(.028)+mul(q(.20),invVoltage));
            const int cone=starved_shape(signal,starveBias)-starveZero;
            clean+=mul(starved,mul(mul(q(.9),gate),cone));
        }
        const int shaped=mul(amplitude,blend(clean,torn_shape(signal),tear));
        low[c]=blend(low[c],shaped,presence);
        offset[c]=blend(offset[c],low[c],dc);
        return low[c]-offset[c];
    }
    void process(int* left,int* right,unsigned frames) {
        int peak=0,lastOver=0;
        for(unsigned i=0;i<frames;++i) {
            drive=follow(drive,driveTo,transition);supply=follow(supply,supplyTo,transition);
            filter=follow(filter,filterTo,transition);release=follow(release,releaseTo,transition);
            deepSag=follow(deepSag,deepSagTo,transition);
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
            const int gain=mul(circuit_drive(drive),voltageGain);
            int amplitude=mul(q(.25)+mul(q(.75),mul(voltage,voltage)),
                              control_table(curves::compensation,(drive-one)>>5));
            if(deepSag) {
                const int collapse=mul(deepSag,within(2*(invVoltage-q(.44))));
                amplitude=mul(amplitude,one-collapse);
            }
            if(lastOver>overloadThreshold || retryCharge || retryFade)
                amplitude=mul(amplitude,protection(lastOver));
            const int presence=mul(mul(filter,voltageGain),one-mul(q(.52),lastOver));
            const int bias=q(.09)+mul(q(.23),invVoltage), sbias=q(.13)+mul(q(.26),invVoltage);
            const int zero=valve?soft(bias):0,szero=one-valve-diode?soft(sbias):0;
            const int tear=mul(q(.62),lastOver);
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
