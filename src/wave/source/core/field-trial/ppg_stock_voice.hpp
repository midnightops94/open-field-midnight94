#ifndef PPG_STOCK_VOICE_HPP
#define PPG_STOCK_VOICE_HPP

#include "../ppg_native_synth.hpp"
#include "../ppg_duty.hpp"

namespace ppg_synth {
// The Field outer synth owns velocity, envelope, voice stealing and the FIR.
// Each admitted stock voice owns its phase/filter/motion and controls.
struct StockSpectrum {
    ppg_reader::Voice oscillator;
    Pitch pitch;
    i32 position_q16, held_position_q16;
    u32 hold_left;
    i32 falling, rising, cycle;
    u32 cycle_position, cycle_carry;
};
struct StockFrame {
    i32 base_q16, amount_q16, spread_q16;
    i32 detune_ratio, detune_mip_q16;
    u32 twin_mix_q16, motion;
    ppg_reader::PulseDuty duty;
};
struct StockVoice {
    StockSpectrum spectral;
    ppg_reader::BankFade bank;
    Controls actual, target;
    u32 preset, control_tick;
    StockFrame frame;
    u32 last_pitch, controls_pending;
    u32 cached_pair_step, twin_step, primary_mip, twin_mip;
    bool active;
};

void stock_initialize(StockVoice& voice);
void stock_start(StockVoice& voice, const int* saved);
bool stock_render(StockVoice& voice, int* output, const unsigned* pitch, const int* effective, u32 frames);
}

#endif
