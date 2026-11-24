#pragma once
#include "build_r6e_card.h"
extern "C" const Op1fCard op1f_card_sag OP1F_NAMED("op1f_card_sag");
void sag_show_state(unsigned place, unsigned flags);
void sag_settings_frame(unsigned ms);
unsigned sag_mode(unsigned place,unsigned lane);
int sag_popup(unsigned place,unsigned ms);
unsigned sag_meter(unsigned place,unsigned index);
void sag_publish_meter(unsigned place,unsigned index,unsigned q15);
extern "C" void sag_shown(unsigned,unsigned) OP1F_NAMED("sag_shown");
extern "C" void sag_frame(const Op1fFrame*) OP1F_NAMED("sag_frame");
