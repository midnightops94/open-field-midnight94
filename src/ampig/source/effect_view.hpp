#pragma once
#include "build_r6e_card.h"
#include "fx_off_canvas.hpp"

namespace effect_view {
constexpr unsigned full=32767;
constexpr int one=1024;
constexpr unsigned cream=OP1F_COLOUR(205,208,194);
constexpr unsigned lane_colors[]={
    OP1F_COLOUR(76,125,168),OP1F_COLOUR(138,101,60),cream,OP1F_COLOUR(204,31,0)
};
inline unsigned word(int raw) { return raw<0 ? 0u : raw>int(full) ? full : unsigned(raw); }
inline int unit(unsigned raw) { return int((word(int(raw))*one+16383)/full); }
inline int mul(int a,int b) { return (a*b+512)>>10; }
inline int px(int n) { return n*one; }
inline unsigned step(int amount,unsigned maximum) { return unsigned((amount*int(maximum)+512)>>10); }
inline int clamp_unit(int n) { return n<0 ? 0 : n>one ? one : n; }
inline void node_visible(void* node,bool visible) { if(node) op1f_show(node,visible); }
inline void move(void* node,int x,int y,int xx=one,int xy=0,int yx=0,int yy=one) {
    if(!node) return;
    const int matrix[]={xx,xy,0,yx,yy,0,x,y,one};
    op1f_move(node,matrix);
}
inline void scale(void* node,int x,int y,int sx,int sy) {
    move(node,x-mul(x,sx),y-mul(y,sy),sx,0,0,sy);
}
inline int bitmap_width(const char* text,int scale) {
    unsigned n=0;
    while(text[n])++n;
    return n ? (6*int(n)-1)*scale : 0;
}
inline int sine(unsigned phase) {
    static const short table[]={
        0,100,200,297,392,483,569,650,724,792,851,903,946,980,1004,1019,
        1024,1019,1004,980,946,903,851,792,724,650,569,483,392,297,200,100
    };
    const int a=table[phase&31u];
    return phase&32u ? -a : a;
}

template<unsigned N> struct Atlas {
    void* nodes[N]{};
    int current=-2;

    void bind(void* doc,char prefix) {
        for(unsigned i=0;i<N;++i) {
            char id[]={prefix,char('0'+i/10),char('0'+i%10),0};
            nodes[i]=op1f_node(doc,id);
            node_visible(nodes[i],false);
        }
    }
    void select(int next) {
        if(next==current) return;
        if(current>=0) node_visible(nodes[current],false);
        if(next>=0) node_visible(nodes[next],true);
        current=next;
    }
};

struct Page {
    void* document=nullptr;
    void* card=nullptr;
    void* off_layer=nullptr;
    void* mode_text=nullptr;
    void* choices[6]{};
    void* backup_text[6]{};
    bool attempted=false,installed=false,off_font=false;
    int shown_choice=-1;
    int off_visible=-1;

    bool setup(const Op1fCard* owner,const char* stock_picture,const char* fallback_picture) {
        if(attempted)return false;
        attempted=true;
        document=op1f_picture(owner,stock_picture);
        if(!document) document=op1f_picture(owner,fallback_picture);
        if(!document)return false;
        card=op1f_node(document,"modeCard");
        mode_text=op1f_node(document,"modeNative");
        off_layer=op1f_node(document,"stockFxOff");
        off_font=off_layer!=nullptr;
        if(!off_layer) {
            off_layer=op1f_node(document,"fxOff");
            off_font=off_layer && op1f_text_set(document,"fxOffText","fx off",0,OP1F_CENTRE);
        }
        node_visible(card,false);
        node_visible(off_layer,false);
        node_visible(mode_text,false);
        for(unsigned i=0;i<6;++i) {
            const char index=char('0'+i);
            const char choice[]={'m','o','d','e','C','h','o','i','c','e',index,0};
            const char backup[]={'m','o','d','e','B','a','c','k','u','p',index,0};
            choices[i]=op1f_node(document,choice);
            backup_text[i]=op1f_node(document,backup);
            node_visible(choices[i],false);
            node_visible(backup_text[i],false);
        }
        return true;
    }

    void place(const Op1fCard* owner) {
        if(document && !installed) installed=op1f_page_stand_in(owner,0,document)!=0;
    }

    static bool values(const Op1fCard* owner,unsigned place,unsigned (&out)[4]) {
        for(unsigned i=0;i<4;++i) {
            int raw;
            if(op1f_param_heard(owner,0,place,i,&raw)!=0) return false;
            out[i]=word(raw);
        }
        return true;
    }

    void draw_values(void* canvas,const char* const (&labels)[4],const unsigned (&values)[4]) const {
        for(unsigned i=0;i<4;++i) {
            const unsigned rounded=values[i]*99u+16383u;
            const unsigned v=(rounded+1u+(rounded>>15))>>15;
            const unsigned tens=(v*205u)>>11;
            const char digits[]={char('0'+tens),char('0'+v-tens*10),0};
            const char caption[]={'c','a','p',char('0'+i),0};
            const char number[]={'v','a','l',char('0'+i),0};
            const unsigned align=i<2?OP1F_LEFT:OP1F_RIGHT;
            const bool caption_ready=installed && op1f_text_set(document,caption,labels[i],0,align);
            const bool number_ready=installed && op1f_text_set(document,number,digits,0,align);
            if(!caption_ready)
                op1f_paint_text(canvas,i<2?13:467-bitmap_width(labels[i],2),
                               i&1?156:10,labels[i],lane_colors[i],2);
            if(!number_ready)
                op1f_paint_text(canvas,i<2?13:467-bitmap_width(digits,5),i&1?177:33,digits,cream,5);
        }
    }

    void mode_card(int lane,unsigned mode,const char* title,const char* value) {
        const int next=installed && card && lane>=0 ? lane*3+int(mode) : -1;
        if(next==shown_choice) return;
        if(shown_choice>=0) node_visible(choices[shown_choice],false);
        shown_choice=next;
        if(next<0 || !choices[next]) {
            node_visible(card,false);
            return;
        }
        const bool native=mode_text &&
                          op1f_text_set(document,"modeTitle",title,165,OP1F_CENTRE) &&
                          op1f_text_set(document,"modeValue",value,165,OP1F_CENTRE);
        node_visible(mode_text,native);
        node_visible(backup_text[next],!native);
        node_visible(choices[next],true);
        node_visible(card,true);
    }

    void bypass(void* canvas,bool off) {
        const bool native=off && installed && off_font;
        if(int(native)!=off_visible) {
            off_visible=native;
            node_visible(off_layer,native);
        }
        if(!off || native || !canvas) return;
        draw_fx_off_card(canvas);
    }
};
}
