#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3")
#endif
#include "build_r6e_card.h"
// Open-Field owns envelopes, mixing, and decimation. The standalone synth
// helpers remain available when building the desktop reference.
#if defined(__bfin__)
#define PPG_PACK_ENTRY_ONLY
#endif
#include "edges.cpp"
#include "core/ppg_pack_synth.inc"

static_assert(sizeof(ppg_synth::StockVoice) == 184, "Wave voice storage in recipe");
extern "C" void wave_render(const Op1fVoiceBlock* block) OP1F_NAMED("wave_render");
extern "C" void wave_render(const Op1fVoiceBlock* block) {
    using namespace ppg_synth;
    if (!block || !block->out) return;
    auto* voice = static_cast<StockVoice*>(block->state);
    if (block->bytes < 52 || !voice || !block->values || block->frames != 128 || block->rate != 88200) {
        for (unsigned i = 0; i < block->frames; ++i) block->out[i] = 0;
        return;
    }
    int words[8] = {};
    for (unsigned i = 0; i < 4; ++i) words[i] = clamp(block->values[i], 0, 32767) << 16;
    words[4] = clamp(block->values[4], 0, 7) << 28;
    words[5] = clamp(block->values[5], 0, 4) << 28;
    if (block->events & OP1F_VOICE_FRESH) stock_initialize(*voice);
    if (block->events & OP1F_VOICE_BEGINS) stock_start(*voice, words);
    stock_controls(*voice, words, false);
    // Open-Field supplies one pitch for this block. Trial 12 evaluates one
    // sample per pair of native frames and the Field owns the final envelope.
    const unsigned pitch = block->step;
    const unsigned step = pitch + pitch;
    const unsigned previous = voice->last_pitch;
    const unsigned difference = previous < pitch ? pitch - previous : previous - pitch;
    const bool pitched = pitch && pitch <= 0x3fffffffu;
    if (!voice->active || !pitched) {
        for (unsigned i = 0; i < block->frames; i += 2) {
            if (voice->active) prepare_stock(*voice);
            block->out[i] = block->out[i + 1] = 0;
        }
        return;
    }
    if (difference > (previous >> 10)) {
        voice->last_pitch = pitch;
        voice->spectral.pitch = field_pitch(pitch);
        voice->cached_pair_step = 0;
    }
    const auto* table = wave_tables_ready();
    if (table) {
        for (unsigned i = 0; i < block->frames; i += 2) {
            prepare_stock(*voice);
            block->out[i] = block->out[i+1] = stock_sample<true>(*voice, step, table);
        }
    } else {
        // Events are consumed above even while the shared table is pending.
        // Keep motion and phases advancing so a held note joins at its normal phase.
        for (unsigned i = 0; i < block->frames; i += 2) {
            prepare_stock(*voice);
            stock_sample<false>(*voice, step, nullptr);
            block->out[i] = block->out[i+1] = 0;
        }
    }
}
