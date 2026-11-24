#pragma once
#include "build_r6e_card.h"
#include "dsp/core.hpp"

struct SyrupPlace {
    syrup::State dsp;
    bool processing;
};
constexpr unsigned syrup_state_bytes = 232;
static_assert(sizeof(SyrupPlace) == syrup_state_bytes, "update the effect's state_bytes in the recipe");

extern "C" {
extern const Op1fCard op1f_card_syrup OP1F_NAMED("op1f_card_syrup");
void syrup_process(const Op1fEffectBlock*) OP1F_NAMED("syrup_process");
void syrup_reset(unsigned, void*) OP1F_NAMED("syrup_reset");
void syrup_shown(unsigned, unsigned) OP1F_NAMED("syrup_shown");
void syrup_frame(const Op1fFrame*) OP1F_NAMED("syrup_frame");
}
extern volatile unsigned syrup_clip_events[OP1F_PLACE_MASTER + 1];
