#pragma once

extern "C" const unsigned* volatile wave_edges;

inline const unsigned* wave_tables_ready() {
    const auto* table = wave_edges;
    asm volatile("" ::: "memory");
    return table;
}

void wave_tables_frame();
bool wave_tables_wait_expired();
