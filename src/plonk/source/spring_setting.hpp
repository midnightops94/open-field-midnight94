#pragma once
namespace plonk {
enum SpringSetting { spring_off, spring_short, spring_long };
// Keep the v4 detents: 0 maps to Off, 10922 to Short, and 21845 and 32767 to Long.
inline SpringSetting spring_setting(unsigned word) {
    return word < 5462 ? spring_off : word < 16384 ? spring_short : spring_long;
}
constexpr unsigned spring_words[] = {0, 10922, 32767};
}
