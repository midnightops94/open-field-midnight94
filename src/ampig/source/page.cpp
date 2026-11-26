#include "effect.hpp"
#include "effect_view.hpp"

namespace {
using namespace effect_view;

constexpr unsigned outline_steps=12, wet_steps=6, dry_steps=7, tone_steps=6, wave_segments=6;
Page page;
volatile unsigned shown_context;
const char* const labels[]={"drive","sag","tone","mix"};
const char* const titles[]={"circuit","recover"};
const char* const modes[][3]={{"valve","diode","starved"},{"flow","fast","slow"}};

int lerp(int first,int last,int amount) {
    return first+mul(last-first,amount);
}

int ease(int current,int target,unsigned elapsed,unsigned time_constant) {
    const int difference=target-current;
    if(!difference || !elapsed) return current;
    const int distance=difference<0?-difference:difference;
    const int step=distance*int(elapsed)/int(elapsed+time_constant);
    return current+(difference<0?-1:1)*(step?step:1);
}

// Phase in 65536ths of a revolution. Interpolate the existing fixed-point
// sine table so smooth encoder motion doesn't produce 5.6-degree jumps.
int wave_sine(unsigned phase) {
    const unsigned sample=phase>>10;
    return lerp(sine(sample),sine(sample+1),int(phase&1023u));
}

void rotate_knob(void* knob,int x,int y,int value) {
    const unsigned angle=unsigned(lerp(-21845,21845,value));
    const int s=wave_sine(angle),c=wave_sine(angle+16384u);
    move(knob,x-mul(x,c)+mul(y,s),y-mul(x,s)-mul(y,c),c,s,-s,c);
}

// Both paths have control points at this baseline. Keeping that baseline
// fixed lets their bowed center move while the shoulders and feet stay put.
void bow(void* part,int baseline,int size,int span) {
    int sy=size/span;
    if(!sy) sy=1;
    scale(part,0,px(baseline),one,sy);
}

struct Roadie {
    Atlas<outline_steps> body,back_ink,belly_ink,handle_ink;
    Atlas<wet_steps> wet,rail_ink,rail_bend_ink;
    Atlas<dry_steps> dry,dry_bend_ink;
    Atlas<tone_steps> left_ink,right_ink;
    Atlas<3> circuits;
    void *back=nullptr,*belly=nullptr,*handle=nullptr,*deck=nullptr,*tone_l=nullptr,*tone_r=nullptr;
    void *cone=nullptr,*drive_knob=nullptr,*tone_knob=nullptr,*rail=nullptr,*rail_bend=nullptr;
    void *dry_bend=nullptr,*dry_pulse=nullptr,*tick_a=nullptr,*tick_b=nullptr;
    void* wave[wet_steps][wave_segments]{};
    int controls[4]{},activity=0,load=0,heat=0;
    int last_d=-1,last_t=-1,last_drop=-32768,last_belly=-32768,last_peak=-1;
    int last_g=-1,last_excursion=-1,last_w=-32768,last_dry_pulse=-1;
    int last_wet=-1,last_circuit=-1;
    unsigned place=~0u,stamp=0,phase=0;
    bool active=false,was_off=false;

    constexpr Roadie() = default;

    void bind(void* doc) {
        body.bind(doc,'o'); back_ink.bind(doc,'b'); belly_ink.bind(doc,'e'); handle_ink.bind(doc,'h');
        wet.bind(doc,'w'); rail_ink.bind(doc,'r'); rail_bend_ink.bind(doc,'n');
        dry.bind(doc,'y'); dry_bend_ink.bind(doc,'j');
        left_ink.bind(doc,'l'); right_ink.bind(doc,'f'); circuits.bind(doc,'c');
        back=op1f_node(doc,"rBack"); belly=op1f_node(doc,"rBelly"); handle=op1f_node(doc,"rHandle");
        deck=op1f_node(doc,"rDeck"); tone_l=op1f_node(doc,"rToneL"); tone_r=op1f_node(doc,"rToneR");
        cone=op1f_node(doc,"cone"); drive_knob=op1f_node(doc,"drive-knob"); tone_knob=op1f_node(doc,"tone-knob");
        rail=op1f_node(doc,"rRail"); rail_bend=op1f_node(doc,"rRailBend");
        dry_bend=op1f_node(doc,"rDryBend"); dry_pulse=op1f_node(doc,"rDryPulse");
        node_visible(dry_pulse,false);
        tick_a=op1f_node(doc,"rTickA"); tick_b=op1f_node(doc,"rTickB");
        for(unsigned i=0;i<wet_steps;++i)
            for(unsigned j=0;j<wave_segments;++j) {
                const char id[]={'u',char('0'+i),char('0'+j),0};
                wave[i][j]=op1f_node(doc,id);
            }
    }

