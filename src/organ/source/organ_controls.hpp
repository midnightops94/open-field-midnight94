#pragma once

constexpr int organ_type_words[] = {0, 1, 2, 5};
constexpr int organ_dual_words[] = {0, 10922, 21845, 32767};
constexpr int organ_spin_words[] = {0, 8192, 32767};

inline int organ_next_choice(unsigned lane, int value, unsigned count) {
    const int* words = lane == 0 ? organ_type_words : lane == 3 ? organ_spin_words : organ_dual_words;
    const unsigned size = lane == 3 ? 3 : 4;
    unsigned index = 0;
    while (index < size && words[index] <= value) ++index;
    unsigned next = index + count - 1;
    if (lane != 3) return words[next & 3u];
    // count is capped at 1,000 by the button collector. 1366/4096 gives
    // exact integer division by three over this range.
    next -= 3u * ((next * 1366u) >> 12);
    return words[next];
}

inline unsigned organ_model(int value) {
    switch (value) {
    case 1: return 1;
    case 2: return 2;
    case 5: return 3;
    default: return 0;
    }
}
