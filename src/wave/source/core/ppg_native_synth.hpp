#ifndef PPG_NATIVE_SYNTH_HPP
#define PPG_NATIVE_SYNTH_HPP

#include "ppg_native_oscillator.hpp"
#include "ppg_duty.hpp"

namespace ppg_synth {
using ppg_reader::i32;
using ppg_reader::u32;
constexpr u32 voices = 6;
constexpr u32 presets = 5;
constexpr i32 one = 1 << 30;
constexpr u32 output_block = 64;

struct Controls { i32 q30[4]; };
struct Pitch { u32 step; i32 mip_q16; u32 grain_q16; };

struct Preset {
    Controls controls;
    u32 bank, motion;
    i32 attack_step, decay_rate, sustain;
    u32 release_frames;
};

struct Voice {
    ppg_reader::Voice oscillator;
    Pitch pitch;
    u32 id, order;
    i32 velocity, envelope, attack_step, decay_rate, sustain;
    u32 release_frames, release_left;
    i32 release_step;
    i32 position_q16, held_position_q16;
    u32 hold_left;
    i32 falling, rising, cycle;
    u32 cycle_position, cycle_carry;
    i32 last, steal_tail;
    u32 steal_mix_q16, steal_carry;
    bool active, releasing, in_decay;
};

struct Frame {
    ppg_reader::BankFrame bank;
    ppg_reader::PulseDuty duty;
    i32 base_q16, amount_q16, spread_q16;
    i32 detune_ratio, detune_mip_q16;
    u32 twin_mix_q16, motion;
    i32 master;
};

struct Fir {
    i32 history[31];
    u32 head;
};

struct StereoFir {
    Fir left, right;
};

struct Synth {
    Voice playing[voices];
    ppg_reader::BankFade bank;
    Controls actual, target;
    u32 preset, control_tick, serial;
    i32 master, master_target;
    Fir decimator;
};

static_assert(sizeof(Voice) == 112 && sizeof(Frame) == 60, "PPG voice and frame layout");
static_assert(sizeof(Synth) == 872 && sizeof(StereoFir) == 256, "PPG caller-owned state layout");

void prepare_frame(Synth& synth, Frame& frame);
i32 voice_sample(Voice& voice, const Frame& frame);
}

extern "C" void ppg_v2_synth_initialize_44100(ppg_synth::Synth* synth);
extern "C" bool ppg_v2_synth_preset(ppg_synth::Synth* synth, ppg_synth::u32 preset, bool immediate);
extern "C" bool ppg_v2_synth_bank(ppg_synth::Synth* synth, ppg_synth::u32 bank, bool immediate);
extern "C" bool ppg_v2_synth_controls(ppg_synth::Synth* synth, const ppg_synth::Controls* controls);
extern "C" bool ppg_v2_synth_master(ppg_synth::Synth* synth, ppg_synth::i32 q30);
extern "C" bool ppg_v2_synth_note_on(ppg_synth::Synth* synth, ppg_synth::u32 id, ppg_synth::u32 midi,
                                    ppg_synth::u32 velocity);
extern "C" void ppg_v2_synth_note_off(ppg_synth::Synth* synth, ppg_synth::u32 id);
extern "C" void ppg_v2_synth_panic(ppg_synth::Synth* synth);
extern "C" bool ppg_v2_synth_render_44100(ppg_synth::Synth* synth, ppg_synth::i32* left, ppg_synth::i32* right,
                                         ppg_synth::u32 frames);
extern "C" bool ppg_v2_synth_render_88200(ppg_synth::Synth* synth, ppg_synth::i32* interleaved,
                                         ppg_synth::u32 frames);
extern "C" void ppg_v2_fir_reset(ppg_synth::StereoFir* filters);
// Consumes 128 interleaved Q31 L/R pairs and overwrites the first 64 pairs.
extern "C" bool ppg_v2_fir_in_place_128(ppg_synth::StereoFir* filters, ppg_synth::i32* interleaved);

#endif
