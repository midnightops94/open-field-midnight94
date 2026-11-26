#include "standard-fx-turns.hpp"

extern "C" int skatter_input(const Op1fInput*) OP1F_NAMED("skatter_input");
OP1F_STANDARD_FX_BRIDGE(skatter, 32767, skatter_input)
