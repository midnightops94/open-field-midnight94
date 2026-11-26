#pragma once

#include "build_r6e_card.h"

// Use the synth packs' queued adjustments at the effect's own parameter scale.
// The key thread records detents. The display frame alone writes knob values.
template<int Maximum>
struct StandardFxTurns {
    static constexpr unsigned places = 17; // eight synth, eight drum, master
    static constexpr int fine_step = (Maximum + 50) / 100;
    static constexpr int coarse_step = (3 * Maximum + 50) / 100;
    struct Move {
        int offset, low, high;
        bool pending;
    };

    Move move[4];
    int remainder[4][2];
    unsigned owner; // effect place plus one, or zero when hidden or covered

    static int clamp(int value, int low, int high) {
        return value < low ? low : value > high ? high : value;
    }

    void shown(unsigned place, unsigned flags) {
        const bool visible = (flags & OP1F_SHOWN_UP) && !(flags & OP1F_SHOWN_COVERED);
        const unsigned next = visible && place < places ? place + 1 : 0;
        const unsigned mask = op1f_mask();
        if (owner != next) {
            for (unsigned axis = 0; axis != 4; ++axis) {
                move[axis].pending = false;
                remainder[axis][0] = remainder[axis][1] = 0;
            }
            owner = next;
        }
        op1f_unmask(mask);
    }

    bool take(const Op1fInput* event) {
        if (!event->mine || event->what != OP1F_TURN || event->key >= 4) return false;
        const unsigned axis = event->key, fine = event->shift != 0;
        const int counts = clamp(event->turn, -128 * 1024, 128 * 1024);
        const unsigned mask = op1f_mask();
        if (!owner) {
            op1f_unmask(mask);
            return false;
        }
        int& rest = remainder[axis][fine];
        const int total = counts + rest, steps = total / 1024;
        rest = total - steps * 1024;
        if (steps) {
            Move& adjustment = move[axis];
            if (!adjustment.pending) adjustment = {0, 0, Maximum, true};
            const int delta = steps * (fine ? fine_step : coarse_step);
            // Composing bounded moves in order preserves reversal at the stops.
            adjustment.low = clamp(adjustment.low + delta, 0, Maximum);
            adjustment.high = clamp(adjustment.high + delta, 0, Maximum);
            adjustment.offset = adjustment.low == adjustment.high ? 0 : adjustment.offset + delta;
        }
        op1f_unmask(mask);
        return true;
    }

    void apply(const Op1fCard* card, const Op1fFrame* frame) {
        if (!frame || frame->bytes < 40 || frame->page != card || frame->disk) return;
        for (unsigned axis = 0; axis != 4; ++axis) {
            const unsigned mask = op1f_mask();
            if (!owner || !move[axis].pending) {
                op1f_unmask(mask);
                continue;
            }
            const unsigned place = owner - 1;
            const Move adjustment = move[axis];
            move[axis].pending = false;
            op1f_unmask(mask);
            int saved = 0;
            // Read the stored knob. Heard values are for animation, not edits.
            if (op1f_param_get(card, 0, place, axis, &saved) == 0) {
                const int next = clamp(saved + adjustment.offset, adjustment.low, adjustment.high);
                if (next != saved) op1f_param_set(card, 0, place, axis, next);
            }
        }
    }
};

// Keep a pack's original page, visibility and input callbacks in place.
// A legacy input callback may observe a turn before we consume it.
inline int standard_fx_input_result(const Op1fInput* event,
                                    int (*legacy)(const Op1fInput*)) {
    return legacy ? legacy(event) : 0;
}

#define OP1F_STANDARD_FX_BRIDGE(name, maximum, legacy_input) \
    extern "C" const Op1fCard op1f_card_##name OP1F_NAMED("op1f_card_" #name); \
    extern "C" void name##_frame(const Op1fFrame*) OP1F_NAMED(#name "_frame"); \
    extern "C" void name##_shown(unsigned, unsigned) OP1F_NAMED(#name "_shown"); \
    namespace { StandardFxTurns<maximum> standard_turns; } \
    extern "C" void name##_standard_frame(const Op1fFrame* f) OP1F_NAMED(#name "_standard_frame"); \
    extern "C" void name##_standard_frame(const Op1fFrame* f) { \
        standard_turns.apply(&op1f_card_##name, f); \
        name##_frame(f); \
    } \
    extern "C" void name##_standard_shown(unsigned p, unsigned how) OP1F_NAMED(#name "_standard_shown"); \
    extern "C" void name##_standard_shown(unsigned p, unsigned how) { \
        standard_turns.shown(p, how); \
        name##_shown(p, how); \
    } \
    extern "C" int name##_standard_input(const Op1fInput* e) OP1F_NAMED(#name "_standard_input"); \
    extern "C" int name##_standard_input(const Op1fInput* e) { \
        const int previous = standard_fx_input_result(e, legacy_input); \
        if (previous && previous != OP1F_SEEN) return previous; \
        return standard_turns.take(e) ? 1 : previous; \
    }
