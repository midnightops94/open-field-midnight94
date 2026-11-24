#include "display.hpp"
#include "direction.hpp"
#include "splice.hpp"
#include "curves.hpp"
#include "catcher_scene.hpp"
extern "C" const Op1fCard op1f_card_skatter OP1F_NAMED("op1f_card_skatter");
inline void* operator new(size_t,void* storage) noexcept {return storage;}

namespace {
constexpr unsigned ring_bytes=131072;
struct State {
    splice::Engine engine;
    unsigned ready;
    unsigned short previous[4];
    unsigned mode;
    alignas(8) unsigned char ring[ring_bytes];
};
static_assert(sizeof(State)<=131648,"Per-place Skatter working memory");
volatile unsigned page_place,page_flags;
skatter_display::DirectionPopup direction_popup{nullptr,{},0,0,-1,false,false};
bool direction_eligible(unsigned where) {
    return where==page_place && (page_flags & (OP1F_SHOWN_UP|OP1F_SHOWN_OFF|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP;
}
// The key thread publishes counts. The frame thread owns serialized mode and saving.
struct Setting {
    volatile unsigned capture_events, direction_events, active;
    unsigned capture_seen, direction_seen, encoded;
    bool read, save;
};
Setting settings[17]{};

unsigned choice(unsigned place) {
    const unsigned v=place<17 ? settings[place].active : 0;
    return v ? v-1 : 2;
}
void settings_frame(unsigned ms) {
    for(unsigned place=0;place<17;++place) {
        Setting& s=settings[place];
        const unsigned scope=place==16 ? unsigned(OP1F_KEEP_MASTER) : place>=8 ? OP1F_KEEP_DRUM : OP1F_KEEP_SYNTH;
        const unsigned slot=place&7;
        if(s.read || !s.active) {
            char buf[8] = {};
            int n=op1f_value_get(&op1f_card_skatter,scope,slot,"skatter_mode",buf,sizeof(buf));
            if(n==OP1F_NOT_NOW) continue;
            s.encoded=n==1 && buf[0]>='0' && buf[0]<='5' ? unsigned(buf[0]-'0') : 2;
            s.active=s.encoded+1;
            s.read=false;
        }
        unsigned capture=s.capture_events,direction=s.direction_events;
        if(capture!=s.capture_seen || direction!=s.direction_seen) {
            unsigned modes=s.encoded>>1;
            unsigned turns=direction-s.direction_seen;
            // Key edges accumulate independently of frame cadence. Each fold preserves mod 3.
            turns=(turns>>16)+(turns&65535u);
            turns=(turns>>8)+(turns&255u);
            turns=(turns>>4)+(turns&15u);
            turns=(turns>>2)+(turns&3u);
            const unsigned n=modes+turns;
            modes=n-3*((n*171u)>>9);
            s.encoded=(modes<<1)|((s.encoded^(capture-s.capture_seen))&1);
            if(direction!=s.direction_seen && direction_eligible(place))direction_popup.select(modes,ms);
            s.capture_seen=capture;s.direction_seen=direction;
            s.active=s.encoded+1;s.save=true;
        }
        if(s.save) {
            char buf[]={char('0'+s.encoded),0};
            s.save=op1f_value_set(&op1f_card_skatter,scope,slot,"skatter_mode",buf)!=0;
        }
    }
}
template<unsigned N> unsigned interpolate(const uint32_t(&table)[N],unsigned word) {
    const unsigned phase=2u*word+unsigned(word>=8192)+unsigned(word>=24576);
    const unsigned i=phase>>8, pos=phase&255;
    if(i>=N-1)return table[N-1];
    const unsigned a=table[i],b=table[i+1];
    return b>=a?a+(((b-a)*pos+128)>>8):a-(((a-b)*pos+128)>>8);
}
unsigned unit(unsigned q) {return 32769u*q+unsigned(q>=16384);}
void arm(State& s) {
    // The ring is circular. Reset invalidates all old samples in constant time.
    new (&s.engine) splice::Engine();
    s.ready=s.engine.bind(s.ring,ring_bytes,splice::full);
    s.mode=~0u;
}
void* doc;
fx_page::OffNotice notice{};
struct ModeBadge {
    void* free_group;
    void* captured_group;
    bool ready;
    int previous;

    void bind(void* document) {
        free_group=op1f_node(document,"sModeFree");
        captured_group=op1f_node(document,"sModeCapture");
        void* background=op1f_node(document,"sCaptureBadge");
        if(!free_group || !captured_group || !background) return;
        if(!op1f_text_set(document,"sFree","free",0,OP1F_CENTRE) ||
           !op1f_text_set(document,"sCapture","capture",0,OP1F_CENTRE)) return;
        const int ink=op1f_text_width(document,"sCapture");
        if(ink<=0 || ink>180) return;
        // Four pixels on each side of the measured stock-font lettering. The
        // position uses half-pixels, keeping odd-width labels centred on x240.
        const int width=ink+8;
        int matrix[]={width*16,0,0,0,1024,0,240*1024-width*512,0,1024};
        op1f_move(background,matrix);
        ready=true;
    }
    void draw(void* canvas,bool installed,bool captured,bool off) {
        const int mode=off || !installed || !ready ? 0 : captured ? 2 : 1;
        if(mode!=previous) {
            if(free_group) op1f_show(free_group,mode==1);
            if(captured_group) op1f_show(captured_group,mode==2);
            previous=mode;
        }
        if(off || mode!=0) return;
        const char* label=captured?"CAPTURE":"FREE";
        const unsigned color=OP1F_COLOUR(205,208,194);
        // The public bitmap helper returns the actual width even off canvas.
        const int ink=op1f_paint_text(canvas,0,-20,label,color,2);
        const int left=240-((ink+1)>>1);
        if(captured) op1f_paint_box(canvas,left-4,4,ink+8,18,OP1F_COLOUR(60,96,135));
        op1f_paint_text(canvas,left,6,label,color,2);
    }
};
ModeBadge mode_badge{nullptr,nullptr,false,-1};
struct Part {const char* id;void* node;int visible;};
#include "parts.inc"
void visible(unsigned n,bool on) {
    if(parts[n].node && parts[n].visible!=int(on)) {
        op1f_show(parts[n].node,on);parts[n].visible=on;
    }
}
void move(unsigned part,catcher_scene::Matrix p) {
    if(parts[part].node) {
        int m[]={p.xx,p.xy,0,p.yx,p.yy,0,p.x,p.y,1024};
        op1f_move(parts[part].node,m);
    }
}
unsigned last_capture,last_place=~0u,pulse_time;
void scene(const unsigned (&v)[4],bool captured,unsigned ms) {
    using namespace catcher_scene;
    if(page_place!=last_place) {last_place=page_place;last_capture=captured;pulse_time=ms-1024;}
    if(captured!=bool(last_capture)) {last_capture=captured;pulse_time=ms;}
    const unsigned elapsed=ms-pulse_time;
    unsigned eased=phase(elapsed>=pulse_ticks?0:pulse_ticks-elapsed);
    move(P_sNet, net(v[0])); move(P_sShaft,shaft(v[0]));move(P_sSwing,swing(v[0],eased));
    move(P_sScarf,scarf(v[3],v[2],eased));
    for(unsigned i=0;i<3;++i) move(fly_id[i],fly(i,v[2],eased,captured));
    visible(P_sSnared,captured);visible(P_sFlyC,!captured);
    for(unsigned i=0;i<14;++i) for(unsigned k=0;k<3;++k)
        visible(grit_id[i][k],engraved(i,k,ink(v[1])));
    for(unsigned k=0;k<3;++k) {
        visible(glow_id[k],k<glow(v[3]));
        for(unsigned i=0;i<3;++i) visible(trail_id[i][k],k<trails(v[3]));
    }
    visible(P_sE30,trails(v[3])!=0);
}
}
extern "C" void skatter_reset(unsigned,void* raw) OP1F_NAMED("skatter_reset");
extern "C" void skatter_reset(unsigned,void* raw) {
    if(raw) arm(*static_cast<State*>(raw));
}
extern "C" void skatter_process(const Op1fEffectBlock* b) OP1F_NAMED("skatter_process");
extern "C" void skatter_process(const Op1fEffectBlock* b) {
    if(!b || b->bytes<44 || b->frames!=64 || !b->on || !b->state || !b->values || !b->left || !b->right) return;
    State& s=*static_cast<State*>(b->state);
    if(!s.ready || b->fresh) arm(s);
    if(!s.ready) return;
    unsigned short q[4];
    unsigned mode=choice(b->place);
    bool changed=s.mode!=mode;
    for(unsigned i=0;i<4;++i) {q[i]=fx_page::q15(b->values[i]);changed|=q[i]!=s.previous[i];}
    if(changed) {
        using namespace splice_field;
        splice::Controls controls{(interpolate(window_frames,q[0])+128)>>8,interpolate(grain_frames,q[1])>>8,
            (interpolate(grain_speed,q[1])+128)>>8,unit(q[0]),unit(q[1]),unit(q[2]),unit(q[3]),
            static_cast<uint8_t>(mode>>1),bool(mode&1),false};
        s.engine.controls(controls);
        for(unsigned i=0;i<4;++i) s.previous[i]=q[i];
        s.mode=mode;
    }
    s.engine.recut_block(splice::Recut::free,{},64);
    for(unsigned i=0;i<64;++i) {
        int32_t l,r;s.engine.tick(b->left[i],b->right[i],l,r);b->left[i]=l;b->right[i]=r;
    }
}
extern "C" void skatter_shown(unsigned,unsigned) OP1F_NAMED("skatter_shown");
extern "C" void skatter_shown(unsigned p,unsigned how) {
    const unsigned previous_place=page_place;
    page_place=p;page_flags=how;
    if(previous_place!=p || !direction_eligible(p))direction_popup.open=false;
}
extern "C" void skatter_changed(unsigned,unsigned,unsigned) OP1F_NAMED("skatter_changed");
extern "C" void skatter_changed(unsigned scope,unsigned slot,unsigned how) {
    if(how==OP1F_KEPT_SAVED || how==OP1F_KEPT_ENGINE) return;
    unsigned p=scope==OP1F_KEEP_MASTER?16:scope==OP1F_KEEP_SYNTH?slot:8+slot;
    if(p>16) return;
    if(p==page_place)direction_popup.open=false;
    Setting& s=settings[p];s.read=true;s.save=false;
    s.capture_seen=s.capture_events;s.direction_seen=s.direction_events;
}
extern "C" int skatter_input(const Op1fInput* e) OP1F_NAMED("skatter_input");
extern "C" int skatter_input(const Op1fInput* e) {
    if(e->bytes<32 || !e->mine || page_place>=17 || !(page_flags&OP1F_SHOWN_UP)) return 0;
    if(e->key!=54 && e->key!=56 && e->key!=61 && e->key!=63) return 0;
    if(e->what==OP1F_UP) return 1;
    if(e->what!=OP1F_DOWN && e->what!=OP1F_DOWN_SHIFT) return 0;
    if(e->key==54) ++settings[page_place].capture_events;
    if(e->key==56) ++settings[page_place].direction_events;
    return 1;
}
extern "C" void skatter_frame(const Op1fFrame* f) OP1F_NAMED("skatter_frame");
extern "C" void skatter_frame(const Op1fFrame* f) {
    if(f->bytes<40) return;
    settings_frame(f->ms);
    if(!f->canvas || f->page!=&op1f_card_skatter || !(page_flags&OP1F_SHOWN_UP)) return;
    static bool tried,installed;
    if(f->disk)return;
    if(!doc && !tried) {
        tried=true;
        doc=op1f_picture(&op1f_card_skatter,"skatter_off.svg");
        if(!doc) doc=op1f_picture(&op1f_card_skatter,"night.svg");
        if(doc) {
            notice.bind(doc);
            direction_popup.bind(doc);
            mode_badge.bind(doc);
            for(auto& p:parts) {p.node=op1f_node(doc,p.id);p.visible=-1;}

        }
    }
    if(doc && !installed) installed=op1f_page_stand_in(&op1f_card_skatter,0,doc)!=0;
    const bool off=notice.update(page_flags | (f->popup?OP1F_SHOWN_COVERED:0),installed);
    const bool captured=choice(page_place)&1;
    mode_badge.draw(f->canvas,installed,captured,page_flags&OP1F_SHOWN_OFF);
    unsigned v[4];
    if(fx_page::values(&op1f_card_skatter,page_place,v)) {
        if(doc) scene(v,captured,f->ms);
        static const char* const names[]={"NET","GRAIN","SCATTER","MIX"};
        fx_page::captions(f->canvas,installed?doc:nullptr,names,v);

    }
    if(f->popup) direction_popup.open=false;
    direction_popup.draw(doc,f->canvas,installed,f->ms);
    notice.fallback(f->canvas,off);
}
