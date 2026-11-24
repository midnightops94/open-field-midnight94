#include "effect.hpp"

namespace {
constexpr unsigned lanes[4]={2,2,2,2};
constexpr char key[]="split_mode";
struct Setting {
    volatile unsigned active;
    volatile unsigned clicks[4],last;
    unsigned seen[4];
    bool loaded,dirty;
};
Setting states[17]{};
volatile unsigned page=~0u,flags;
volatile unsigned meters[17][3]{};
unsigned opened=~0u,openedAt;
int laneShown=-1;
unsigned scope(unsigned p){return p==16?unsigned(OP1F_KEEP_MASTER):p>=8?OP1F_KEEP_DRUM:OP1F_KEEP_SYNTH;}
bool visible(unsigned p){return page==p && (flags&(OP1F_SHOWN_UP|OP1F_SHOWN_OFF|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP;}
}
void split_show_state(unsigned p,unsigned how){
    if(p!=page || (how&(OP1F_SHOWN_UP|OP1F_SHOWN_OFF|OP1F_SHOWN_COVERED))!=OP1F_SHOWN_UP)opened=~0u;
    page=p;flags=how;
}
unsigned split_mode(unsigned p,unsigned lane){return p<17 && lane<4?(states[p].active>>(lane*2))&3:0;}
int split_popup(unsigned p,unsigned ms){
    if(opened==p && ms-openedAt>=1024)opened=~0u;
    return opened==p && visible(p)?laneShown:-1;
}
void split_publish_meter(unsigned p,unsigned i,unsigned q15){if(p<17 && i<3)meters[p][i]=q15;}
unsigned split_meter(unsigned p,unsigned i){return p<17 && i<3?meters[p][i]:0;}
extern "C" void split_changed(unsigned,unsigned,unsigned) OP1F_NAMED("split_changed");
extern "C" void split_changed(unsigned where,unsigned slot,unsigned why){
    if(why==OP1F_KEPT_SAVED || why==OP1F_KEPT_ENGINE)return;
    const unsigned p=where==OP1F_KEEP_MASTER?16:where==OP1F_KEEP_SYNTH?slot:where==OP1F_KEEP_DRUM?8+slot:~0u;
    if(p>=17)return;
    Setting& s=states[p];
    s.active=0;s.loaded=false;s.dirty=false;
    for(unsigned i=0;i<4;++i)s.seen[i]=s.clicks[i];
    if(opened==p)opened=~0u;
}
extern "C" int split_input(const Op1fInput*) OP1F_NAMED("split_input");
extern "C" int split_input(const Op1fInput* e){
    const unsigned p=page;
    if(!e || e->bytes<32 || !e->mine || p>=17 || !(flags&OP1F_SHOWN_UP) || (flags&OP1F_SHOWN_COVERED))return 0;
    if(e->key<54 || e->key>57 || lanes[e->key-54]<2)return 0;
    if(e->what==OP1F_UP)return 1;
    if(e->what!=OP1F_DOWN && e->what!=OP1F_DOWN_SHIFT)return 0;
    const unsigned i=e->key-54;
    ++states[p].clicks[i];states[p].last=i;
    return 1;
}
void split_settings_frame(unsigned ms){
    for(unsigned p=0;p<17;++p){
        Setting& s=states[p];
        if(!s.loaded){
            char saved[8]={};
            int n=op1f_value_get(&op1f_card_split,scope(p),p&7,key,saved,sizeof(saved));
            if(n==OP1F_NOT_NOW)continue;
            unsigned restored=0;
            if(n==6 && saved[0]=='"' && saved[5]=='"'){
                for(unsigned i=0;i<4;++i){
                    unsigned v=unsigned(saved[1+i]-'0');
                    if(v<lanes[i])restored|=v<<(i*2);
                }
            }
            s.active=restored;s.loaded=true;
        }
        unsigned latest=s.active;
        for(unsigned i=0;i<4;++i){
            const unsigned n=s.clicks[i],steps=n-s.seen[i];s.seen[i]=n;
            if(!steps)continue;
            const unsigned bit=2*i,previous=(latest>>bit)&3;
            unsigned m=previous;
            // For ternary controls, byte sums equal the original word modulo 3.
            if(lanes[i]==3){
                unsigned reduced=(steps&255)+((steps>>8)&255)+((steps>>16)&255)+(steps>>24)+previous;
                m=reduced-3*((reduced*43691u)>>17);
            }else m=(previous+steps)&1;
            latest=(latest&~(3u<<bit))|(m<<bit);s.dirty=true;
            if(visible(p)){opened=p;openedAt=ms;laneShown=int(s.last);}
        }
        s.active=latest;
        if(s.dirty){
            char saved[]="\"0000\"";
            for(unsigned i=0;i<4;++i)saved[1+i]=char('0'+((latest>>(2*i))&3));
            s.dirty=op1f_value_set(&op1f_card_split,scope(p),p&7,key,saved)!=0;
        }
    }
}
