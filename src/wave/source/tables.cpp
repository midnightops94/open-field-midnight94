#include "build_r6e_card.h"
#include "tables.hpp"

extern "C" const Op1fCard op1f_card_wave OP1F_NAMED("op1f_card_wave");
extern "C" const unsigned ppg_bank_index[];
extern "C" const short ppg_bank_pcm[];
extern "C" { const unsigned* volatile wave_edges = nullptr; }

namespace {
constexpr unsigned tables_ask = 1;
constexpr unsigned bank_count = 8;
constexpr unsigned phases = 128;
constexpr unsigned rows = 96;
constexpr unsigned columns_per_frame = 32;
constexpr unsigned columns = bank_count * phases;
constexpr unsigned wait_frames = 120;
static_assert(columns % columns_per_frame == 0, "Wave construction works in full slices");
static_assert(columns * rows * sizeof(unsigned) == 393216, "Wave memory ask covers all edge cells");

unsigned* building;
unsigned column;
unsigned waiting;

int sample(unsigned offset, unsigned layout, unsigned phase) {
    bool opposite = false;
    if (layout == 1 && phase > 64) {
        phase = 128 - phase;
        opposite = true;
    } else if (layout >= 2) {
        opposite = phase >= 64;
        phase &= 63;
        if (layout == 3 && phase > 32) phase = 64 - phase;
    }
    const int value = ppg_bank_pcm[offset + phase];
    return opposite ? -value : value;
}
}

void wave_tables_frame() {
    if (wave_tables_ready()) return;
    if (!building) {
        building = static_cast<unsigned*>(op1f_memory(&op1f_card_wave, tables_ask));
        if (!building) {
            if (waiting < wait_frames) ++waiting;
            return;
        }
    }
    const unsigned stop = column + columns_per_frame;
    for (; column < stop; ++column) {
        const unsigned phase = column & (phases - 1);
        const unsigned next = (phase + 1) & (phases - 1);
        const unsigned* index = ppg_bank_index + (column / phases) * rows;
        unsigned* edge = building + column * rows;
        for (unsigned row = 0; row < rows; ++row) {
            const unsigned layout = index[row] >> 16;
            const unsigned offset = index[row] & 65535u;
            const int left = sample(offset, layout, phase);
            const int delta = sample(offset, layout, next) - left;
            edge[row] = (static_cast<unsigned>(delta) << 16) | (static_cast<unsigned>(left) & 65535u);
        }
    }
    if (column == columns) {
        // The display thread is the only writer. Publish once both threads can
        // read the complete, immutable table for the rest of this boot.
        asm volatile("" ::: "memory");
        wave_edges = building;
    }
}

bool wave_tables_wait_expired() {
    return !building && waiting >= wait_frames;
}
