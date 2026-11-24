#pragma once
#include "build_r6e_card.h"
extern "C" const Op1fCard op1f_card_split OP1F_NAMED("op1f_card_split");
void split_show_state(unsigned place, unsigned flags);
void split_settings_frame(unsigned ms);
unsigned split_mode(unsigned place,unsigned lane);
int split_popup(unsigned place,unsigned ms);
unsigned split_meter(unsigned place,unsigned index);
void split_publish_meter(unsigned place,unsigned index,unsigned q15);
extern "C" void split_shown(unsigned,unsigned) OP1F_NAMED("split_shown");
extern "C" void split_frame(const Op1fFrame*) OP1F_NAMED("split_frame");
