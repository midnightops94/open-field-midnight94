#include "build_r6e_card.h"
#include "core/ppg_duty.hpp"
#include "engine_turns.hpp"
#include "tables.hpp"

extern "C" int ppg_v2_read_fast_q30(unsigned, unsigned, unsigned, unsigned, unsigned);
extern "C" const Op1fCard op1f_card_wave OP1F_NAMED("op1f_card_wave");

namespace {
using u32 = unsigned;
constexpr u32 edge = 15u << 16;
constexpr u32 blue = OP1F_COLOUR(0x7c,0xaa,0xcb), red = OP1F_COLOUR(0xcc,0x1f,0x00);
constexpr u32 ochre = OP1F_COLOUR(0x8a,0x65,0x3c);
constexpr const char* names[] = {"wave",nullptr,"pulse","twin"};
constexpr char banks[8][7] = {"hollow","glass","metal","reed","choir","pulse","octave","broken"};
constexpr u32 motion_of[] = {0,0,1,0,2};
constexpr int motion_preset[] = {0,2,4};
constexpr const char* motion_names[] = {"Fall","Rise","Cycle"};
constexpr char value_ids[4][12] = {"ppgText4","ppgText5","ppgText6","ppgText7"};
constexpr char label_ids[4][12] = {"ppgText0","ppgText1","ppgText2","ppgText3"};
static void* document;
static void* layers[8];
static u32 last_bank = 8, last_motion = 3;
static bool tried, stood;
static volatile u32 shown_slot, shown_flags, clicks[8][2];
static u32 consumed[8][2];
static EngineTurns turns{};

struct Point { int x,y; };
struct SampleAt { int x; u32 phase; };
constexpr SampleAt trace[] = {
#include "core/field-trial/ppg_trace_coordinates.inc"
};
constexpr unsigned trace_points = sizeof(trace) / sizeof(trace[0]);
struct WaveTrace {
    u32 bank = 8, frame = 0, pulse = 0;
    Point samples[trace_points]{};
    Point mark_top{}, mark_bottom{};
};
static WaveTrace primary_trace, twin_trace;
static unsigned last_values[4] = {32768,32768,32768,32768};

u32 fold(u32 x) {
    if (x >= 2 * edge) x -= 2 * edge;
    return x > edge ? 2 * edge - x : x;
}
Point plane(u32 frame) {
    const int at = static_cast<int>(frame >> 2);
    return {106*1024 + ((4762*at+8192)>>14),151*1024 - ((5222*at+8192)>>14)};
}
int px(int units,int center,int scale) {
    const int anchor = center*1024;
    return (anchor+((units-anchor)*scale+512)/1024+512)>>10;
}
Point screen(Point p) {
    return {px(p.x,224,1147),px(p.y,110,1106)};
}
void line(void* canvas, Point a, Point b, int width, u32 colour) {
    a=screen(a);
    b=screen(b);
    op1f_paint_line(canvas,a.x,a.y,b.x,b.y,width,colour);
}
void mark(void* canvas, Point p, int width, u32 color) {
    line(canvas, {p.x,p.y-1024}, {p.x,p.y+1024}, width, color);
}
void draw_wave(void* canvas, WaveTrace& cached, u32 bank, u32 frame, u32 pulse, u32 color) {
    if (cached.bank!=bank || cached.frame!=frame || cached.pulse!=pulse) {
        const Point base=plane(frame);
        const auto duty=ppg_reader::make_duty(pulse<<15);
        for (unsigned i=0; i<trace_points; ++i) {
            const auto& at=trace[i];
            const int s=ppg_v2_read_fast_q30(bank,0,frame,ppg_reader::map_duty(at.phase,duty),0);
            cached.samples[i]=screen({base.x+at.x,base.y-(((s>>10)*19)>>10)});
        }
        cached.mark_top=screen({base.x-5*1024,base.y-1024});
        cached.mark_bottom=screen({base.x-5*1024,base.y+1024});
        cached.bank=bank;
        cached.frame=frame;
        cached.pulse=pulse;
    }
    Point previous=cached.samples[0];
    for (unsigned i=1; i<trace_points; ++i) {
        const Point next=cached.samples[i];
        op1f_paint_line(canvas,previous.x,previous.y,next.x,next.y,2,color);
        previous=next;
    }
    op1f_paint_line(canvas,cached.mark_top.x,cached.mark_top.y,cached.mark_bottom.x,cached.mark_bottom.y,3,color);
}
void draw_sweep(void* canvas, u32 frame, u32 control) {
    const u32 distance=60*((control*control+16384u)>>15);
    if (!distance) return;
    const u32 limit=frame+distance, end=fold(limit);
    Point a=plane(limit>=2*edge?0:(frame<end?frame:end));
    Point b=plane(limit>=edge?edge:(frame>end?frame:end));
    a.x+=180*1024;
    b.x+=180*1024;
    line(canvas,a,b,1,ochre);
    mark(canvas,a,2,ochre);
    mark(canvas,b,2,ochre);
}
void format(char* out,u32 key,bool decimal,bool padded) {
    // The maximum value is 300 in tenths. Exact small unsigned divide by ten.
    const u32 whole10=(key*52429u)>>19;
    const u32 n=decimal ? whole10 : key;
    const u32 tens=(n*52429u)>>19;
    const u32 hundreds=(tens*52429u)>>19;
    if (hundreds) *out++=static_cast<char>('0'+hundreds);
    if (tens || padded) *out++=static_cast<char>('0'+tens-10*hundreds);
    *out++=static_cast<char>('0'+n-10*tens);
    if (decimal) { *out++='.'; *out++=static_cast<char>('0'+key-10*whole10); }
    *out=0;
}
void attach() {
    if (!tried) {
        tried=true;
        document=op1f_picture(&op1f_card_wave,"wave.svg");
        if (document) {
            char id[]="ppgBank0";
            for (unsigned i=0; i<8; ++i) {
                id[7]=static_cast<char>('0'+i);
                layers[i]=op1f_node(document,id);
            }
            for (unsigned i=0; i<4; ++i)
                if (names[i]) op1f_text_set(document,label_ids[i],names[i],0,i<2?OP1F_LEFT:OP1F_RIGHT);
        }
    }
    if (!stood) stood=op1f_page_stand_in(&op1f_card_wave,0,document)!=0;
}
void discard_clicks(unsigned slot) {
    for(unsigned i=0;i<2;++i) consumed[slot][i]=clicks[slot][i];
}
void apply_click(unsigned slot,unsigned lane) {
    const unsigned mask=op1f_mask();
    const unsigned count=clicks[slot][lane], delta=count-consumed[slot][lane];
    consumed[slot][lane]=count;
    op1f_unmask(mask);
    if(!delta) return;
    int base=0;
    if(op1f_param_get(&op1f_card_wave,0,slot,4+lane,&base)!=0) return;
    if(lane==0)
        op1f_param_set(&op1f_card_wave,0,slot,4,(static_cast<u32>(base)+delta)&7u);
    else if(base>=0 && base<5)
        op1f_param_set(&op1f_card_wave,0,slot,5,motion_preset[(motion_of[base]+delta)%3]);
}
}
extern "C" void wave_shown(unsigned slot,unsigned flags) OP1F_NAMED("wave_shown");
extern "C" void wave_shown(unsigned slot,unsigned flags) {
    const bool active=(flags&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP;
    const unsigned mask=op1f_mask();
    if (slot<8 && (!active || slot!=shown_slot))
        discard_clicks(slot);
    shown_slot=slot;
    shown_flags=flags;
    op1f_unmask(mask);
    turns.shown(slot,active);
}
extern "C" int wave_input(const Op1fInput* e) OP1F_NAMED("wave_input");
extern "C" int wave_input(const Op1fInput* e) {
    if (!e || e->bytes<40 || e->thread!=OP1F_KEY_THREAD || !e->mine || shown_slot>=8 ||
        (shown_flags&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))!=OP1F_SHOWN_UP) return 0;
    if (turns.take(e)) return 1;
    if (e->shift || (e->key!=54 && e->key!=55)) return 0;
    if (e->what==OP1F_UP) return 1;
    if (e->what!=OP1F_DOWN) return 0;
    const unsigned mask=op1f_mask(), slot=shown_slot;
    if(slot<8) ++clicks[slot][e->key-54];
    op1f_unmask(mask);
    return 1;
}
extern "C" void wave_frame(const Op1fFrame* now) OP1F_NAMED("wave_frame");
extern "C" void wave_frame(const Op1fFrame* now) {
    if (!now || now->bytes<40 || now->disk) return;
    wave_tables_frame();
    attach();
    const unsigned slot=shown_slot, flags=shown_flags;
    if (slot>=8 || !(flags&OP1F_SHOWN_UP) || now->page!=&op1f_card_wave) return;
    if (!(flags&OP1F_SHOWN_COVERED)) {
        turns.apply(&op1f_card_wave,0,slot);
        apply_click(slot,0);
        apply_click(slot,1);
    }
    if (!now->canvas) return;
    int values[6];
    for (unsigned i=0; i<6; ++i)
        if (op1f_param_heard(&op1f_card_wave,0,slot,i,&values[i])!=0) return;
    const unsigned bank=static_cast<unsigned>(values[4])&7u;
    if (document && bank!=last_bank) {
        for (unsigned i=0;i<8;++i) if(layers[i]) op1f_show(layers[i],i==bank);
        op1f_text_set(document,"bankName",banks[bank],0,OP1F_CENTRE);
        last_bank=bank;
    }
    const unsigned motion=values[5]>=0 && values[5]<5 ? motion_of[values[5]] : 0;
    if(document && motion!=last_motion) {
        op1f_text_set(document,label_ids[1],motion_names[motion],0,OP1F_LEFT);
        last_motion=motion;
    }
    // The former native renderer rounds the effective Q31 lane to Q15 for its display.
    unsigned q[4];
    for (unsigned i=0;i<4;++i) q[i]=values[i]>=32767?32768u:unsigned(values[i]);
    const unsigned span=(q[1]*q[1]+16384u)>>15;
    const unsigned twin_readout=(99u*unsigned(values[3])+16383u)/32767u;
    const unsigned k[4]={(150*q[0]+16384u)>>15,(300*span+16384u)>>15,25u+((50*q[2]+16384u)>>15),twin_readout};
    for (unsigned i=0;i<4;++i) {
        if (document && k[i]!=last_values[i]) {
            char value[8];
            format(value,k[i],i<2,i==3);
            if (op1f_text_set(document,value_ids[i],value,0,i<2?OP1F_LEFT:OP1F_RIGHT)) last_values[i]=k[i];
        }
    }
    const u32 frame=30*q[0];
    const bool table_ready = wave_tables_ready() != nullptr;
    if (table_ready) {
        if(q[3]) draw_wave(now->canvas,twin_trace,bank,fold(frame+8*q[3]),q[2],red);
        draw_wave(now->canvas,primary_trace,bank,frame,q[2],blue);
    }
    draw_sweep(now->canvas,frame,q[1]);
    if (flags&OP1F_SHOWN_OFF) op1f_paint_text(now->canvas,164,103,"switched off",red,2);
    else if (!table_ready) {
        const char* status=wave_tables_wait_expired()?"wave memory":"loading wave";
        op1f_paint_text(now->canvas,164,103,status,ochre,2);
    }
}
