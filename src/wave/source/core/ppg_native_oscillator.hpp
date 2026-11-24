#ifndef PPG_NATIVE_OSCILLATOR_HPP
#define PPG_NATIVE_OSCILLATOR_HPP

#include "ppg_native_reader.hpp"

namespace ppg_reader {
constexpr u32 max_internal_block = 128;
struct BankFrame { u32 bank, previous, mix_q16; };
struct BankFade { BankFrame frame; u32 carry, target; };

// A new request received midfade starts when the current fade completes.
// Updating the target costs one word and keeps the two-table render bounded.
template<u32 frames>
inline void advance_fade(BankFade& fade) {
    if (fade.frame.mix_q16 >= one_q16) {
        if (fade.frame.bank == fade.target) return;
        fade.frame = {fade.target, fade.frame.bank, 0};
        fade.carry = frames / 2;
    }
    fade.frame.mix_q16 += one_q16 / frames;
    fade.carry += one_q16 % frames;
    if (fade.carry >= frames) {
        ++fade.frame.mix_q16;
        fade.carry -= frames;
    }
}
struct Voice { u32 phase, twin_phase; };
struct OscillatorFrame {
    u32 step_q32, twin_step_q32;
    u32 mip_q16, twin_mip_q16;
    u32 position_q16, twin_position_q16;
    u32 grain_q16, twin_mix_q16;
};
}

extern "C" void ppg_v2_bank_reset(ppg_reader::BankFade* fade);
extern "C" bool ppg_v2_bank_select(ppg_reader::BankFade* fade, ppg_reader::u32 bank, bool immediate);
extern "C" bool ppg_v2_bank_prepare_88200(ppg_reader::BankFade* fade, ppg_reader::BankFrame* frames,
                                          ppg_reader::u32 count);
extern "C" bool ppg_v2_render_oscillator(ppg_reader::Voice* voice, const ppg_reader::BankFrame* bank,
                                         const ppg_reader::OscillatorFrame* controls, ppg_reader::i32* output,
                                         ppg_reader::u32 count);
extern "C" ppg_reader::i32 ppg_v2_step_oscillator(ppg_reader::Voice* voice, const ppg_reader::BankFrame* bank,
                                                const ppg_reader::OscillatorFrame* control);

#endif
