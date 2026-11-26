#include "standard-fx-turns.hpp"

extern "C" int pong_input(const Op1fInput*) OP1F_NAMED("pong_input");
OP1F_STANDARD_FX_BRIDGE(pong, 32767, pong_input)
