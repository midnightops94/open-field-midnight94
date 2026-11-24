#include "pong.hpp"
#include "dsp/extended_court.hpp"
#include "dsp/slot.hpp"
#include "dsp/sync_word.hpp"

// Construct a fresh control object in base-owned, already-aligned memory.
// Valid-length guards in the original packed rings make clearing history unnecessary.
inline void* operator new(__SIZE_TYPE__, void* at) noexcept { return at; }
volatile unsigned pong_modes[17];
namespace {
constexpr unsigned ring_frames = 262144, history_bytes = 786432;
struct State {
    pong_slot::Slot slot;
    unsigned quiet;
    int32_t tail_l[64], tail_r[64];
    unsigned char history[history_bytes];
};
static_assert(sizeof(State) <= 790528, "Pong working state exceeds the recipe");
const int32_t silence[64] = {};

pong_proto::Params parameters(const int* v, pong_instrument_choice::Choice choice, unsigned period) {
    pong_proto::Params p{};
    p.feedback = pong_rally::feedback(pong_rally::repeats(unsigned(v[1])));
    p.wall = choice.wall == 0 ? 8192 : choice.wall == 1 ? 24576 : 65536;
    p.wall_loss = choice.wall == 0 ? 49152 : choice.wall == 1 ? 57344 : 65536;
    p.parallel = choice.parallel;
    p.mix = mod_fx::unit(static_cast<int16_t>(v[3]));
    p.duck = 8192; p.duck_attack = 149; p.duck_release = 7;
    const int court = static_cast<int>(unsigned(v[0]) << 16);
    const int spin = static_cast<int>(unsigned(v[2]) << 16);
    if (choice.sync && period != 0) {
        // Base period = 1/96 beat in Q18 frames. The owner's word = beat in Q14 frames.
        const unsigned beat = period > 0xffffffffu / 6u ? 0xffffffffu : period * 6u;
        pong_sync_word::Result timing{};
        if (pong_sync_word::convert(beat, unsigned(v[0]), unsigned(v[2]), timing) &&
            timing.left < ring_frames && timing.right < ring_frames) {
            p.delay_l = timing.left; p.delay_r = timing.right;
            return p;
        }
    }
    pong_extended_court::map(court, spin, choice.bits, p.delay_l, p.delay_r);
    return p;
}
}

extern "C" void pong_process(const Op1fEffectBlock* block) OP1F_NAMED("pong_process");
extern "C" void pong_process(const Op1fEffectBlock* block) {
    if (!block || block->frames != 64 || !block->state || !block->left || !block->right || !block->values) return;
    State* const s = static_cast<State*>(block->state);
    unsigned mode = block->place < 17 ? pong_modes[block->place] : 0;
    pong_instrument_choice::Choice choice{};
    if (!pong_instrument_choice::decode(mode, choice)) return;
    auto p = parameters(block->values, choice, block->period);
    if (block->fresh) {
        new (&s->slot) pong_slot::Slot();
        s->quiet = ring_frames;
        if (!s->slot.init(s->history, history_bytes, ring_frames, p, choice.bits)) return;
    }
    pong_slot::Work work{};
    if (block->on) {
        s->quiet = 0;
        for(unsigned i=0;i<64;++i){s->tail_l[i]=block->left[i];s->tail_r[i]=block->right[i];}
        if(s->slot.process_block(choice.bits, s->tail_l, s->tail_r, s->tail_l, s->tail_r, 64, p, work)) {
            for(unsigned i=0;i<64;++i){block->left[i]=s->tail_l[i];block->right[i]=s->tail_r[i];}
        }
        return;
    }
    if (s->quiet >= ring_frames) return;
    if (!s->slot.process_block(choice.bits, silence, silence, s->tail_l, s->tail_r, 64, p, work)) return;
    unsigned activity = 0;
    for (unsigned i = 0; i < 64; ++i) {
        activity |= unsigned(s->tail_l[i]) | unsigned(s->tail_r[i]);
        block->left[i] = mod_fx::saturated_add(block->left[i], s->tail_l[i]);
        block->right[i] = mod_fx::saturated_add(block->right[i], s->tail_r[i]);
    }
    s->quiet = activity ? 0 : s->quiet + 64;
}
