// Keep the display routines compact. The audio callback uses the package's -O3 setting.
#if defined(__bfin__) && defined(__GNUC__)
#pragma GCC optimize ("O2")
#endif
#include "pong.hpp"
#include "pose_motion.hpp"
#include "dsp/extended_court.hpp"
#include "dsp/sync_word.hpp"

namespace {
constexpr unsigned chip = OP1F_PLACE_MASTER;
constexpr unsigned all_places = (1u << 17) - 1;
unsigned place, visibility, pending_load = all_places;
volatile unsigned save_mask;
volatile unsigned court_cap[17]; // maximum position plus one, pending a texture change
volatile unsigned last_click = 4, notice_changed;
unsigned stamp, popup = 4, active_pose = 99;
void* document;
void* off_layer;

bool requested, installed;
void* layers[pong_live_pose::no_layer];
void* bars[12], *frames[2], *tiles[4], *icons[9];
void* marks[6], *bodies[2][7], *needle;
unsigned cached_controls[4], cached_mode = ~0u;
unsigned drawn_card = ~0u;
bool shown_off_layer = false;

char* digits(char* out, unsigned n) {
    const unsigned q=(n * 0xcccdU) >> 19; // exact for page numbers (0..65535)
    if (q) out = digits(out,q);
    *out++ = char('0'+n-q*10);
    *out = 0;
    return out;
}
unsigned number(const char* s) {
    unsigned n=0;
    for (; *s; ++s) {
        if (*s<'0'||*s>'9'||n>35) return 0;
        n=n*10+unsigned(*s-'0');
    }
    return n<=35?n:0;
}
unsigned kind(unsigned p) {return p<8?unsigned(OP1F_KEEP_SYNTH):p<16?unsigned(OP1F_KEEP_DRUM):unsigned(OP1F_KEEP_MASTER);}
unsigned slot(unsigned p) {return p<16?(p&7):0;}
void* part(const char* name) {return op1f_node(document,name);}
void show(void* n,bool visible) {if(n)op1f_show(n,visible);}
void move(void* n,const pong_display_motion::Matrix& m) {if(n){int words[9];for(unsigned i=0;i<9;++i)words[i]=m.words[i];op1f_move(n,words);}}
void text(const char* name,const char* value,unsigned align=OP1F_LEFT) {op1f_text_set(document,name,value,0,align);}
void* numbered(const char* prefix,unsigned n) {
    char name[32];unsigned i=0;
    while(prefix[i]){name[i]=prefix[i];++i;}
    digits(name+i,n);
    return part(name);
}
void two_digits(char* s,unsigned n) {
    const unsigned tens=(n * 0xcccdU) >> 19; n -= tens*10;
    s[0]=char('0'+tens);s[1]=char('0'+n);
}
void bind() {
    char name[8];
    for(unsigned i=0;i<pong_live_pose::no_layer;++i) {
        if(i<2){name[0]='p';name[1]='m';digits(name+2,i+1);}
        else if(i<56){unsigned n=i-2;name[0]='p';name[1]='p';two_digits(name+2,n);name[4]=0;}
        else if(i<59){name[0]='p';name[1]='b';digits(name+2,i-56);}
        else {const unsigned n=i-59;name[0]='p';name[1]='a';name[2]='0'+n/2;name[3]='0'+n%2;name[4]=0;}
        layers[i]=part(name);show(layers[i],false);
    }
    for(unsigned i=0;i<12;++i)bars[i]=numbered("pongmix",i);
    for(unsigned i=0;i<4;++i)tiles[i]=numbered("pct",i);
    for(unsigned i=0;i<9;++i)icons[i]=numbered("pci",i);
    frames[0]=part("pc_frame");frames[1]=part("pc_panel");
    needle=part("pongspinmark");
    text("pongn0","TABLE");text("pongn1","RALLY");text("pongn2","SPIN",OP1F_CENTRE);text("pongn3","INPUT",OP1F_CENTRE);
}
void draw_pose(pong_live_pose::Controls c,pong_instrument_choice::Choice mode) {
    const unsigned texture=mode.bits==12?0:mode.bits==8?1:2;
    pong_live_pose::Selection selection{};
    if(!pong_live_pose::select(c,2-mode.wall,mode.parallel,texture,selection))return;
    for(unsigned i=0;i<pong_live_pose::no_layer;++i)
        show(layers[i],i==selection.material||i==selection.pose||i==selection.ball||i==selection.arrow);
    unsigned pose=selection.pose-pong_live_pose::first_pose;
    unsigned count=mode.parallel?2:1;
    if(active_pose!=pose) {
        active_pose=pose;
        char id[]="m0000";
        two_digits(id+1,pose);
        for(unsigned b=0;b<count;++b)for(unsigned k=0;k<7;++k){
            id[3]='0'+b;id[4]='0'+k;bodies[b][k]=part(id);
        }
        char m[]="pp00r0";m[2]=id[1];m[3]=id[2];
        for(unsigned k=0;k<6;++k){m[5]='0'+k;marks[k]=part(m);}
    }
    const auto p=pong_display_motion::paddle(c,mode.parallel);
    for(unsigned b=0;b<count;++b){
        move(bodies[b][0],p.face[b]);move(bodies[b][1],p.face[b]);move(bodies[b][2],p.rear[b]);
        for(unsigned k=0;k<4;++k)move(bodies[b][3+k],pong_display_motion::connector(p,pose,b,k));
    }
    for(unsigned k=0;k<6;++k){
        show(marks[k],k<selection.marks);
        if(k<selection.marks)move(marks[k],pong_display_motion::rally_mark(p,pose,k));
    }
    move(needle,pong_display_motion::spin_needle(c.spin));
}
void draw(const unsigned* v,pong_instrument_choice::Choice ch) {
    draw_pose({v[0],v[1],v[2]},ch);
    char cv[16],rv[12];
    const char* divisions[]={"1/16","1/8","1/4","1/2","1/16T","1/8T"};
    digits(cv,pong_extended_court::milliseconds(pong_extended_court::fitted_samples(
        pong_extended_court::requested_samples(int(v[0]<<16)),pong_extended_court::maximum_samples(ch.bits))));
    text("pongn0",ch.sync?"TABLE SYNC":"TABLE MS");
    text("pongv0",ch.sync?divisions[unsigned(pong_sync_word::division(v[0]))]:cv);
    digits(rv,pong_rally::repeats(v[1]));text("pongv1",rv);
    for(unsigned i=0;i<12;++i)show(bars[i],i<((v[3]*12u+16383u)/32767u));
}
void card(pong_instrument_choice::Choice c,unsigned selection) {
    const char* title[]={"TABLE","WALL","PLAYERS","TEXTURE"};
    const char* walls[]={"FOAM","WOOD","CONCRETE"};
    const char* values[]={c.sync?"SYNC":"MS",walls[c.wall],c.parallel?"DOUBLES":"SINGLES",
                          c.bits==12?"12 BIT":c.bits==8?"8 BIT":"4 BIT"};
    const unsigned icon[]={8,2-c.wall,c.parallel?4u:3u,c.bits==12?5u:c.bits==8?6u:7u};
    for(auto* f:frames)show(f,selection<4);
    const unsigned tile[]={3,0,1,2};
    for(unsigned i=0;i<4;++i)show(tiles[i],selection<4&&i==tile[selection]);
    for(unsigned i=0;i<9;++i)show(icons[i],selection<4&&i==icon[selection]);
    // The native label keeps its first-use alignment, including on the empty first frame.
    text("poc_name", selection<4 ? title[selection] : "", OP1F_CENTRE);
    text("poc_value", selection<4 ? values[selection] : "", OP1F_CENTRE);
}
}

