#pragma once
#include "build_r6e_card.h"
extern "C" const Op1fCard op1f_card_ampig OP1F_NAMED("op1f_card_ampig");
void ampig_show_state(unsigned place, unsigned flags);
void ampig_settings_frame(unsigned ms);
unsigned ampig_mode(unsigned place,unsigned lane);
int ampig_popup(unsigned place,unsigned ms);
unsigned ampig_meter(unsigned place,unsigned index);
void ampig_publish_meter(unsigned place,unsigned index,unsigned q15);
extern "C" void ampig_shown(unsigned,unsigned) OP1F_NAMED("ampig_shown");
extern "C" void ampig_frame(const Op1fFrame*) OP1F_NAMED("ampig_frame");
