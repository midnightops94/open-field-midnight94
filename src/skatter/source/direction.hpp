#pragma once
#include "build_r6e_card.h"

namespace skatter_display {
constexpr unsigned direction_ms=1350;
static const char* const names[]={"forward","both","reverse"};
static const char* const icons[]={"sDirectionForward","sDirectionBoth","sDirectionReverse"};

struct DirectionPopup {
    void* group;
    void* arrows[3];
    unsigned mode,start;
    int drawn;
    bool open,ready;

    void bind(void* doc) {
        group=op1f_node(doc,"sDirectionCard");
        if(!group) return;
        op1f_show(group,0);
        ready=op1f_text_set(doc,"sDirectionLabel","direction",165,OP1F_CENTRE)!=0;
        for(unsigned i=0;i<3;++i) {
            arrows[i]=op1f_node(doc,icons[i]);
            ready=ready && arrows[i]!=nullptr;
        }
    }
    void select(unsigned choice,unsigned ms) {
        mode=choice;
        start=ms;
        open=true;
    }
    void draw(void* doc,void* canvas,bool installed,unsigned ms) {
        if(open && ms-start>=direction_ms) open=false;
        const int next=open && installed && ready?int(mode):-1;
        if(next!=drawn) {
            drawn=next;
            if(installed && ready && open) {
                ready=op1f_text_set(doc,"sDirectionValue",names[mode],165,OP1F_CENTRE)!=0;
                for(unsigned i=0;i<3;++i)op1f_show(arrows[i],i==mode);
            }
            if(group)op1f_show(group,installed && ready && open);
        }
        if(!open || (ready && installed))return;
        const unsigned cream=OP1F_COLOUR(205,208,194),black=OP1F_COLOUR(0,0,0);
        const unsigned gray=OP1F_COLOUR(162,164,166),ink=OP1F_COLOUR(17,23,27);
        op1f_paint_box(canvas,108,75,265,70,cream);
        op1f_paint_box(canvas,179,80,189,60,black);
        op1f_paint_box(canvas,113,80,60,60,gray);
        const int label_w=op1f_paint_text(canvas,0,-32,"direction",cream,2);
        const int value_w=op1f_paint_text(canvas,0,-32,names[mode],cream,2);
        op1f_paint_text(canvas,274-(label_w>>1),90,"direction",cream,2);
        op1f_paint_text(canvas,274-(value_w>>1),115,names[mode],cream,2);
        op1f_paint_line(canvas,129,110,157,110,2,ink);
        if(mode!=2) {
            op1f_paint_line(canvas,150,103,157,110,2,ink);
            op1f_paint_line(canvas,150,117,157,110,2,ink);
        }
        if(mode!=0) {
            op1f_paint_line(canvas,136,103,129,110,2,ink);
            op1f_paint_line(canvas,136,117,129,110,2,ink);
        }
    }
};
}
