#include "standard-fx-turns.hpp"

extern "C" int squash_input(const Op1fInput*) OP1F_NAMED("squash_input");

OP1F_STANDARD_FX_BRIDGE(squash, 32767, squash_input)
