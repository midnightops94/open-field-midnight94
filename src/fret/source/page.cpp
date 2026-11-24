#include "build_r6e_card.h"
#include "engine_turns.hpp"

extern "C" {
extern const Op1fCard op1f_card_fret OP1F_NAMED("op1f_card_fret");
extern volatile unsigned fret_level[8], fret_activity_stamp[8];
void fret_frame(const Op1fFrame*) OP1F_NAMED("fret_frame");
void fret_shown(unsigned,unsigned) OP1F_NAMED("fret_shown");
int fret_input(const Op1fInput*) OP1F_NAMED("fret_input");
}

namespace {
#include "picture_tables.inc"
constexpr const char* captions[]={"hand","position","mute","amp"};
constexpr const char* popup_names[]={"strings","pickup","mute","amp"};
constexpr const char* choices[4][3]={
    {"roundwound","flatwound","steel"},{"single coil","split coil","twin coil"},
    {"palm","foam","gate"},{"di","tube","fuzz"}};
constexpr int stored_pick[]={0,16384,32767};
constexpr unsigned mode_hold_ms=1024;


EngineTurns turns{};
void* picture;
bool tried, stood;
void* hands[4];
void* pickup[3];
void* damper[3];
void* strings[4][2];
void* card_layer;
void* card_lanes[4];
void* string_icons[3];
unsigned hand_on=1, pickup_on=1, damper_style;
int damper_position=-1, string_bend[4];
int displayed[4]={-1,-1,-1,-1};
int pickup_position=16384;
int pickup_matrix[]={1024,0,0,0,1024,0,0,0,1024};
unsigned popup_axis, popup_deadline;
bool popup_up, popup_visible;
unsigned panel_on=4, choice_on=3;
volatile unsigned slot_on=8, shown_flags;
struct Click {unsigned slot, axis;};
Click pending[16];
volatile unsigned next_click, read_click;
unsigned activity, activity_slot=8, last_stamp, last_activity_ms;

int clip(int n,int low,int high) {return n<low?low:n>high?high:n;}
unsigned bounded(int value) {return unsigned(clip(value,0,32767));}
unsigned mode(unsigned raw) {return raw<8192?0:raw<24576?1:2;}
unsigned hand_style(unsigned raw) {return raw<5243?0:raw<15729?1:raw<26214?2:3;}

void show(void* part,bool visible) {if(part)op1f_show(part,visible);}
void decimal(char* text,unsigned value) {
    if(value==100) {text[0]='1';text[1]='0';text[2]='0';text[3]=0;return;}
    const unsigned tens=(value*205)>>11;
    if(tens)*text++=char('0'+tens);
    *text++=char('0'+value-tens*10);
    *text=0;
}
void bind() {
    char hand[]="frHand0", pick[]="frPickup0", pad[]="frDamper0", line[]="frString00", panel[]="frCard0";
    char material[]="frMaterial0";
    card_layer=op1f_node(picture,"frPopup");
    show(card_layer,false);
    for(unsigned lane=0;lane<4;++lane) {
        hand[6]=panel[6]=char('0'+lane);
        hands[lane]=op1f_node(picture,hand);
        card_lanes[lane]=op1f_node(picture,panel);
        show(hands[lane],lane==hand_on);
        show(card_lanes[lane],false);
        char label[]="frLabel0";
        label[7]=char('0'+lane);
        op1f_text_set(picture,label,captions[lane],0,lane<2?OP1F_LEFT:OP1F_RIGHT);
        line[8]=char('0'+lane);
        for(unsigned side=0;side<2;++side) {
            line[9]=char('0'+side);
            strings[lane][side]=op1f_node(picture,line);
        }
    }
    for(unsigned index=0;index<3;++index) {
        pick[8]=pad[8]=char('0'+index);
        material[10]=char('0'+index);
        pickup[index]=op1f_node(picture,pick);
        damper[index]=op1f_node(picture,pad);
        string_icons[index]=op1f_node(picture,material);
        show(pickup[index],index==pickup_on);
        show(damper[index],index==damper_style);
        show(string_icons[index],false);
    }
}

void controls(unsigned slot,unsigned (&values)[4],unsigned (&modes)[4]) {
    for(unsigned lane=0;lane<4;++lane) {
        int primary=0, secondary=0;
        op1f_param_heard(&op1f_card_fret,0,slot,lane,&primary);
        op1f_param_get(&op1f_card_fret,0,slot,4+lane,&secondary);
        values[lane]=bounded(primary);
        modes[lane]=mode(bounded(secondary));
    }
}

void readouts(const unsigned (&values)[4]) {
    const unsigned hand=hand_style(values[0]);
    if(hand!=hand_on) {
        show(hands[hand_on],false);
        show(hands[hand],true);
        hand_on=hand;
    }
    for(unsigned lane=1;lane<4;++lane) {
        int code=int((values[lane]*100+16384)>>15);
        if(lane==1 && values[lane]<=327)code=101;
        else if(lane==1 && values[lane]>=32440)code=102;
        if(displayed[lane]==code)continue;
        char number[4], id[]="frValue0";
        const char* value=number;
        if(code==101)value="neck";
        else if(code==102)value="bridge";
        else decimal(number,unsigned(code));
        id[7]=char('0'+lane);
        if(op1f_text_set(picture,id,value,0,lane<2?OP1F_LEFT:OP1F_RIGHT)>0)displayed[lane]=code;
    }
}

void sound_activity(unsigned slot,unsigned ms) {
    const unsigned stamp=fret_activity_stamp[slot];
    if(activity_slot!=slot) {
        activity_slot=slot;
        activity=0;
        last_stamp=stamp;
        last_activity_ms=ms-120;
    }
    if(stamp!=last_stamp) {
        last_stamp=stamp;
        last_activity_ms=ms;
    }
    const unsigned target=ms-last_activity_ms<72 ? unsigned(clip(int((fret_level[slot]*7+16)>>5),0,1024)) : 0;
    activity = (3*activity+target+2)>>2;
    if(!target && activity<4) activity=0;
}

void hardware(const unsigned (&values)[4],const unsigned (&modes)[4],unsigned ms) {
    const int position=int(values[1]);
    const unsigned w=modes[1];
    if(pickup_position!=position || pickup_on!=w) {
        // The approved bass art is scaled .72 and rotated -11 degrees. The
        // source pickup travels -45 local units from neck to bridge.
        const int delta=16384-position;
        pickup_matrix[6]=(delta*32568)>>15;
        pickup_matrix[7]=-((delta*6331)>>15);
        if(pickup[w])op1f_move(pickup[w],pickup_matrix);
        if(pickup_on!=w) {
            show(pickup[pickup_on],false);
            show(pickup[w],true);
            pickup_on=w;
        }
        pickup_position=position;
    }
    const int mute=int(values[2]);
    if(modes[2]!=damper_style || mute!=damper_position) {
        const unsigned next=modes[2];
        int matrix[9]={1024,0,0,0,1024,0,(mute*13027)>>15,-((mute*2532)>>15),1024};
        if(damper[next])op1f_move(damper[next],matrix);
        if(next!=damper_style) {
            show(damper[damper_style],false);show(damper[next],true);
            damper_style=next;
        }
        damper_position=mute;
    }
    const unsigned trem=(activity*(1024-((values[2]*870)>>15)))>>10;
    constexpr unsigned rates[]={472,559,647,735};
    constexpr int weights[]={1024,881,737,594};
    constexpr int material_motion[]={8,5,9};
    for(unsigned string=0;string<4;++string) {
        const unsigned phase=((ms*rates[string])>>10)+41*string;
        const int amplitude=(int(trem)*weights[string])>>10;
        const int quarters=(((amplitude*visual_sine[phase&255])>>10)*material_motion[modes[0]]+512)>>10;
        if(quarters==string_bend[string])continue;
        // Both strokes span 106 screen units in x. They shear toward one
        // shared midpoint and keep each outer endpoint fixed.
        const int shear=(quarters*618+128)>>8;
        for(unsigned side=0;side<2;++side) {
            const int slope=side?-shear:shear;
            int matrix[9]={1024,slope,0,0,1024,0,0,-slope*string_pivot[string][side],1024};
            if(strings[string][side])op1f_move(strings[string][side],matrix);
        }
        string_bend[string]=quarters;
    }
}

unsigned ratio(unsigned numerator,unsigned denominator) {
    // The trace is drawn on the display thread. Keep software division out
    // of the runtime imports while matching the authored rational soft clip.
    unsigned bit=1, result=0;
    while(denominator<=numerator/2) {denominator<<=1;bit<<=1;}
    while(bit) {
        if(numerator>=denominator){numerator-=denominator;result|=bit;}
        denominator>>=1;bit>>=1;
    }
    return result;
}
int curve(int dry,int amount,unsigned circuit) {
    const int gain=circuit==0?1946:circuit==1?6144:8192;
    const int driven=(dry*(1024+((gain*amount)>>10)))>>10;
    int coloured;
    if(circuit==2) coloured=(clip(driven,-799,666)*(1024-((246*amount)>>10)))>>10;
    else {
        const unsigned mag=unsigned(driven<0?-driven:driven);
        const unsigned clipped=ratio(mag*1024,1024+((mag*(circuit==0?358u:983u))>>10));
        coloured=driven<0?-int(clipped):int(clipped);
    }
    return dry+(((coloured-dry)*amount)>>10);
}
void wave(void* canvas,unsigned amount,unsigned circuit,unsigned ms) {
    if(!canvas)return;
    const int amp=int((amount+16)>>5);
    const int height=8192+14*amp+7*int(activity);
    const unsigned phase=((ms*154u)>>10)*activity>>10;
    int x0=213,y0=158;
    constexpr unsigned ink=OP1F_COLOUR(0xe9,0x75,0x53);
    for(unsigned point=4;point<=96;point+=4) {
        const unsigned angle=point*8-phase;
        const int dry=(532*visual_sine[angle&255]+108*visual_sine[(angle*2)&255])>>10;
        const int signal=curve(dry,amp,circuit);
        const int trace_height=(height*visual_taper[point])>>10;
        const int y=(162017-(((trace_height*signal)>>10)*738>>10)+512)>>10;
        const int x=(218563+1044*int(point)+512)>>10;
        if(!popup_up || (y0>145 && y>145))op1f_paint_line(canvas,x0,y0,x,y,1,ink);
        x0=x;y0=y;
    }
}

void receive_clicks(unsigned slot,unsigned ms) {
    for(;;) {
        const unsigned mask=op1f_mask();
        if(read_click==next_click){op1f_unmask(mask);break;}
        const auto change=pending[read_click&15];
        read_click=read_click+1;
        op1f_unmask(mask);
        if(change.slot!=slot)continue;
        int previous=0;
        const unsigned lane=change.axis;
        if(op1f_param_get(&op1f_card_fret,0,slot,4+lane,&previous)!=0)continue;
        const unsigned next=mode(bounded(previous))+1;
        if(op1f_param_set(&op1f_card_fret,0,slot,4+lane,stored_pick[next==3?0:next])!=0)continue;
        popup_axis=lane;
        popup_deadline=ms+mode_hold_ms;
        popup_up=true;
    }
}

void popup(const unsigned (&modes)[4],unsigned ms) {
    if(popup_up && static_cast<int>(ms-popup_deadline)>=0)popup_up=false;
    if(popup_up!=popup_visible) {
        show(card_layer,popup_up);
        popup_visible=popup_up;
    }
    if(!popup_up)return;
    if(popup_axis!=panel_on) {
        if(panel_on<4)show(card_lanes[panel_on],false);
        show(card_lanes[popup_axis],true);
        panel_on=popup_axis;choice_on=3;
        op1f_text_set(picture,"frTitle",popup_names[popup_axis],165,OP1F_CENTRE);
    }
    if(modes[popup_axis]!=choice_on) {
        choice_on=modes[popup_axis];
        if(popup_axis==0)
            for(unsigned index=0;index<3;++index)
                show(string_icons[index],index==choice_on);
        op1f_text_set(picture,"frChoice",choices[popup_axis][choice_on],165,OP1F_CENTRE);
    }
}
}

