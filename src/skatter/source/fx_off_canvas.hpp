#pragma once
#include "build_r6e_card.h"

// Last-resort rendering if both page resources or the fallback font are unavailable.
inline void draw_fx_off_card(void* canvas) {
    if(!canvas) return;
    const unsigned ink=OP1F_COLOUR(205,208,194);
    const unsigned purple=OP1F_COLOUR(86,47,73);
    op1f_paint_box(canvas,110,77,260,66,ink);
    op1f_paint_box(canvas,179,80,188,60,0);
    op1f_paint_box(canvas,113,80,60,60,purple);
    op1f_paint_box(canvas,126,98,34,24,ink);
    op1f_paint_text(canvas,132,103,"fx",purple,2);
    const int label=op1f_paint_text(canvas,0,-25,"fx off",ink,2);
    op1f_paint_text(canvas,273-label/2,103,"fx off",ink,2);
}
