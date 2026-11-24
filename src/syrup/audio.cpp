#include "syrup.hpp"

volatile unsigned syrup_clip_events[OP1F_PLACE_MASTER + 1];

extern "C" void syrup_reset(unsigned, void* memory) {
    if (memory) static_cast<SyrupPlace*>(memory)->processing = false;
}

extern "C" void syrup_process(const Op1fEffectBlock* block) {
    if (!block || block->bytes < 44 || !block->state) return;
    auto& place = *static_cast<SyrupPlace*>(block->state);
    if (!block->on) {
        place.processing = false;
        return;
    }
    if (block->frames != 64 || !block->left || !block->right || !block->values) return;
    if (block->fresh || !place.processing) {
        syrup::reset(place.dsp, 44100);
        place.processing = true;
    }
    const int* v = block->values;
    const syrup::Parameters heard{uint16_t(v[0]), uint16_t(v[1]), uint16_t(v[2]), uint16_t(v[3]), 32768};
    const bool previous_clip = place.dsp.clipped;
    // The target libc calls int32_t "long" while the public ABI calls an identical Q31 word "int".
    static_assert(sizeof(int) == sizeof(int32_t), "Q31 buffer word width");
    auto* left = reinterpret_cast<int32_t*>(block->left);
    auto* right = reinterpret_cast<int32_t*>(block->right);
    syrup::process(place.dsp, heard, left, right, left, right, 64);
    if (place.dsp.clipped && !previous_clip && block->place <= OP1F_PLACE_MASTER)
        ++syrup_clip_events[block->place];
}
