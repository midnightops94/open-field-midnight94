#pragma once

namespace plonk_spring {

using I = int;
using U = unsigned;
constexpr I one = 32768;
constexpr U rate = 88200, frames = 128, attack_frames = 6174, fade_frames = 1588;
enum Mode : unsigned { Off, Fixed, Linked };

constexpr Mode mode(unsigned saved) {
    return saved == (16384u << 16) ? Fixed : saved == (32767u << 16) ? Linked : Off;
}

constexpr unsigned word(Mode value) {
    return value == Fixed ? 16384u << 16 : value == Linked ? 32767u << 16 : 0;
}

inline I clip(I value) { return value < -32768 ? -32768 : value > 32767 ? 32767 : value; }
inline I multiply(I value, I coefficient) {
    const I product = value * coefficient;
    return (product + 16384 - (product < 0)) >> 15;
}

inline I pole_step(I delta, I coefficient) {
    const I upper = (delta >> 8) * coefficient;
    const I remainder = ((upper & 127) << 8) + (delta & 255) * coefficient;
    return (upper >> 7) + ((remainder + 16384) >> 15);
}

inline I q15(I fine) { return (fine + 128) >> 8; }

template<U length>
struct Delay {
    short data[length];
    U index, written;

    I arrival() const { return written == length ? data[index] : 0; }
    void advance(I value) {
        data[index] = static_cast<short>(clip(value));
        if (written < length) ++written;
        if (++index == length) index = 0;
    }
    void clear() { index = written = 0; }
};

template<U length, I coefficient>
struct Allpass {
    Delay<length> delay;
    I run(I in) {
        const I out = clip(delay.arrival() - multiply(in, coefficient));
        delay.advance(in + multiply(out, coefficient));
        return out;
    }
};

struct Tank {
    Delay<2620> first;
    Delay<3696> second;
    Allpass<19,22282> a;
    Allpass<30,21365> b;
    Allpass<51,20447> c;
    Allpass<72,19530> d;
    Allpass<95,18612> e;
    Allpass<15,22282> f;
    Allpass<34,21365> g;
    Allpass<56,20447> h;
    Allpass<81,19530> j;
    I input_low, first_low, second_low, pickup_low; // Q23 avoids DC from coarse pole state.

    void clear() {
        first.clear(); second.clear();
        a.delay.clear(); b.delay.clear(); c.delay.clear(); d.delay.clear(); e.delay.clear();
        f.delay.clear(); g.delay.clear(); h.delay.clear(); j.delay.clear();
        input_low = first_low = second_low = pickup_low = 0;
    }

    I run(I input) {
        input_low += pole_step(input * 256 - input_low, 498);
        const I drive = clip(input - q15(input_low));
        const I first_sample = first.arrival();
        const I second_sample = second.arrival();
        first_low += pole_step(first_sample * 256 - first_low, 9407);
        second_low += pole_step(second_sample * 256 - second_low, 7376);
        first.advance(e.run(d.run(c.run(b.run(a.run(clip(drive + multiply(q15(first_low),26281))))))));
        second.advance(j.run(h.run(g.run(f.run(clip(drive + multiply(q15(second_low),24004)))))));
        const I pickup = multiply(first_sample, 20316) + multiply(second_sample, 12452);
        pickup_low += pole_step(pickup * 256 - pickup_low, 11091);
        return q15(pickup_low);
    }
};

inline I send_target(Mode selected, I strike) {
    if (selected != Linked) return one;
    const I offset = strike - 13107;
    return one + multiply(offset, offset < 0 ? 58982 : 49152);
}

inline I excitation(I sample, I level, U frame_age) {
    if (frame_age >= attack_frames) return 0;
    const U remaining = attack_frames - frame_age - 1u;
    const I time = static_cast<I>((remaining * 21739u + 2048u) >> 12);
    return multiply(multiply(sample, level), multiply(time, time));
}

inline U root(U square) {
    U bit = 1u << 30, value = 0;
    while (bit > square) bit >>= 2;
    while (bit) {
        if (square >= value + bit) { square -= value + bit; value = (value >> 1) + bit; }
        else value >>= 1;
        bit >>= 2;
    }
    return value;
}

inline U soft_fraction(U numerator, U denominator) {
    U divisor_scale = 1, quotient = 0;
    while (denominator <= (numerator >> 1)) { denominator <<= 1; divisor_scale <<= 1; }
    while (divisor_scale) {
        if (numerator >= denominator) { numerator -= denominator; quotient |= divisor_scale; }
        denominator >>= 1;
        divisor_scale >>= 1;
    }
    return quotient;
}

inline I add_wet(I dry, I wet) {
    constexpr I knee = 26214, width = 5898;
    const I pre_mix = dry >> 13;
    const I sum = pre_mix + wet;
    const I magnitude = sum < 0 ? -sum : sum;
    if ((wet > 0 && sum > knee) || (wet < 0 && sum < -knee)) {
        const U excess = static_cast<U>(magnitude - knee);
        const I limited = knee + static_cast<I>(soft_fraction(width * excess, width + excess));
        const I wanted = sum < 0 ? -limited : limited;
        if ((wet > 0 && wanted > pre_mix) || (wet < 0 && wanted < pre_mix)) wet = wanted - pre_mix;
        else wet = 0;
    }
    const I addition = wet * 8192;
    if (addition > 0 && dry > 2147483647 - addition) return 2147483647;
    if (addition < 0 && dry < (-2147483647 - 1) - addition) return -2147483647 - 1;
    return dry + addition;
}

struct Spring {
    Tank tank;
    U fade;
    I send; // Q23 maintains smoothness with the 24 Hz update coefficient.

    bool active(Mode value) const { return value != Off || fade != 0; }
    void clear() { tank.clear(); fade = 0; send = one * 256; }

    U mix(Mode value, I strike, const I* excitation, I* stereo) {
        if (!active(value)) return 0;
        const I target = send_target(value, strike) * 256;
        const bool enabled = value != Off;
        if (enabled && fade == 0) send = target;
        U squares = 0;
        for (U i = 0; i < frames; ++i) {
            if (enabled) { if (fade < fade_frames) ++fade; }
            else if (--fade == 0) { clear(); break; }
            send += multiply(target - send, 56);
            const I pickup = tank.run(enabled ? clip(multiply(clip(excitation[i]), send >> 8)) : 0);
            const I ramp = static_cast<I>((fade * 1352320u + 32768u) >> 16);
            const I wet = multiply(pickup + (pickup >> 2), ramp);
            const I left = stereo[2*i], right = stereo[2*i+1];
            const I mixed_left = add_wet(left, wet), mixed_right = add_wet(right, wet);
            stereo[2*i] = mixed_left; stereo[2*i+1] = mixed_right;
            const I level = ((mixed_left >> 13) - (left >> 13) + (mixed_right >> 13) - (right >> 13)) >> 5;
            squares += static_cast<U>(level * level);
        }
        return root(squares >> 7) << 4;
    }
};

static_assert(sizeof(Spring) < 14000, "single bounded Plonk Spring tank");

}
