#pragma once
#include <stdint.h>
// Exact owned desktop codecs, expressed without target floating point/runtime.
// No allocator, state, native IDs, hardware ABI or lifetime/publication API.
extern "C" {
bool pong_encode_halfword(int32_t halfword,unsigned bits,int32_t* code);
bool pong_decode_q31(int32_t code,unsigned bits,int32_t* sample);
}
