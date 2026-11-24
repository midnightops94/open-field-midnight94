#include "build_r6e_card.h"
#include "sid_core.hpp"
#include "sid_scene.hpp"
#include "engine_turns.hpp"

extern "C" const Op1fCard op1f_card_sid OP1F_NAMED("op1f_card_sid");
extern "C" void sid_render(const Op1fVoiceBlock*) OP1F_NAMED("sid_render");
extern "C" void sid_shown(unsigned,unsigned) OP1F_NAMED("sid_shown");
extern "C" int sid_input(const Op1fInput*) OP1F_NAMED("sid_input");
extern "C" void sid_frame(const Op1fFrame*) OP1F_NAMED("sid_frame");

namespace {
constexpr unsigned frames=128;
static_assert(sizeof(sid::Voice)<=160,"Voice must fit the declared pack state");
int to_word(int value) { return sid::clip(value,0,32767)*65536; }
}

extern "C" void sid_render(const Op1fVoiceBlock* block) {
    auto& voice=*static_cast<sid::Voice*>(block->state);
    if(block->events & OP1F_VOICE_FRESH) sid::initialize(voice);
    if(block->events & OP1F_VOICE_BEGINS) sid::start(voice);
    if(!voice.active) {
        for(unsigned n=0;n<2*frames;++n) block->out[n]=0;
        return;
    }
    int words[8];
    for(unsigned n=0;n<8;++n) words[n]=to_word(block->values[n]);
    const unsigned packed=sid::packed_modes([&](unsigned i){return words[4+i];});
    const unsigned pitch=block->step>sid::maximum_step?sid::maximum_step:block->step;
    for(unsigned group=0;group<frames;group+=16) {
        sid::configure(voice,words,packed);
        // Open-Field supplies constant pitch for the block; the ratios change at control ticks.
        const unsigned base=sid::phase_ratio(pitch,voice.base_ratio), donor=sid::phase_ratio(pitch,voice.donor_ratio);
        for(unsigned i=0;i<16;i++){
            const int sample=sid::sample_steps(voice,pitch,base,donor);
            const unsigned n=2*(group+i);
            block->out[n]=sample;
            block->out[n+1]=sample;
        }
    }
    voice.tick+=frames;
}

namespace {
struct Layers {
    void* stack[3]; void* link[3]; void* pulse[5]; void* wire[5];
    void* card; void* accent[4]; void* icon[4][3];
};
void* document;
Layers layers;
bool tried,stood;
volatile unsigned location=8,shown_flags;
unsigned card_until,card_axis;
bool card_open;
EngineTurns turns{};
struct Click {unsigned where,axis;};
Click clicks[16];
volatile unsigned next_click,read_click;
const char* const trace_names[]={"sidBodyVoice","sidEdgeVoice","sidBodyPulse","sidEdgePulse","sidFilter","sidCutTick","sidCircuit"};
const unsigned colours[]={OP1F_COLOUR(0x65,0x97,0xc1),OP1F_COLOUR(0x65,0x97,0xc1),
    OP1F_COLOUR(0x8a,0x65,0x3c),OP1F_COLOUR(0x8a,0x65,0x3c),OP1F_COLOUR(0xa2,0xa4,0xa6),
    OP1F_COLOUR(0xcd,0xd0,0xc2),OP1F_COLOUR(0xcc,0x1f,0)};
void show(void* node,bool on) { if(node) op1f_show(node,on); }
template<unsigned N> void layer(void* (&nodes)[N],unsigned active) {
    for(unsigned n=0;n<N;n++) show(nodes[n],n==active);
}
template<unsigned N> void groups(void* (&nodes)[N],char* name,unsigned digit) {
    for(unsigned n=0;n<N;n++){ name[digit]='0'+n; nodes[n]=op1f_node(document,name); }
}
void bind() {
    char stack[]="sidSt0",link[]="sidLi0",pulse[]="sidPulse0",wire[]="sidWire0",card[]="sidCard0",icon[]="sidCd00";
    groups(layers.stack,stack,5);groups(layers.link,link,5);groups(layers.pulse,pulse,8);
    groups(layers.wire,wire,7);groups(layers.accent,card,7);
    for(unsigned a=0;a<4;a++){icon[5]='0'+a;groups(layers.icon[a],icon,6);}
    layers.card=op1f_node(document,"sidCard");
    for(auto name:trace_names)show(op1f_node(document,name),false);
    show(layers.card,false);
}
void draw(void* canvas,const sid_scene::Values& values,bool card) {
    struct Pixel {short x,y;};
    constexpr unsigned point_count=121+121+5+5+33+2+15;
    static Pixel pixels[point_count];
    static sid_scene::Values previous;
    static bool initialized;
    if(!initialized || !sid_scene::same(values,previous)){
        const auto geom=sid_scene::geometry(values);
        const unsigned dirty=initialized?sid_scene::dirty_paths(previous,values):(1u<<sid_scene::PathCount)-1;
        layer(layers.stack,values.mode[0]);layer(layers.link,values.mode[1]);
        layer(layers.pulse,geom.pulse_layer);layer(layers.wire,geom.wire_layer);
        unsigned index=0;
        for(unsigned role=0;role<sid_scene::PathCount;role++){
            for(unsigned i=0;i<sid_scene::path_points[role];i++,index++){
                if(!(dirty & (1u<<role)))continue;
                const auto p=sid_scene::point(static_cast<sid_scene::PathRole>(role),i,geom);
                pixels[index]={static_cast<short>((p.x+512)>>10),static_cast<short>((p.y+512)>>10)};
            }
        }
        previous=values;initialized=true;
    }
    unsigned offset=0;
    for(unsigned role=0;role<sid_scene::PathCount;role++){
        for(unsigned i=1;i<sid_scene::path_points[role];i++) {
            const Pixel& last=pixels[offset+i-1];
            const Pixel& p=pixels[offset+i];
            int y0=last.y,y1=p.y;
            if(card && role==sid_scene::BodyVoice)continue;
            if(card && role==sid_scene::BodyPulse){
                if(y0<=145 && y1<=145)continue;
                if(y0<146)y0=146;
                if(y1<146)y1=146;
            }
            op1f_paint_line(canvas,last.x,y0,p.x,y1,role==sid_scene::CutTick?1:2,colours[role]);
        }
        offset+=sid_scene::path_points[role];
    }
}
void consume(unsigned slot,unsigned ms) {
    while(read_click!=next_click){
        asm volatile("":::"memory");
        const auto click=clicks[read_click & 15];
        asm volatile("":::"memory");
        read_click=read_click+1;
        if(click.where!=slot)continue;
        int value=0;
        const auto lane=4+click.axis;
        if(op1f_param_get(&op1f_card_sid,0,slot,lane,&value)!=0)continue;
        const auto current=sid::choice(to_word(value));
        const int next=current==sid::First?16384:current==sid::Second?32767:0;
        if(op1f_param_set(&op1f_card_sid,0,slot,lane,next)!=0)continue;
        card_axis=click.axis;card_until=ms+900;card_open=true;
    }
}
}