    void shape_cabinet(int sag,int pressure) {
        const int drop=-px(3)+10*sag+4*mul(sag,pressure);
        const int belly_y=px(142)+18*sag+6*mul(sag,pressure);
        if(drop!=last_drop || belly_y!=last_belly) {
            last_drop=drop;
            last_belly=belly_y;
            bow(back,67,drop,10);
            bow(belly,152,belly_y-px(152),19);
            move(handle,0,drop);
            const int squeeze=mul(63,sag);
            move(deck,0,drop+mul(74240,squeeze),one,0,0,one-squeeze);
            move(rail,0,drop/2);
            const int arc=1+(8*sag+8*mul(sag,pressure))/16;
            move(rail_bend,0,drop/2+px(134)-134*arc,one,0,0,arc);
            bow(dry_bend,141,belly_y-px(145),21);
        }
    }

    void louvres(void* group,int center,int max_shear,int tone) {
        const int sx=lerp(164,one,tone),sy=lerp(508,one,tone);
        const int shear=mul(max_shear,one-tone);
        move(group,center-mul(center,sx),px(105)-mul(px(105),sy)-mul(center,shear),
             sx,shear,0,sy);
    }

    void shape_tone(int tone) {
        if(tone==last_t) return;
        last_t=tone;
        const int shade=int(step(tone,tone_steps-1));
        left_ink.select(shade); right_ink.select(shade);
        louvres(tone_l,255181,-195,tone);
        louvres(tone_r,px(333),559,tone);
        rotate_knob(tone_knob,260506,82432,tone);
    }

    void shape_wave(int power,unsigned circuit,unsigned wet_ink) {
        const int phase_sample=power?int(phase>>8):-1;
        if(power==last_g && phase_sample==last_w && int(wet_ink)==last_wet &&
           int(circuit)==last_circuit) return;
        last_g=power; last_w=phase_sample; last_wet=int(wet_ink); last_circuit=int(circuit);
        // The same six segments make a soft, clipped, or interrupted trace.
        // The sine window holds both endpoints at their cabinet contacts.
        constexpr int window[]={0,512,887,1024,887,512,0};
        constexpr int centers[]={113152,113152,113152,112528,110444,108032,108032};
        int previous=centers[0];
        for(unsigned i=0;i<wave_segments;++i) {
            const unsigned next=i+1;
            const unsigned at=next*10923u;
            int form=wave_sine(at-phase);
            if(circuit==1) {
                form*=3;
                if(form>one) form=one;
                if(form<-one) form=-one;
            } else if(circuit==2) {
                int gate=wave_sine(phase+next*8391u);
                gate=mul(gate,gate);
                form=mul(form,gate);
            }
            const int end=centers[next]-mul(mul(form,window[next]),power);
            move(wave[wet_ink][i],px(156)+7680*int(i),previous,
                 one,(end-previous)*2/15,0,one);
            previous=end;
        }
    }