extern "C" void pong_changed(unsigned,unsigned,unsigned) OP1F_NAMED("pong_changed");
extern "C" void pong_changed(unsigned kind_,unsigned index,unsigned) {
    if(index<8 && (kind_==OP1F_KEEP_SYNTH||kind_==OP1F_KEEP_DRUM||kind_==OP1F_KEEP_MASTER))
        pending_load |= 1u<<(kind_==OP1F_KEEP_MASTER?16:kind_==OP1F_KEEP_DRUM?index+8:index);
}
extern "C" void pong_shown(unsigned,unsigned) OP1F_NAMED("pong_shown");
extern "C" void pong_shown(unsigned p,unsigned how) {
    if(p!=place || !how || (how & (OP1F_SHOWN_OFF|OP1F_SHOWN_COVERED))){popup=4;last_click=4;notice_changed=0;}
    visibility=how;place=p;cached_mode=~0u;
}
extern "C" int pong_input(const Op1fInput*) OP1F_NAMED("pong_input");
extern "C" int pong_input(const Op1fInput* ev) {
    if(!ev->mine||place>=17)return 0;
    if(ev->what==OP1F_TURN){last_click=4;notice_changed=1;return 0;}
    if(ev->key<54||ev->key>57)return 0;
    if(ev->what==OP1F_UP)return 1;
    if(ev->what!=OP1F_DOWN&&ev->what!=OP1F_DOWN_SHIFT)return 0;
    uint32_t next;
    if(!pong_instrument_choice::propose(pong_modes[place],ev->key,next))return 0;
    if(ev->key==57){
        pong_instrument_choice::Choice from{},to{};
        pong_instrument_choice::decode(pong_modes[place],from);
        pong_instrument_choice::decode(next,to);
        if(!to.sync){
            const unsigned cap=pong_extended_court::texture_change_stop(from.bits,to.bits)+1u;
            if(!court_cap[place] || court_cap[place]>cap)court_cap[place]=cap;
        }
    }
    pong_modes[place]=next;
    save_mask |= 1u<<place;
    last_click=ev->key-54;notice_changed=1;
    return 1;
}
extern "C" void pong_frame(const Op1fFrame*) OP1F_NAMED("pong_frame");
extern "C" void pong_frame(const Op1fFrame* now) {
    for(unsigned p=0;p<17;++p){
        const unsigned bit=1u<<p;
        if(court_cap[p]){
            int value=0,limit=int(court_cap[p]-1);
            if(op1f_param_get(&op1f_card_pong,0,p,0,&value)==0 && value>limit)
                op1f_param_set(&op1f_card_pong,0,p,0,limit);
            court_cap[p]=0;
        }
        if(save_mask&bit){
            char value[4];digits(value,pong_modes[p]>>16);
            if(op1f_value_set(&op1f_card_pong,kind(p),slot(p),"pong_mode",value)>=0)save_mask&=~bit;
        }else if(pending_load&bit){
            char value[8]={};int got=op1f_value_get(&op1f_card_pong,kind(p),slot(p),"pong_mode",value,sizeof(value));
            if(got>=0 || got==OP1F_NO_FILE) {pong_modes[p]=(got>=0?number(value):0)<<16;pending_load &=~bit;}
        }
    }
    if(!visibility || place>=17 || now->page!=&op1f_card_pong)return;
    if (!requested) {
        requested=true;
        document=op1f_picture(&op1f_card_pong,"pong.svg");
        if (document) {
            bind();
            off_layer=part("fxOff");
            text("fxOffText","fx off",OP1F_CENTRE);
            show(off_layer,false);
        }
    }
    if(!installed)installed=op1f_page_stand_in(&op1f_card_pong,0,document)!=0;
    if(!document||!installed)return;
    if(notice_changed){popup=last_click;stamp=now->ms;notice_changed=0;}
    if(now->ms-stamp>1024u || now->popup || (visibility & (OP1F_SHOWN_OFF|OP1F_SHOWN_COVERED)))popup=4;
    pong_instrument_choice::Choice ch{};pong_instrument_choice::decode(pong_modes[place],ch);
    unsigned v[4];
    const bool mode_changed=cached_mode!=pong_modes[place];
    bool changed=mode_changed;
    for(unsigned i=0;i<4;++i){
        int heard=0;
        if(op1f_param_heard(&op1f_card_pong,0,place,i,&heard)!=0)return;
        v[i]=unsigned(heard);changed |= cached_controls[i]!=v[i];cached_controls[i]=v[i];
    }
    if(changed){draw(v,ch);cached_mode=pong_modes[place];}
    const bool off=(visibility & OP1F_SHOWN_OFF)!=0;
    const bool visible_off=off && !now->popup && !(visibility & OP1F_SHOWN_COVERED);
    const unsigned selection=off ? 4 : popup;
    if (selection!=drawn_card || (selection<4 && mode_changed)) {
        card(ch,selection);
        drawn_card=selection;
    }
    if (visible_off!=shown_off_layer) {
        show(off_layer,visible_off);
        shown_off_layer=visible_off;
    }
}
