#include "standard-fx-turns.hpp"

extern "C" int split_input(const Op1fInput*) OP1F_NAMED("split_input");

OP1F_STANDARD_FX_BRIDGE(split, 32767, split_input)
