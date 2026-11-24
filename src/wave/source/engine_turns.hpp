#pragma once

#include "build_r6e_card.h"

// Four continuous 0..32767 engine parameters. The input callback receives 1024
// counts per detent. Only the frame callback is permitted to write parameters.
struct EngineTurns {
    static constexpr int maximum = 32767;
    struct Move {
        int offset, low, high;
        bool pending;
    };

    Move move[4];
    int remainder[4][2];
    unsigned owner; // displayed synth slot + 1, or zero

    static int clamp(int value, int low, int high) {
        return value < low ? low : value > high ? high : value;
    }

    // Call from shown(). A new page never inherits another sound's old gesture.
    void shown(unsigned slot, bool visible) {
        const unsigned next = visible && slot < 8 ? slot + 1 : 0;
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

    int take(const Op1fInput* event) {
        if (!event->mine || event->what != OP1F_TURN || event->key >= 4) return 0;
        const unsigned axis = event->key, fine = event->shift != 0;
        // One event larger than a full sweep already ends at its stop.
        const int counts = clamp(event->turn, -128 * 1024, 128 * 1024);
        const unsigned mask = op1f_mask();
        if (!owner) {
            op1f_unmask(mask);
            return 0;
        }
        int& rest = remainder[axis][fine];
        const int total = counts + rest, steps = total / 1024;
        rest = total - steps * 1024;
        if (steps) {
            Move& m = move[axis];
            if (!m.pending) {
                m.offset = m.low = 0;
                m.high = maximum;
                m.pending = true;
            }
            // 3 or 1 on a 0..100 display, without snapping saved positions.
            const int delta = steps * (fine ? 328 : 983);
            m.low = clamp(m.low + delta, 0, maximum);
            m.high = clamp(m.high + delta, 0, maximum);
            // clamp(x + offset, low, high) composes turns in order, including
            // immediate reversals at a stop, using fixed storage per control.
            m.offset = m.low == m.high ? 0 : m.offset + delta;
        }
        op1f_unmask(mask);
        return 1;
    }

    // Call from the pack frame before painting or requiring a canvas. Read the
    // stored position here: the last block's LFO result is for display only.
    void apply(const Op1fCard* card, unsigned ask, unsigned slot) {
        for (unsigned axis = 0; axis < 4; ++axis) {
            const unsigned mask = op1f_mask();
            if (slot >= 8 || owner != slot + 1 || !move[axis].pending) {
                op1f_unmask(mask);
                continue;
            }
            const Move m = move[axis];
            move[axis].pending = false;
            op1f_unmask(mask);
            int base = 0;
            if (op1f_param_get(card, ask, slot, axis, &base) == 0) {
                const int next = clamp(base + m.offset, m.low, m.high);
                if (next != base) op1f_param_set(card, ask, slot, axis, next);
            }
        }
    }
};
