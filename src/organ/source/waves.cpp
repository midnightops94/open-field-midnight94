#include "build_r6e_card.h"
#include "waves.hpp"

extern "C" const Op1fCard op1f_card_organ OP1F_NAMED("op1f_card_organ");
extern "C" const signed char organ_encoded_waves[];

const int16_t* volatile organ_waves::installed;

namespace {
constexpr unsigned waves_ask = 2;
constexpr unsigned rows_per_frame = 48;
static_assert(organ_waves::table_cells * sizeof(int16_t) == 209922, "Match the shared memory ask");

int16_t* building;
unsigned next_row;
}

extern "C" void organ_prepare_waves() {
    if (organ_waves::ready()) return;
    if (!building) {
        building = static_cast<int16_t*>(op1f_memory(&op1f_card_organ, waves_ask));
        if (!building) return;
    }

    const unsigned end = next_row + rows_per_frame;
    const unsigned limit = end < organ_waves::row_count ? end : organ_waves::row_count;
    for (; next_row < limit; ++next_row) {
        const signed char* delta = organ_encoded_waves + next_row * organ_waves::encoded_row_bytes;
        auto* row = building + next_row * organ_waves::row_stride;
        int slope = static_cast<unsigned char>(delta[0]) + 256 * int(delta[1]);
        int cell = slope;
        row[0] = 0;
        row[1] = cell;
        for (unsigned index = 2; index < organ_waves::row_stride; ++index) {
            slope += delta[index];
            cell += slope;
            row[index] = cell;
        }
    }
    if (next_row == organ_waves::row_count) {
        // Every row's terminal zero is the next row's first cell, except the last.
        building[organ_waves::table_cells - 1] = 0;
        // The display thread publishes one immutable table for the whole boot.
        asm volatile("" ::: "memory");
        organ_waves::installed = building;
    }
}