extern "C" void sid_shown(unsigned slot,unsigned how) {
    turns.shown(slot,(how & OP1F_SHOWN_UP)!=0);
    location=slot;shown_flags=how;card_open=false;read_click=next_click;
}
extern "C" int sid_input(const Op1fInput* event) {
    if(turns.take(event))return 1;
    if(!event->mine || !(shown_flags&OP1F_SHOWN_UP) || location>=8 ||
       event->key<54 || event->key>57) return 0;
    if(event->what==OP1F_UP) return 1;
    if(event->what!=OP1F_DOWN) return 0;
    const unsigned index=next_click;
    if(index-read_click<16) {
        clicks[index & 15]={location,event->key-54};
        asm volatile("":::"memory");
        next_click=index+1;
    }
    return 1;
}
extern "C" void sid_frame(const Op1fFrame* frame) {
    if(frame->bytes<40)return;
    if(!tried) {
        tried=true;document=op1f_picture(&op1f_card_sid,"sid-beetle.svg");
        if(document)bind();
    }
    if(!stood)stood=op1f_page_stand_in(&op1f_card_sid,0,document)!=0;
    const unsigned slot=location;
    turns.apply(&op1f_card_sid,0,slot);
    if(slot>=8 || !(shown_flags & OP1F_SHOWN_UP) || !frame->canvas || frame->page!=&op1f_card_sid)return;
    consume(slot,frame->ms);
    sid_scene::Values values{};
    for(unsigned i=0;i<4;i++){
        int current,mode;
        if(op1f_param_heard(&op1f_card_sid,0,slot,i,&current) || op1f_param_get(&op1f_card_sid,0,slot,4+i,&mode))return;
        values.control[i]=sid::control(to_word(current));values.mode[i]=sid::choice(to_word(mode));
    }
    if(card_open && static_cast<int>(frame->ms-card_until)>=0)card_open=false;
    if(document){
        if(card_open) {
            const auto mode=values.mode[card_axis];
            layer(layers.accent,card_axis);layer(layers.icon[card_axis],mode);
            op1f_text_set(document,"sidTitle",sid_scene::title[card_axis],165,OP1F_CENTRE);
            op1f_text_set(document,"sidValue",sid_scene::choice_name[card_axis][mode],165,OP1F_CENTRE);
        }
        show(layers.card,card_open);
        draw(frame->canvas,values,card_open);
    }
}
