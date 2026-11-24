#include "ppg_native_oscillator.hpp"
#include "ppg_word_math.hpp"
#include "ppg_duty.hpp"

using namespace ppg_reader;

namespace ppg_reader {
const unsigned short pulse_reciprocal[65] = {
#include "ppg_duty_reciprocal.inc"
};
}

extern "C" void ppg_v2_bank_reset(BankFade* fade) {
    if (fade) *fade = {{0, 0, one_q16}, 220, 0};
}

extern "C" bool ppg_v2_bank_select(BankFade* fade, u32 bank, bool immediate) {
    if (!fade || bank >= banks) return false;
    fade->target = bank;
    if (immediate) fade->frame = {bank, bank, one_q16};
    return true;
}

#ifndef PPG_PACK_ENTRY_ONLY
extern "C" bool ppg_v2_bank_prepare_88200(BankFade* fade, BankFrame* output, u32 count) {
    if (!fade || !output || count > max_internal_block) return false;
    constexpr u32 steps = 441; // Five milliseconds at the approved 88.2 kHz internal cadence.
    for (u32 index = 0; index < count; ++index) {
        advance_fade<steps>(*fade);
        output[index] = fade->frame;
    }
    return true;
}

static i32 bank_sample(const BankFrame& pair, u32 mip, u32 position, u32 phase, u32 grain) {
    const i32 next = ppg_v2_read_q30(pair.bank, mip, position, phase, grain);
    if (pair.mix_q16 >= one_q16 || pair.bank == pair.previous) return next;
    const i32 previous = ppg_v2_read_q30(pair.previous, mip, position, phase, grain);
    return interpolate(previous, next, pair.mix_q16);
}

extern "C" i32 ppg_v2_step_oscillator(Voice* voice, const BankFrame* bank, const OscillatorFrame* control) {
    const auto& p = *control;
    const auto& b = *bank;
    i32 sample = bank_sample(b, p.mip_q16, p.position_q16, voice->phase, p.grain_q16);
    if (p.twin_mix_q16 > 0 && p.twin_mix_q16 <= one_q16) {
        const i32 twin = bank_sample(b, p.twin_mip_q16, p.twin_position_q16, voice->twin_phase, p.grain_q16);
        sample = interpolate(sample, twin, p.twin_mix_q16);
    }
    // The source blends 35% of its Q11 quantizer before the filter.
    constexpr i32 step = 1 << 19;
    const i32 crushed = ((sample + step / 2) >> 19) * step;
    voice->phase += p.step_q32;
    voice->twin_phase += p.twin_step_q32;
    return interpolate(sample, crushed, 22938);
}

extern "C" bool ppg_v2_render_oscillator(Voice* voice, const BankFrame* bank, const OscillatorFrame* controls,
                                         i32* output, u32 count) {
    if (!voice || !bank || !controls || !output || count > max_internal_block) return false;
    for (u32 index = 0; index < count; ++index) {
        output[index] = ppg_v2_step_oscillator(voice, &bank[index], &controls[index]);
    }
    return true;
}

extern "C" i32 ppg_v2_read_banked_q30(const BankFrame*, u32 mip, u32 position, u32 phase, u32 grain);

extern "C" i32 ppg_v2_step_fast(Voice* voice, const BankFrame* bank, const OscillatorFrame* control,
                                const PulseDuty* duty) {
    const auto& p = *control;
    i32 sample = ppg_v2_read_banked_q30(bank, p.mip_q16, p.position_q16, map_duty(voice->phase, *duty), p.grain_q16);
    if (p.twin_mix_q16) {
        const i32 twin = ppg_v2_read_banked_q30(bank, p.twin_mip_q16, p.twin_position_q16,
                                                   map_duty(voice->twin_phase, *duty), p.grain_q16);
        sample = interpolate(sample, twin, p.twin_mix_q16);
    }
    constexpr i32 step = 1 << 19;
    const i32 crushed = ((sample + step / 2) >> 19) * step;
    voice->phase += p.step_q32;
    voice->twin_phase += p.twin_step_q32;
    return interpolate(sample, crushed, 22938);
}
#endif
