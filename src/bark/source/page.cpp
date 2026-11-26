#include "build_r6e_card.h"
#include "motion.hpp"
#include "engine_turns.hpp"
#include "contact.hpp"

extern volatile unsigned bark_notes[8];
extern "C" const Op1fCard op1f_card_bark OP1F_NAMED("op1f_card_bark");

namespace {
EngineTurns turns{};
const char* const contactNames[]={"tine hammer","reed hammer","sticky pad","string hammer","felt","rubber","wood"};
unsigned materialClicks[8],usedMaterialClicks[8],cardUntil;
void* tip[4][2][4];
void* materialIcons[4];
void* materialCard;
void* contactIcons[4];
void* valueGroup;
void* stringRow;
void* stringFixed;
int displayedContact=-1;
int displayedMaterial=-1;
bool cardVisible;
const char* const titles[]={"rhodes","wurli","pianet","cp-70"};
const char* const labels[]={"color","strike","bark","trem"};
const char* const cornerLabels[]={"btmL0","btmL1","btmL2","btmL3"};
const char* const cornerValues[]={"btmN0","btmN1","btmN2","btmN3"};
volatile unsigned where, shown, clicks[8];
unsigned consumed[8], previousNote, lastMs, visibleSlot=8;
void* doc;
bool tried, stood;
void* groups[4];
void* movers[24];
void* swatches[sizeof(pigments)/sizeof(pigments[0])][5];
int oldShade[sizeof(pigments)/sizeof(pigments[0])];
int previousAxis[3]={-1,-1,-1};
int oldPiano=-1, oldValue[4]={-1,-1,-1,-1};
void visibility(void* node,bool on) {if(node)op1f_show(node,on);}
bool load() {
    if(tried) return doc!=0;
    tried=true;
    doc=op1f_picture(&op1f_card_bark,"bark-page.svg");
    if(!doc)return false;
    char group[]="tmF0",part[]="AtmM0",swatch[]="c0000";
    // Scale the common parent around (240,110). Dynamic child transforms remain in artwork coordinates.
    const int scale[9]={928,0,0, 0,928,0, 23040,10560,1024};
    for(unsigned f=0;f<4;++f) {
        group[3]=char('0'+f);groups[f]=op1f_node(doc,group);
        visibility(groups[f],false);
        if(groups[f])op1f_move(groups[f],scale);
        for(unsigned j=0;j<6;++j){
            part[0]=char('A'+f); part[4]=char('0'+j);
            movers[6*f+j]=op1f_node(doc,part);
        }
    }
    char tipId[]="bkTipA10",iconId[]="bkMaterialIcon0";
    for(unsigned m=0;m<4;++m){
        iconId[14]=char('0'+m);materialIcons[m]=op1f_node(doc,iconId);visibility(materialIcons[m],false);
        for(unsigned f=0;f<4;++f)for(unsigned side=0;side<2;++side){
            tipId[5]=char('A'+f);tipId[6]=char('1'+side);tipId[7]=char('0'+m);
            tip[f][side][m]=op1f_node(doc,tipId);visibility(tip[f][side][m],false);
        }
    }
    char contactId[]="bkContactIconA";
    for(unsigned piano=0;piano<4;++piano){
        contactId[13]=char('A'+piano);
        contactIcons[piano]=op1f_node(doc,contactId);
        visibility(contactIcons[piano],false);
    }
    valueGroup=op1f_node(doc,"bkValueFit");
    stringRow=op1f_node(doc,"bkStringRow");
    stringFixed=op1f_node(doc,"bkStringFixed");
    visibility(stringRow,false);
    visibility(stringFixed,false);
    materialCard=op1f_node(doc,"bkMaterialCard");
    visibility(materialCard,false);
    op1f_text_set(doc,"bkMaterialLabel","contact",165,OP1F_CENTRE);
    for(unsigned i=0;i<sizeof(pigments)/sizeof(pigments[0]);++i) {
        unsigned n=pigments[i][1];
        swatch[1]=char('0'+pigments[i][0]);swatch[2]=char('0'+(n>=10));swatch[3]=char('0'+(n>=10?n-10:n));
        for(unsigned k=0;k<5;++k){
            swatch[4]=char('0'+k);swatches[i][k]=op1f_node(doc,swatch);visibility(swatches[i][k],k==0);
        }
        oldShade[i]=0;
    }
    for(unsigned i=0;i<4;++i){
        op1f_text_set(doc,cornerLabels[i],labels[i],0,i<2?OP1F_LEFT:OP1F_RIGHT);
    }
    return true;
}
void labelContact(unsigned material,unsigned piano){
    const int next=int(material?material+3:piano);
    if(next==displayedContact)return;
    if(displayedContact>=0 && displayedContact<4)visibility(contactIcons[displayedContact],false);
    if(next<4)visibility(contactIcons[next],true);
    // The two-word CP-70 contact has its own centered 13 px native row.
    // Use fixed artwork if this base cannot fit every glyph inside 165 px.
    const bool stringHammer=next==3;
    visibility(valueGroup,!stringHammer);
    if(stringHammer){
        const bool laid=op1f_text_set(doc,"bkStringNative",contactNames[next],0,OP1F_CENTRE)!=0;
        const int width=laid?op1f_text_width(doc,"bkStringNative"):0;
        const bool fits=width>0 && width<=162;
        visibility(stringRow,fits);
        visibility(stringFixed,!fits);
        displayedContact=next;
        return;
    }
    visibility(stringRow,false);
    visibility(stringFixed,false);
    // The base measures the actual instrument lettering. Lay out the complete
    // name first, then fit the parent group without truncating it.
    const int unscaled[9]={1024,0,0,0,1024,0,0,0,1024};
    if(valueGroup)op1f_move(valueGroup,unscaled);
    if(!op1f_text_set(doc,"bkMaterialValue",contactNames[next],0,OP1F_CENTRE))return;
    const int width=op1f_text_width(doc,"bkMaterialValue");
    int scale=1024;
    while(scale>384 && width*scale>165*1024)scale-=16;
    if(valueGroup && scale!=1024){
        // Preserve the text center at (273.5, 121.85) in Q10 coordinates.
        const int delta=1024-scale;
        const int offsetY=(124774*delta+512)>>10;
        const int fit[9]={scale,0,0,0,scale,0,547*delta/2,offsetY,1024};
        op1f_move(valueGroup,fit);
    }
    displayedContact=next;
}
void two_digits(unsigned value, char (&text)[3]) {
    const unsigned tens=(value*205)>>11;
    text[0]=char('0'+tens);
    text[1]=char('0'+value-10*tens);
    text[2]=0;
}
void draw(unsigned slot,unsigned ms) {
    int v[5];
    for(unsigned i=0;i<5;++i)
        if(op1f_param_heard(&op1f_card_bark,0,slot,i,&v[i])!=0)return;
    const unsigned piano=unsigned(v[4])>>13;
    if(piano>3)return;
    if(oldPiano!=int(piano)) {
        if(oldPiano>=0)visibility(groups[oldPiano],false);
        visibility(groups[piano],true);oldPiano=int(piano);
        for(auto& axis:previousAxis)axis=-1;
        op1f_text_set(doc,"tmTitle",titles[piano],0,OP1F_CENTRE);
    }
    for(unsigned i=0;i<4;++i){
        const unsigned n=(unsigned(v[i])*99u+16383u)/32767u;
        if(oldValue[i]!=int(n)){
            char s[3];two_digits(n,s);
            op1f_text_set(doc,cornerValues[i],s,0,i<2?OP1F_LEFT:OP1F_RIGHT);
            oldValue[i]=int(n);
        }
    }
    if(slot!=visibleSlot){previousNote=bark_notes[slot];lastMs=ms-1000;visibleSlot=slot;}
    if(previousNote!=bark_notes[slot]){previousNote=bark_notes[slot];lastMs=ms;}
    const unsigned age=ms-lastMs;
    const int hit=age<48 ? int(age)*21 : age<128 ? int(128-age)*12 : 0;
    const int travel=(hit*(512+(v[1]>>6)))>>10;
    const int ring=age<128 ? int(128-age)*((age&16)?-3:3):0;
    const int axes[]={piano==2?travel:1024-travel,512+ring,v[2]>>5};
    for(unsigned j=0;j<5;++j){
        unsigned index=6*piano+j;
        const auto* role=motion[index];
        if(previousAxis[role[2]]==axes[role[2]])continue;
        const int t=axes[role[2]]*(role[1]-1);
        auto* pose=matrices[role[0]+(t>>10)];
        auto* next=matrices[role[0]+((t>>10)+1<role[1]?(t>>10)+1:t>>10)];
        int matrix[9]={0,0,0,0,0,0,0,0,1024};
        const int idx[]={0,1,3,4,6,7};
        for(int b=0;b<6;++b) matrix[idx[b]]=pose[b]+(((next[b]-pose[b])*(t&1023))>>10);
        if(movers[index])op1f_move(movers[index],matrix);
    }
    for(unsigned i=0;i<3;++i)previousAxis[i]=axes[i];
    for(unsigned i=0;i<sizeof(pigments)/sizeof(pigments[0]);++i){
        if(pigments[i][0]!=piano)continue;
        int axis=pigments[i][2];
        int level=v[axis];
        if(axis==3 && age<200)level=(level*(256+int(200-age)))>>9;
        const int grade=(level*4+16384)>>15;
        if(grade!=oldShade[i]){
            visibility(swatches[i][oldShade[i]],false);visibility(swatches[i][grade],true);oldShade[i]=grade;
        }
    }
}
}
extern "C" void bark_shown(unsigned place,unsigned how) OP1F_NAMED("bark_shown");
extern "C" void bark_shown(unsigned place,unsigned how) {
    where=place;shown=how;
    turns.shown(place,(how&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))==OP1F_SHOWN_UP);
    if(place<8 && !(how&OP1F_SHOWN_UP)) {
        consumed[place]=clicks[place];usedMaterialClicks[place]=materialClicks[place];cardVisible=false;
    }
}
extern "C" int bark_input(const Op1fInput* e) OP1F_NAMED("bark_input");
extern "C" int bark_input(const Op1fInput* e) {
    if(e->bytes<40)return 0;
    if(turns.take(e))return 1;
    if(e->thread!=OP1F_KEY_THREAD || !e->mine || (shown&(OP1F_SHOWN_UP|OP1F_SHOWN_COVERED))!=OP1F_SHOWN_UP ||
       where>=8 || (e->key!=61 && e->key!=54 && e->key!=55) || e->shift || e->what==OP1F_TURN || e->what==OP1F_DOWN_SHIFT)return 0;
    if(e->what==OP1F_DOWN && e->key==61) ++clicks[where];
    if(e->what==OP1F_DOWN && e->key==55) ++materialClicks[where];
    return 1;
}
extern "C" void bark_frame(const Op1fFrame* frame) OP1F_NAMED("bark_frame");
extern "C" void bark_frame(const Op1fFrame* frame) {
    if(frame->bytes<40)return;
    const unsigned slot=where;
    turns.apply(&op1f_card_bark,0,slot);
    if(!stood){load();stood=op1f_page_stand_in(&op1f_card_bark,0,doc)!=0;}
    if(frame->page!=&op1f_card_bark || !(shown&OP1F_SHOWN_UP) || slot>=8)return;
    const unsigned count=clicks[slot],delta=count-consumed[slot];
    if(delta){
        int v;
        if(!op1f_param_get(&op1f_card_bark,0,slot,4,&v))
            op1f_param_set(&op1f_card_bark,0,slot,4,4096+8192*(((unsigned(v)>>13)+delta)&3));
        consumed[slot]=count;
    }
    if(doc)draw(slot,frame->ms);
    const unsigned current=materialClicks[slot],steps=current-usedMaterialClicks[slot];
    int saved;
    if(doc && !op1f_param_get(&op1f_card_bark,0,slot,6,&saved)){
        unsigned material=bark_contact::decode(saved);
        if(steps){
            const unsigned next=(material+steps)&3;
            if(!op1f_param_set(&op1f_card_bark,0,slot,6,bark_contact::words[next])){
                material=next;cardUntil=frame->ms+1350;cardVisible=true;
            }
            usedMaterialClicks[slot]=current;
        }
        if(displayedMaterial!=int(material)){
            for(unsigned f=0;f<4;++f)for(unsigned side=0;side<2;++side){
                if(displayedMaterial>=0)visibility(tip[f][side][displayedMaterial],false);
                visibility(tip[f][side][material],true);
            }
            if(displayedMaterial>=0)visibility(materialIcons[displayedMaterial],false);
            visibility(materialIcons[material],true);
            displayedMaterial=int(material);
        }
        if(oldPiano>=0)labelContact(material,unsigned(oldPiano));
        if(cardVisible&&int(frame->ms-cardUntil)>=0)cardVisible=false;
        visibility(materialCard,cardVisible);
    }
    if((shown&OP1F_SHOWN_OFF) && frame->canvas)op1f_paint_text(frame->canvas,8,8,"switched off",0xffff,1);
}
