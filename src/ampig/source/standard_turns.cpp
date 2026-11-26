#include "standard-fx-turns.hpp"

extern "C" int ampig_input(const Op1fInput*) OP1F_NAMED("ampig_input");

OP1F_STANDARD_FX_BRIDGE(ampig, 32767, ampig_input)
