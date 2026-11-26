#pragma once
#include "build_r6e_card.h"
#include "fx_off_canvas.hpp"

namespace fx_page {
static inline unsigned q15(int v) { return v<0 ? 0u : v>32767 ? 32767u : unsigned(v); }

static inline bool values(const Op1fCard* card,unsigned where,unsigned (&out)[4]) {
    for (unsigned lane=0; lane<4; ++lane) {
        int raw;
        if(op1f_param_heard(card,0,where,lane,&raw)!=0) return false;
        out[lane]=q15(raw);
    }
    return true;
}
static inline void captions(void* canvas,void* document,const char* const (&labels)[4],const unsigned (&v)[4]) {
    static const char* const cap[]={"cap0","cap1","cap2","cap3"};
    static const char* const val[]={"val0","val1","val2","val3"};
    static const unsigned colors[]={OP1F_COLOUR(76,125,168),OP1F_COLOUR(138,101,60),
                                    OP1F_COLOUR(205,208,194),OP1F_COLOUR(204,31,0)};
    for(unsigned lane=0;lane<4;++lane) {
        const unsigned rounded=v[lane]*99u+16383u;
        // Exact division by 32767 for the bounded display numerator, with no runtime helper.
        const unsigned n=(rounded+1u+(rounded>>15))>>15;
        const unsigned tens=(n*205u)>>11;
        const char digits[]={char('0'+tens),char('0'+n-10*tens),0};
        const unsigned align=lane<2?OP1F_LEFT:OP1F_RIGHT;
        if(document && op1f_text_set(document,cap[lane],labels[lane],0,align) &&
                       op1f_text_set(document,val[lane],digits,0,align)) continue;
        unsigned title=0;while(labels[lane][title]) ++title;
        const int y=lane&1 ? 156 : 10;
        // The base bitmap font has seven-pixel capitals. Scale 5 gives 35 px digits.
        op1f_paint_text(canvas,lane<2?13:467-int(6*title-1)*2,y,labels[lane],colors[lane],2);
        op1f_paint_text(canvas,lane<2?13:467-(6*2-1)*5,lane&1?177:33,digits,OP1F_COLOUR(205,208,194),5);
    }
}

struct OffNotice {
    void* group;
    bool native_text;
    int visible;

    void bind(void* document) {
        group=op1f_node(document,"stockFxOff");
        native_text=group!=nullptr;
        if(!group) {
            group=op1f_node(document,"fxOff");
            native_text=group && op1f_text_set(document,"fxOffText","fx off",0,OP1F_CENTRE);
        }
        if(group) op1f_show(group,0);
        visible=-1;
    }

    bool update(unsigned how,bool installed) {
        const bool off=(how & OP1F_SHOWN_OFF) && !(how & OP1F_SHOWN_COVERED);
        const bool show=off && installed && native_text;
        if(group && visible!=int(show)) {
            op1f_show(group,show);
            visible=show;
        }
        return off;
    }

    void fallback(void* canvas,bool off) const {
        if(!off || visible==1) return;
        draw_fx_off_card(canvas);
    }
};
}
