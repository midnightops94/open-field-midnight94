#pragma once
#include <stdint.h>

namespace organ_waves {
constexpr unsigned row_count = 205;
constexpr unsigned row_stride = 512;
constexpr unsigned encoded_row_bytes = row_stride + 1;
constexpr unsigned table_cells = row_count * row_stride + 1;

extern const int16_t* volatile installed;

inline const int16_t* ready() {
    const int16_t* rows = installed;
    asm volatile("" ::: "memory");
    return rows;
}
}

extern "C" void organ_prepare_waves();
