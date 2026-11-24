// Compile these calls together so the pack's hot oscillator reads can inline.
// The original bank reader is used separately by the host parity oracle.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3")
#endif
#if defined(__bfin__)
#define PPG_PACK_ENTRY_ONLY
#endif
#include "core/ppg_native_oscillator.cpp"
