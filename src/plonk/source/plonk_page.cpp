#include "build_r6e_card.h"
#include "engine_turns.hpp"
#include "plonk_core.hpp"
#include "spring_setting.hpp"

extern "C" const Op1fCard op1f_card_plonk OP1F_NAMED("op1f_card_plonk");
extern "C" unsigned plonk_ratio_q16(unsigned);
extern "C" unsigned plonk_wet_level();

namespace {
#include "plonk_motion_tables.inc"
#include "plonk_ribbon.inc"

EngineTurns turns{};
unsigned place=8, shown=0, counts[2]={}, lastClick=0, until=0;
unsigned previousWet=0, wetChanged=0;
int popup=-1, requested=-1;
void* doc;
bool stood=false;
void* cells[60], *light[60], *cards[6], *scaledRatio, *hatch, *hatchLip;
void* strikeLive, *strikeClosed, *coils, *chanceNodes[3];
const char* const titles[]={"fold","ratio","strike","chance"};
const char* const titleIds[]={"plTitle0","plTitle1","plTitle2","plTitle3"};
const char* const valueIds[]={"plValue0","plValue1","plValue2","plValue3"};
const char* const cardIds[]={"plWave0","plWave1","plWave2","plSpring0","plSpring1","plSpring3"};
struct CardText {
    const char* titleId;
    const char* valueId;
    const char* fitId;
    const char* title;
    const char* value;
};
const CardText cardText[]={
    {"plWaveTitle0","plWaveName0","plWaveFit0","waveform","sine"},
    {"plWaveTitle1","plWaveName1","plWaveFit1","waveform","triangle"},
    {"plWaveTitle2","plWaveName2","plWaveFit2","waveform","soft square"},
    {"plSpringTitle0","plMix0","plSpringFit0","spring","off"},
    {"plSpringTitle1","plMix1","plSpringFit1","spring","short"},
    {"plSpringTitle3","plMix3","plSpringFit3","spring","long"},
};
const unsigned waveWords[]={0,16384,32767};
int last[4]={-1,-1,-1,-1}, lastWave=-1, lastFlex=-1, lastNumber[4]={-1,-1,-1,-1};
bool lit[60];
void show(void* node,bool on){if(node)op1f_show(node,on);}
void* node(const char* id){return op1f_node(doc,id);}
void transform(void* n,int a,int b,int c,int d,int x,int y){
    const int m[9]={a,b,0,c,d,0,x,y,1024};
    if(n)op1f_move(n,m);
}
void writeCard(unsigned index){
    const CardText& card=cardText[index];
    constexpr int room=165;
    op1f_text_set(doc,card.titleId,card.title,room,OP1F_CENTRE);
    // Width zero lays out the complete option. Measuring an already shortened
    // line would lose the last letters before the parent could be scaled.
    if(!op1f_text_set(doc,card.valueId,card.value,0,OP1F_CENTRE))return;
    int width=op1f_text_width(doc,card.valueId);
    // Archived 14 px atlas ink measures 136 px for "soft square". When the
    // native width is unavailable, reserve 170 px for the 15 px face.
    if(width<=0 && index==2)width=170;
    if(width<=room)return;
    int scale=1024;
    while(scale>8 && width*scale>room*1024)scale-=8;
    const int inset=1024-scale;
    // Preserve the native rows' horizontal centre and the lower row's
    // vertical centre (121.85 px) when the finite Q10 transform is applied.
    const int dy=121*inset+((55706*inset+32768)>>16);
    transform(node(card.fitId),scale,0,0,scale,547*inset/2,dy);
}
void setCard(int index){
    if(index==popup)return;
    if(popup>=0)show(cards[popup],false);
    if(index>=0)show(cards[index],true);
    popup=index;
}
void bind(){
    doc=op1f_picture(&op1f_card_plonk,"plonk.svg");
    if(!doc)return;
    transform(node("plMechanism"),1024,0,0,1024,0,-18*1024);
    char id[]="plCell00", shade[]="plLit00";
    for(unsigned i=0;i<60;++i){
        const unsigned tens=(205*i)>>11;
        id[6]=shade[5]=char('0'+tens);
        id[7]=shade[6]=char('0'+i-10*tens);
        cells[i]=node(id);light[i]=node(shade);show(light[i],false);
    }
    for(unsigned i=0;i<6;++i){
        cards[i]=node(cardIds[i]);
        writeCard(i);
        show(cards[i],false);
    }
    for(unsigned i=0;i<4;++i){
        op1f_text_set(doc,titleIds[i],titles[i],0,i<2?OP1F_LEFT:OP1F_RIGHT);
    }
    scaledRatio=node("plRatioLive");hatch=node("plHatchFace");hatchLip=node("plHatchLip");
    strikeLive=node("plStrikeLive");strikeClosed=node("plStrike0");coils=node("plStrikeLiveCoil");
    chanceNodes[0]=node("plTravelRange");chanceNodes[1]=node("plTravelLeft");chanceNodes[2]=node("plTravelRight");
}
unsigned waveMode(int v){return unsigned(v)<8192?0:unsigned(v)<24576?1:2;}

int foldHeight(const short* wave,unsigned phase,unsigned point){
    const unsigned cell=phase>>24,frac=(phase>>16)&255;
    const int shape=wave[cell]+(((wave[cell+1]-wave[cell])*int(frac))>>8);
    const int amp=(shape*fold_envelope[point])>>15;
    return 116*1024-((amp*17408)>>15);
}
void fold(unsigned raw,unsigned mode){
    constexpr unsigned span=98426334u;
    const unsigned inc=55476661u+(span>>15)*raw+(((span&32767u)*raw)>>15);
    const short* wave=mode==0?plonk::sine:mode==1?profile1:profile2;
    int y=foldHeight(wave,0,0);
    for(unsigned i=0;i<60;++i){
        const int y1=foldHeight(wave,(2*i+2)*inc,2*i+2);
        transform(cells[i],(edge_x[i+1]-edge_x[i]+8)>>4,(y1-y+8)>>4,
                  (edge_skew[i]+8)>>4,704,edge_x[i],y);
        const bool bright=y1<y;
        if(bright!=lit[i]){show(light[i],bright);lit[i]=bright;}
        y=y1;
    }
}
void ratio(unsigned raw){
    const unsigned knee=10813;
    const int octaves=raw<=knee ? -131072+int((raw*12350+512)>>10)
                                : -655+int(((raw-knee)*9201+512)>>10);
    const int span=127*1024-((octaves*(octaves<0?26:18))>>6);
    const int scale=int((unsigned(span)*8240+524288)>>20);
    transform(scaledRatio,scale,0,0,1024,151*(1024-scale),0);
}
void strike(unsigned raw){
    show(strikeClosed,raw==0);show(strikeLive,raw!=0);
    if(!raw)return;
    // The installed hatch's top pivot is y=157. Its authored 40% rest height
    // is 19.2 px, while the lip is 1.2 px high at that setting.
    const int bottom=185*1024-int((raw*22528+16384)>>15);
    const int scale=((bottom-157*1024)*3413+32768)>>16;
    transform(hatch,1024,0,0,scale,0,157*(1024-scale));
    const int lipScale=int((raw*5+32)>>6);
    const int shift=bottom-((180429>>6)*lipScale>>4);
    transform(hatchLip,1024,0,0,lipScale,0,shift);
}
void chance(unsigned raw){
    const unsigned cell=raw>>10;
    const int first=travel_reach[cell],next=travel_reach[cell+1];
    const int reach=2*(first+(((next-first)*int(raw&1023)+512)>>10));
    const int scale=int((unsigned(reach)*79814+524288)>>20);
    const int dx=reach-13453;
    transform(chanceNodes[0],scale,0,0,1024,300*(1024-scale),0);
    transform(chanceNodes[1],1024,0,0,1024,-dx,0);
    transform(chanceNodes[2],1024,0,0,1024,dx,0);
}
void writeNumber(unsigned lane,unsigned raw){
    // Divide the rounded numerator by 32767 exactly on the full encoder range.
    const unsigned rounded=raw*99u+16383u;
    const unsigned units=(rounded+(rounded>>15)+1u)>>15;
    unsigned v=lane==1 ? (plonk_ratio_q16(raw)*100+32768)>>16 : units;
    if(lastNumber[lane]==int(v))return;
    lastNumber[lane]=int(v);
    char s[7];unsigned pos=0;
    if(lane==1){
        unsigned whole=0;while(v>=100){++whole;v-=100;}
        s[pos++]=char('0'+whole);s[pos++]='.';
    }
    const unsigned tens=(205*v)>>11;
    s[pos++]=char('0'+tens);
    s[pos++]=char('0'+v-10*tens);
    s[pos]=0;
    op1f_text_set(doc,valueIds[lane],s,0,lane<2?OP1F_LEFT:OP1F_RIGHT);
}
void draw(unsigned slot,unsigned ms){
    int vals[6];
    for(unsigned i=0;i<6;++i)if(op1f_param_heard(&op1f_card_plonk,0,slot,i,vals+i))return;
    const int mode=waveMode(vals[4]);
    for(unsigned i=0;i<4;++i){
        writeNumber(i,vals[i]);
        if(last[i]==vals[i] && (i || lastWave==mode))continue;
        switch(i){case 0:fold(vals[0],mode);break;case 1:ratio(vals[1]);break;
                  case 2:strike(vals[2]);break;default:chance(vals[3]);}
        last[i]=vals[i];
    }
    lastWave=mode;
    const unsigned wet=plonk_wet_level();
    if(wet!=previousWet){previousWet=wet;wetChanged=ms;}
    const int amount=(vals[2] && plonk::spring_setting(vals[5]) && ms-wetChanged<128)?int((wet*52+16)>>5):0;
    int flex=amount>225?225:amount;
    const int decay=lastFlex>0?(lastFlex*707)>>10:0;
    if(decay>flex)flex=decay;
    if(flex!=lastFlex){
        transform(coils,1024,0,0,1024+flex,0,-((345*flex)>>1));
        lastFlex=flex;
    }
}
void clicks(unsigned slot,unsigned ms){
    unsigned steps[2],recent;
    const unsigned mask=op1f_mask();
    steps[0]=counts[0];steps[1]=counts[1];counts[0]=counts[1]=0;recent=lastClick;
    op1f_unmask(mask);
    if(recent==2)requested=-1;
    for(unsigned axis=0;axis<2;++axis){
        if(!steps[axis])continue;
        int stored;
        if(op1f_param_get(&op1f_card_plonk,0,slot,4+axis,&stored))continue;
        unsigned index=((axis?unsigned(plonk::spring_setting(stored)):waveMode(stored))+steps[axis])%3;
        const unsigned word=axis?plonk::spring_words[index]:waveWords[index];
        if(op1f_param_set(&op1f_card_plonk,0,slot,4+axis,word)==0 && axis==recent){
            requested=(axis?3:0)+int(index);until=ms+1450;
        }
    }
}
}
extern "C" void plonk_shown(unsigned slot,unsigned how) OP1F_NAMED("plonk_shown");
extern "C" void plonk_shown(unsigned slot,unsigned how){
    const bool visible=(how&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP;
    turns.shown(slot,visible);
    if(place!=slot||!visible){counts[0]=counts[1]=0;requested=-1;}
    place=slot;shown=how;
}
extern "C" int plonk_input(const Op1fInput* e) OP1F_NAMED("plonk_input");
extern "C" int plonk_input(const Op1fInput* e){
    if(e->bytes<40)return 0;
    if(turns.take(e))return 1;
    if(!e->mine || e->shift || place>=8 || e->thread!=OP1F_KEY_THREAD ||
       (shown&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))!=OP1F_SHOWN_UP ||
       (e->key<54 || e->key>56))return 0;
    if(e->what==OP1F_UP)return 1;
    if(e->what!=OP1F_DOWN)return 0;
    const unsigned axis=e->key==56;
    const unsigned mask=op1f_mask();
    if(e->key==55){turns.snapRatio();lastClick=2;}
    else {++counts[axis];lastClick=axis;}
    op1f_unmask(mask);
    return 1;
}
extern "C" void plonk_frame(const Op1fFrame* f) OP1F_NAMED("plonk_frame");
extern "C" void plonk_frame(const Op1fFrame* f){
    if(f->bytes<40)return;
    if(!doc)bind();
    if(!stood&&doc)stood=op1f_page_stand_in(&op1f_card_plonk,0,doc)!=0;
    const unsigned slot=place;
    turns.apply(&op1f_card_plonk,0,slot);
    if(slot>=8 || !(shown&OP1F_SHOWN_UP) || f->page!=&op1f_card_plonk || !doc)return;
    clicks(slot,f->ms);
    if(requested>=0&&int(f->ms-until)>=0)requested=-1;
    setCard(requested);
    draw(slot,f->ms);
    if((shown&OP1F_SHOWN_OFF)&&f->canvas)
        op1f_paint_text(f->canvas,8,8,"switched off",0xffff,1);
}