extern "C" void fret_shown(unsigned slot,unsigned flags) {
    const bool visible=(flags&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP;
    const unsigned mask=op1f_mask();
    if(slot!=slot_on || !visible) {
        popup_up=false;
        read_click=next_click;
    }
    slot_on=slot;
    shown_flags=flags;
    op1f_unmask(mask);
    turns.shown(slot,visible);
}

extern "C" int fret_input(const Op1fInput* event) {
    if(!event || event->bytes<40 || event->thread!=OP1F_KEY_THREAD || !event->mine ||
       slot_on>=8 || (shown_flags&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))!=OP1F_SHOWN_UP)return 0;
    if(turns.take(event))return 1;
    if(event->shift || event->key<54 || event->key>57)return 0;
    if(event->what==OP1F_UP)return 1;
    if(event->what!=OP1F_DOWN)return 0;
    const unsigned mask=op1f_mask();
    const unsigned index=next_click;
    if(index-read_click<16) {
        pending[index&15]={slot_on,event->key-54};
        next_click=index+1;
    }
    op1f_unmask(mask);
    return 1;
}

extern "C" void fret_frame(const Op1fFrame* frame) {
    if(!frame || frame->bytes<40 || frame->disk)return;
    const unsigned slot=slot_on, flags=shown_flags;
    if(slot>=8 || !(flags&OP1F_SHOWN_UP) || frame->page!=&op1f_card_fret)return;
    // The base owns a blank ask page until the pack supplies its document.
    // Load on Fret's first visible page, once, including a failed attempt.
    if(!tried && !(flags&OP1F_SHOWN_COVERED)) {
        tried=true;
        picture=op1f_picture(&op1f_card_fret,"fret-bass.svg");
        if(picture)bind();
    }
    if(picture && !stood)stood=op1f_page_stand_in(&op1f_card_fret,0,picture)!=0;
    if(!(flags&OP1F_SHOWN_COVERED)) {
        turns.apply(&op1f_card_fret,0,slot);
        receive_clicks(slot,frame->ms);
    }
    if(picture) {
        unsigned values[4], modes[4];
        controls(slot,values,modes);
        sound_activity(slot,frame->ms);
        readouts(values);
        hardware(values,modes,frame->ms);
        popup(modes,frame->ms);
        wave(frame->canvas,values[3],modes[3],frame->ms);
    }
    if((flags&OP1F_SHOWN_OFF) && frame->canvas)
        op1f_paint_text(frame->canvas,164,103,"switched off",OP1F_COLOUR(0xcc,0x1f,0x00),2);
}
