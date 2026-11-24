#include "effect.hpp"
#include "effect_view.hpp"

namespace {
using namespace effect_view;
Page page;
volatile unsigned shown_context;
const char* const labels[]={"crossover","low drive","high drive","mix"};
const char* const modes[][2]={
    {"soft","focus"},{"round","tube"},{"clip","fold"},{"blend","delta"}
};
Atlas<36> upper_paths,lower_paths;
Atlas<2> high_modes,low_modes,fork_modes,aperture_modes;
void *source,*dry,*prism,*high,*low,*pulse,*wet,*aperture;
unsigned previous[4],last_mode=~0u,last_energy;
bool articulated;

void bind() {
    void* doc=page.document;
    source=op1f_node(doc,"pSource");
    dry=op1f_node(doc,"pDry");
    prism=op1f_node(doc,"pPrism");
    high=op1f_node(doc,"pHigh");
    low=op1f_node(doc,"pLow");
    pulse=op1f_node(doc,"pPulse");
    wet=op1f_node(doc,"pWet");
    aperture=op1f_node(doc,"pAperture");
    upper_paths.bind(doc,'h');
    lower_paths.bind(doc,'l');
    high_modes.bind(doc,'a');
    low_modes.bind(doc,'b');
    fork_modes.bind(doc,'f');
    aperture_modes.bind(doc,'x');
}

void draw_art(const unsigned (&values)[4],unsigned place,unsigned ms,bool off) {
    const unsigned focus=split_mode(place,0),tube=split_mode(place,1);
    const unsigned fold=split_mode(place,2),delta=split_mode(place,3);
    const unsigned modes=focus|(tube<<1)|(fold<<2)|(delta<<3);
    const int color=unit(values[0]),bottom=unit(values[1]),top=unit(values[2]),mix=unit(values[3]);
    const int signal=off?0:unit(split_meter(place,0));
    const int energy=mul(signal,614+mul(410,sine((ms*125u)>>10)));
    if(articulated && last_mode==modes && last_energy==unsigned(energy) && previous[0]==values[0] &&
        previous[1]==values[1] && previous[2]==values[2] && previous[3]==values[3]) return;
    articulated=true;
    last_mode=modes;
    last_energy=unsigned(energy);
    for(unsigned i=0;i<4;++i) previous[i]=values[i];
    const int column=int(step(color,8));
    upper_paths.select(column*4+2*int(focus)+int(fold));
    lower_paths.select(column*4+2*int(focus)+int(tube));
    high_modes.select(int(fold));
    low_modes.select(int(tube));
    fork_modes.select(int(focus));
    aperture_modes.select(int(delta));
    const int dx=64*color-25559;
    move(prism,dx,0);
    scale(source,px(53),0,one+dx/74,one);
    scale(dry,px(438),0,one-dx/220,one);
    node_visible(dry,mix<one);
    move(high,px(288),px(67-8*int(focus))-12*color,(px(8)+10*top)*10/121,0,0,one);
    move(low,px(288),px(160+8*int(focus))-10*color,(px(10)+12*bottom)*100/1432,0,0,one);
    const int activity=one+mul(709,energy);
    scale(pulse,px(53),px(110),activity,activity);
    const int radius=(px(3)+7*mix)*100/888;
    scale(aperture,px(372),px(110),delta?one:radius,radius);
    scale(wet,px(438),px(110),one+(6021-7*mix)/57,one);
    node_visible(wet,mix!=0);
}
}

extern "C" void split_shown(unsigned place,unsigned how) {
    shown_context=(place<<3)|(how&7);
    split_show_state(place,how);
}

extern "C" void split_frame(const Op1fFrame* frame) {
    if(!frame || frame->bytes<40) return;
    split_settings_frame(frame->ms);
    const unsigned shown=shown_context,place=shown>>3;
    if(!(shown&OP1F_SHOWN_UP) || frame->disk || frame->page!=&op1f_card_split || !frame->canvas || place>OP1F_PLACE_MASTER) return;
    if(page.setup(&op1f_card_split,"split_off.svg","split_p3.svg")) bind();
    page.place(&op1f_card_split);
    const bool off=shown&OP1F_SHOWN_OFF;
    const bool covered=(shown&OP1F_SHOWN_COVERED) || frame->popup;
    unsigned values[4];
    if(Page::values(&op1f_card_split,place,values)) {
        draw_art(values,place,frame->ms,off);
        page.draw_values(frame->canvas,labels,values);
    }
    const int lane=off || covered?-1:split_popup(place,frame->ms);
    const unsigned active_mode=lane<0?0:split_mode(place,unsigned(lane));
    page.mode_card(frame->canvas,lane,active_mode,lane<0?nullptr:labels[lane],lane<0?nullptr:modes[lane][active_mode]);
    page.bypass(frame->canvas,off && !covered);
}
