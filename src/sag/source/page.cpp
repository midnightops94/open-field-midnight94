#include "effect.hpp"
#include "effect_view.hpp"

namespace {
using namespace effect_view;
Page page;
volatile unsigned shown_context;
const char* const labels[]={"drive","sag","tone","mix"};
const char* const title[]={"circuit","recover"};
const char* const modes[][3]={{"valve","diode","starved"},{"flow","fast","slow"}};
Atlas<9> tones,routes;
Atlas<8> ruptures;
Atlas<3> circuits,recovery;
void *head,*cabinet,*cone,*crack,*drive_blip,*lamp,*fader,*output_level;
void* coil[8];
unsigned previous[4],last_demand,last_heat,last_level,previous_mode=~0u;
bool articulated;

void bind() {
    void* doc=page.document;
    head=op1f_node(doc,"sHead");
    cabinet=op1f_node(doc,"sCabinet");
    cone=op1f_node(doc,"sTone");
    crack=op1f_node(doc,"sCrack");
    drive_blip=op1f_node(doc,"sDrive");
    lamp=op1f_node(doc,"sLamp");
    fader=op1f_node(doc,"sFader");
    output_level=op1f_node(doc,"sOutput");
    for(unsigned i=0;i<8;++i) {
        const char id[]={'s','C','o','i','l',char('0'+i),0};
        coil[i]=op1f_node(doc,id);
    }
    tones.bind(doc,'t');
    routes.bind(doc,'r');
    ruptures.bind(doc,'u');
    circuits.bind(doc,'d');
    recovery.bind(doc,'c');
}

void draw_art(const unsigned (&values)[4],unsigned place,unsigned ms,bool off) {
    const unsigned heat=off?0:sag_meter(place,1);
    const unsigned demand=off?0:sag_meter(place,0);
    const unsigned signal=off?0:sag_meter(place,2);
    const unsigned mode0=sag_mode(place,0),mode1=sag_mode(place,1);
    const unsigned mode=(mode0<<4)|mode1;
    const int d=unit(values[0]),s=unit(values[1]),t=unit(values[2]),m=unit(values[3]);
    const int warmth=unit(heat);
    const int rupture=clamp_unit((warmth-440)*one/481);
    if(!articulated || previous_mode!=mode || previous[0]!=values[0] ||
       previous[1]!=values[1] || previous[2]!=values[2] || previous[3]!=values[3] ||
       last_demand!=demand || last_heat!=heat || last_level!=signal || rupture) {
        articulated=true;
        previous_mode=mode;
        last_demand=demand;
        last_heat=heat;
        last_level=signal;
        for(unsigned i=0;i<4;++i) previous[i]=values[i];
        tones.select(int(step(t,8)));
        routes.select(int(step(m,8)));
        ruptures.select(rupture?((rupture-1)*8)>>10:-1);
        circuits.select(int(mode0));
        recovery.select(int(mode1));
        node_visible(crack,rupture!=0);
        move(fader,67*(m-one),0);
        const int energy=mul(unit(signal),m);
        scale(cone,px(274),px(134),one+energy/16,one+energy/16);
        scale(output_level,px(336),px(128),one+mul(energy,461),one+mul(energy,461));
        scale(lamp,px(347),px(67),one+warmth,one+warmth);
        scale(drive_blip,px(126),px(77),one,(px(3)+17*d)/20);

        const int coil_y=px(65)+mul(s,px(6)+9*mul(unit(demand),m));
        int x=px(198),y=px(61);
        for(unsigned i=0;i<8;++i) {
            const int next_x=i==7?px(257):px(210+6*int(i));
            const int next_y=i==7?px(61):coil_y+px((i&1)?1:-7);
            move(coil[i],x,y,next_x-x,next_y-y);
            x=next_x;
            y=next_y;
        }
        const int shake=mul(12,mul(rupture,sine((ms*83u)>>10)));
        move(cabinet,shake*176,-shake*241,one,shake,-shake,one);
        const int lift=mul(-89,rupture);
        move(head,lift*61-7*rupture,-lift*241-13*rupture,one,lift,-lift,one);
    }
}
}

extern "C" void sag_shown(unsigned place,unsigned how) {
    shown_context=(place<<3)|(how&7);
    sag_show_state(place,how);
}

extern "C" void sag_frame(const Op1fFrame* frame) {
    if(!frame || frame->bytes<40) return;
    sag_settings_frame(frame->ms);
    const unsigned shown=shown_context,place=shown>>3;
    if(!(shown&OP1F_SHOWN_UP) || frame->disk || frame->page!=&op1f_card_sag || !frame->canvas || place>OP1F_PLACE_MASTER) return;
    if(page.setup(&op1f_card_sag,"sag_off.svg","sag_p3.svg")) bind();
    page.place(&op1f_card_sag);
    const bool off=shown&OP1F_SHOWN_OFF;
    const bool covered=(shown&OP1F_SHOWN_COVERED) || frame->popup;
    unsigned values[4];
    if(Page::values(&op1f_card_sag,place,values)) {
        draw_art(values,place,frame->ms,off);
        page.draw_values(frame->canvas,labels,values);
    }
    const int lane=off || covered?-1:sag_popup(place,frame->ms);
    const unsigned active_mode=lane<0?0:sag_mode(place,unsigned(lane));
    page.mode_card(frame->canvas,lane,active_mode,lane<0?nullptr:title[lane],lane<0?nullptr:modes[lane][active_mode]);
    page.bypass(frame->canvas,off && !covered);
}
