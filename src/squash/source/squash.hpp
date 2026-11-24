#pragma once
#include "squash-tables.hpp"

namespace squash {
constexpr int one = 1048576;
constexpr int db_one = 65536;
constexpr int db_per_octave = 394566;
constexpr unsigned default_modes = 10; // Body, Rubber, Glue, Cool, encoded little-endian.

inline int limit(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
inline int magnitude(int x) { return x < 0 ? -x : x; }
template<unsigned bits> inline int product(int a, int b) {
#if defined(__bfin__)
    int result;
    asm volatile("R0 = %1; R1 = %2; (R1:0) = R0 * R1 (IS,NS);\n\t"
                 "R0 >>= %3; R1 <<= %4; %0 = R0 | R1;"
                 : "=&d"(result) : "d"(a), "d"(b), "i"(bits), "i"(32-bits)
                 : "R0", "R1", "ASTAT");
    return result;
#else
    return int((static_cast<long long>(a) * b) >> bits);
#endif
}
inline int mul(int a, int b) { return product<20>(a, b); }
inline int mix(int a, int b, int weight) { return a + mul(b - a, weight); }
inline int smooth_to(int a, int b, int weight) {
    const int distance = b - a;
    const int movement = mul(magnitude(distance), weight);
    return a + (distance < 0 ? -(movement ? movement : 1) : distance && !movement ? 1 : movement);
}

inline unsigned exponent(unsigned x) {
    unsigned n = 0;
    if (x >= 65536) { x >>= 16; n += 16; }
    if (x >= 256) { x >>= 8; n += 8; }
    if (x >= 16) { x >>= 4; n += 4; }
    if (x >= 4) { x >>= 2; n += 2; }
    return n + (x >= 2);
}

inline int norm_curve(const int* table, unsigned x, unsigned power) {
    const unsigned normalized = x << (31u - power);
    const unsigned index = (normalized >> 23) & 255;
    return table[index] + ((table[index + 1] - table[index]) * int((normalized >> 15) & 255) >> 8);
}

inline int level_db(int level) {
    const unsigned x = level > 1 ? unsigned(level) : 1;
    const unsigned n = exponent(x);
    return (int(n) - 20) * db_per_octave + norm_curve(log_db, x, n);
}

inline int root(int power) {
    if (power <= 0) return 0;
    const unsigned x = unsigned(power), n = exponent(x);
    const int e = int(n) - 20;
    int mantissa = norm_curve(root_norm, x, n);
    if (e & 1) mantissa = mul(mantissa, 1482910);
    return e < 0 ? mantissa >> (-(e >> 1)) : mantissa << (e >> 1);
}

inline int gain(int db) {
    const unsigned position = unsigned(limit(db, -96 * db_one, 12 * db_one - 1) + 96 * db_one);
    const unsigned index = position >> 13;
    return make_gain[index] + ((make_gain[index + 1] - make_gain[index]) * int((position >> 3) & 1023) >> 10);
}

inline int tanh_curve(int x) {
    const unsigned position = unsigned(magnitude(x));
    if (position >= 8u * one) return x < 0 ? -one : one;
    const unsigned i = position >> 15;
    const int v = curve_tanh[i] + ((curve_tanh[i + 1] - curve_tanh[i]) * int((position >> 5) & 1023) >> 10);
    return x < 0 ? -v : v;
}

template<unsigned bits = 7> inline int get_curve(const int* values, unsigned word) {
    const unsigned index = word >> bits;
    return values[index] + ((values[index + 1] - values[index]) * int(word & ((1u << bits) - 1)) >> bits);
}

inline int knee_gain(int db, int knee) {
    const int half = knee >> 1;
    if (db >= half) return db;
    if (db <= -half || knee < 4096) return 0;
    const unsigned index = unsigned(knee) >> 12;
    const int near = inverse_knee[index];
    const int far = inverse_knee[index + 1];
    const int inverse = near + product<12>(far - near, knee & 4095);
    const int distance = db + half;
    return mul(distance, product<16>(distance, inverse));
}

inline int protect(int x) {
    constexpr int ceiling_start = 880804;
    if (magnitude(x) <= ceiling_start) return x;
    const int v = ceiling_start + mul(157286, tanh_curve(mul(magnitude(x) - ceiling_start, 6990507)));
    return x < 0 ? -v : v;
}

inline int hot(int x) {
    const int driven = limit(x + (x >> 1), -one, one);
    return driven - mul(mul(mul(driven, driven), driven), 349525);
}

struct Settings {
    int threshold, strength, blend, knee, rms, low_cut, character, heat, attack, release, makeup;
};

inline Settings settings(const unsigned (&words)[4], unsigned modes) {
    const unsigned detector = modes % 3;
    const unsigned knee = (modes / 3) % 3;
    const unsigned profile = (modes / 9) % 3;
    const int* const attacks[] = {attack_0, attack_1, attack_2};
    const int* const hard[] = {hard_0, hard_1, hard_2};
    const int* const releases[] = {release_0, release_1, release_2};
    const int threshold = -2 * db_one - int(words[0] * 76u);
    const int strength = get_curve(strength_curve, words[1]);
    const int margin = -threshold - (detector == 1 ? 19 : 14) * db_one;
    const int makeup = limit(mul(mul(margin > 0 ? margin : 0, strength), 503316), 0, 10 * db_one);
    return {threshold, strength, int(words[3] == 32767 ? one : words[3] << 5),
            (knee == 0 ? 12 : knee == 1 ? 0 : 3) * db_one,
            detector == 1 ? one : 0, detector == 2 ? one : 0,
            knee == 2 ? int(words[1] == 32767 ? one : words[1] << 5) : 0,
            modes >= 27 ? one : 0, get_curve<8>(knee == 1 ? hard[profile] : attacks[profile], words[2]),
            get_curve<8>(releases[profile], words[2]), makeup};
}

struct State {
    Settings live, target;
    int rms_power, bass_left, bass_right, reduction;
    unsigned words[4], modes, valid;

    void set(const unsigned (&w)[4], unsigned next_modes) {
        if (valid && words[0] == w[0] && words[1] == w[1] && words[2] == w[2] && words[3] == w[3]
            && next_modes == modes) return;
        for (unsigned i = 0; i < 4; ++i) words[i] = w[i];
        modes = next_modes;
        target = settings(w, modes);
        if (!valid) {
            live = target;
            rms_power = bass_left = bass_right = reduction = 0;
            valid = 1;
        }
    }

    void update() {
        live.threshold = smooth_to(live.threshold, target.threshold, smooth);
        live.strength = smooth_to(live.strength, target.strength, smooth);
        live.blend = smooth_to(live.blend, target.blend, smooth);
        live.knee = smooth_to(live.knee, target.knee, smooth);
        live.rms = smooth_to(live.rms, target.rms, smooth);
        live.low_cut = smooth_to(live.low_cut, target.low_cut, smooth);
        live.character = smooth_to(live.character, target.character, smooth);
        live.heat = smooth_to(live.heat, target.heat, smooth);
        live.attack = smooth_to(live.attack, target.attack, smooth);
        live.release = smooth_to(live.release, target.release, smooth);
        live.makeup = smooth_to(live.makeup, target.makeup, smooth);
    }

    int wet(int x, int linear_gain, int drive, int inverse, int weight) const {
        int y = mul(x, linear_gain);
        if (weight) y = mix(y, mul(tanh_curve(mul(y, drive)), inverse), weight);
        y = protect(y);
        if (live.heat) y = mix(y, hot(y), live.heat);
        return y;
    }

    void step(int& l, int& r) {
        update();
        const int left = l >> 11, right = r >> 11;
        bass_left = mix(bass_left, left, bass_speed);
        bass_right = mix(bass_right, right, bass_speed);
        const int a = magnitude(left - mul(bass_left, live.low_cut));
        const int b = magnitude(right - mul(bass_right, live.low_cut));
        const int peak = a > b ? a : b;
        rms_power = smooth_to(rms_power, mul(peak, peak), rms_speed);
        const int level = live.rms ? mix(peak, root(rms_power), live.rms) : peak;
        const int desire = mul(live.strength, knee_gain(level_db(level) - live.threshold, live.knee));
        reduction = smooth_to(reduction, desire, desire > reduction ? live.attack : live.release);
        if (!live.blend || (!live.strength && !live.heat)) return;
        const int linear_gain = gain(live.makeup - reduction);
        const unsigned position = unsigned(live.character == one ? 32767 : live.character >> 5);
        const int inverse = live.character ? get_curve(inverse_pillow, position) : one;
        const int drive = one + mul(live.character, 6815744);
        const int weight = mul(live.character, 901775);
        const int wl = wet(left, linear_gain, drive, inverse, weight);
        const int wr = wet(right, linear_gain, drive, inverse, weight);
        // Keep the full Q31 dry signal while the wet curve runs in Q20.
        l = mix(l, wl * 2048, live.blend);
        r = mix(r, wr * 2048, live.blend);
    }
};
}