    void draw(const unsigned (&values)[4],unsigned where,unsigned ms,bool off) {
        if(!deck) return;
        const bool restart=!active || place!=where || ms-stamp>250;
        const unsigned elapsed=restart?0:ms-stamp;
        stamp=ms;
        place=where;
        active=true;
        for(unsigned i=0;i<4;++i) {
            const int target=unit(values[i]);
            controls[i]=restart?target:ease(controls[i],target,elapsed,64);
        }
        const int level=off?0:unit(ampig_meter(where,2));
        const int demand=off?0:unit(ampig_meter(where,0));
        const int stress=off?0:unit(ampig_meter(where,1));
        activity=restart || off || was_off?level:ease(activity,level,elapsed,level>activity?25:116);
        load=restart || off || was_off?demand:ease(load,demand,elapsed,demand>load?35:82);
        heat=restart || off || was_off?stress:ease(heat,stress,elapsed,78);
        was_off=off;
        if(activity>5) phase+=elapsed*483u;

        const int d=controls[0],s=controls[1],t=controls[2],m=controls[3];
        const int motion=mul(activity,lerp(164,one,d));
        const int hot=mul(d,mul(m,activity));
        const int ink=int(step(d+mul(384,hot),8));
        body.select(ink); back_ink.select(ink); belly_ink.select(ink); handle_ink.select(ink);
        shape_cabinet(s,mul(clamp_unit(load+load/2),m));
        shape_tone(t);
        if(d!=last_d) {
            last_d=d;
            rotate_knob(drive_knob,247296,82432,d);
        }
        const int excursion=mul(189,mul(mul(m,motion),wave_sine(phase)));
        if(excursion!=last_excursion) {
            last_excursion=excursion;
            scale(cone,301261,107008,one+excursion,one+mul(829,excursion));
        }
        const unsigned wet_ink=step(m,wet_steps-1);
        wet.select(int(wet_ink)); rail_ink.select(int(wet_ink)); rail_bend_ink.select(int(wet_ink));
        const int dry_ink=int(step(m,dry_steps-1));
        dry.select(dry_ink); dry_bend_ink.select(dry_ink);
        const bool dry_moving=activity>48 && dry_ink<int(dry_steps-1);
        if(int(dry_moving)!=last_dry_pulse) {
            last_dry_pulse=dry_moving;
            node_visible(dry_pulse,dry_moving);
        }
        if(dry_moving) {
            const int progress=int((phase>>6)&1023u);
            const int dip=mul(mul(progress,one-progress)*4,last_belly-px(145));
            move(dry_pulse,px(218)+75*progress,px(141)+dip);
        }
        const unsigned circuit=ampig_mode(where,0);
        circuits.select(int(circuit));
        const int peak=1638+mul(2867,d)+5*motion+heat;
        if(peak!=last_peak) {
            last_peak=peak;
            move(tick_a,0,px(98),one,0,0,-peak/5);
            move(tick_b,0,px(98),one,0,0,-mul(850,peak)/5);
        }
        shape_wave(mul(1536+13*d,motion),circuit,wet_ink);
    }
};

Roadie roadie;
}

extern "C" void ampig_shown(unsigned place,unsigned how) {
    shown_context=(place<<3)|(how&7);
    ampig_show_state(place,how);
}

extern "C" void ampig_frame(const Op1fFrame* frame) {
    if(!frame || frame->bytes<40) return;
    ampig_settings_frame(frame->ms);
    const unsigned shown=shown_context,place=shown>>3;
    if(!(shown&OP1F_SHOWN_UP) || frame->disk || frame->page!=&op1f_card_ampig || !frame->canvas || place>OP1F_PLACE_MASTER) return;
    if(page.setup(&op1f_card_ampig,"ampig_off.svg","ampig_p3.svg")) roadie.bind(page.document);
    page.place(&op1f_card_ampig);
    const bool off=shown&OP1F_SHOWN_OFF;
    const bool covered=(shown&OP1F_SHOWN_COVERED) || frame->popup;
    unsigned values[4];
    if(Page::values(&op1f_card_ampig,place,values)) {
        roadie.draw(values,place,frame->ms,off);
        page.draw_values(frame->canvas,labels,values);
    }
    const int lane=off || covered?-1:ampig_popup(place,frame->ms);
    const unsigned active_mode=lane<0?0:ampig_mode(place,unsigned(lane));
    page.mode_card(lane,active_mode,lane<0?nullptr:titles[lane],lane<0?nullptr:modes[lane][active_mode]);
    page.bypass(frame->canvas,off && !covered);
}
