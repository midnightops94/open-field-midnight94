#pragma once
#include "build_r6e_card.h"
#include "dsp/instrument_choice.hpp"
extern "C" const Op1fCard op1f_card_pong OP1F_NAMED("op1f_card_pong");
extern volatile unsigned pong_modes[17]; // one encoded choice word per sound or master
