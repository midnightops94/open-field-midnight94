#pragma once
#include "sid_tables.hpp"

namespace sid {

using I32 = int;
using U32 = unsigned;
static_assert(sizeof(I32) == 4, "SID fixed-point word width");
constexpr I32 one = 32768;
constexpr U32 half_turn = 0x80000000u;
constexpr U32 maximum_step = 858993459u;

enum Choice { First, Second, Third };

inline I32 clip(I32 value, I32 low, I32 high) {
    return value < low ? low : value > high ? high : value;
}

inline I32 qmul(I32 left, I32 right) {
#if defined(__bfin__)
    I32 result;
    // Inputs and result follow the compiler's actual allocation. R0/R1
    // belong exclusively to the signed Blackfin+ product and its shifts.
    asm("(R1:0) = %1 * %2 (IS,NS);\n\t"
        "R0 >>= 15;\n\tR1 <<= 17;\n\t%0 = R0 | R1;"
        : "=d"(result) : "d"(left), "d"(right) : "R0", "R1", "ASTAT");
    return result;
#else
    return static_cast<I32>((static_cast<__INT64_TYPE__>(left) * right) >> 15);
#endif
}

inline I32 magnitude(I32 value) {
    return value < 0 ? -value : value;
}

inline U32 phase_ratio(U32 phase, U32 q12) {
    // Trial 2's split product is exact over the SID pitch and ratio ranges.
    const U32 high = (phase >> 16) * q12;
    if (high > (maximum_step >> 4)) return maximum_step;
    const U32 step = (high << 4) + (((phase & 65535u) * q12) >> 12);
    return step > maximum_step ? maximum_step : step;
}

inline U32 phase_scale(U32 phase, U32 q15) {
    return (phase >> 15) * q15 + (((phase & 32767u) * q15) >> 15);
}

inline unsigned high_bit(U32 value) {
    unsigned index = 0;
    if (value >> 16) { value >>= 16; index += 16; }
    if (value >> 8) { value >>= 8; index += 8; }
    if (value >> 4) { value >>= 4; index += 4; }
    if (value >> 2) { value >>= 2; index += 2; }
    if (value >> 1) index += 1;
    return index;
}

// The caller supplies a nonzero denominator and a numerator below it.
// A normalized reciprocal is linear-interpolated over [1, 2).
inline I32 fraction(U32 numerator, U32 denominator) {
    const unsigned bit = high_bit(denominator);
    const U32 hi = bit >= 15 ? denominator >> (bit - 15) : denominator << (15 - bit);
    const U32 lo = bit >= 15 ? numerator >> (bit - 15) : numerator << (15 - bit);
    const U32 offset = hi - one;
    const unsigned index = offset >> 7;
    const I32 base = sid_tables::phase_reciprocal[index];
    const I32 slope = sid_tables::phase_reciprocal[index + 1] - base;
    const I32 reciprocal = base + (slope * static_cast<I32>(offset & 127) >> 7);
    return static_cast<I32>(lo * static_cast<U32>(reciprocal) >> 15);
}

inline I32 triangle(U32 phase) {
    const U32 distance = phase < half_turn ? half_turn - phase : phase - half_turn;
    return one - static_cast<I32>(distance >> 15);
}

inline I32 blep(U32 position, U32 step) {
    if (position < step) {
        const I32 time = fraction(position, step);
        return time + time - qmul(time, time) - one;
    }
    const U32 remaining = 0u - position;
    if (position && remaining < step) {
        const I32 time = fraction(remaining, step);
        return qmul(time, time) - time - time + one;
    }
    return 0;
}

inline I32 reciprocal(I32 value) {
    const I32 offset = clip(value - one, 0, 480 * 1024 - 1);
    const auto index = static_cast<unsigned>(offset >> 10);
    const I32 base = sid_tables::reciprocal[index];
    const I32 slope = sid_tables::reciprocal[index + 1] - base;
    return base + (slope * (offset & 1023) >> 10);
}

inline I32 table(const unsigned short (&values)[257], I32 control) {
    const unsigned index = static_cast<unsigned>(control) >> 7;
    if (index == 256) return values[256];
    const I32 low = values[index];
    const I32 high = values[index + 1];
    return low + ((high - low) * (control & 127) >> 7);
}

template<unsigned columns>
inline I32 table(const unsigned short (&values)[257][columns], I32 control, unsigned column) {
    const unsigned index = static_cast<unsigned>(control) >> 7;
    if (index == 256) return values[256][column];
    const I32 low = values[index][column];
    const I32 high = values[index + 1][column];
    return low + ((high - low) * (control & 127) >> 7);
}

inline I32 control(I32 word) {
    if (word <= 0) return 0;
    if (word >= 0x7fff0000) return one;
    return word >> 16;
}

inline Choice choice(I32 word) {
    const I32 value = control(word);
    if (value < 8192) return First;
    return value < 24576 ? Second : Third;
}

constexpr unsigned use_scratch_modes = 0x100u;

template<class Read>
inline unsigned packed_modes(Read read) {
    unsigned modes = 0;
    for (unsigned i = 0; i < 4; ++i) modes |= static_cast<unsigned>(choice(read(i))) << (2 * i);
    return modes;
}

struct Voice {
    U32 phase[3];
    I32 smooth[4], weight[4];
    I32 pulse_route, wire_route, route_pulse, route_wire;
    I32 ic1, ic2, a, b, c, inverse_q;
    I32 drive_amount, drive_gain, combined_lp, mean, residue;
    U32 base_ratio, donor_ratio, duty;
    Choice link, response;
    unsigned tick;
    bool active, initialized;
};

inline void initialize(Voice& voice) {
    auto* bytes = reinterpret_cast<volatile unsigned char*>(&voice);
    for (unsigned i = 0; i < sizeof(Voice); ++i) bytes[i] = 0;
}

inline void start(Voice& voice) {
    initialize(voice);
    voice.phase[1] = 730144440u;   // .17 turn
    voice.phase[2] = 1331439862u;  // .31 turn
    voice.active = true;
}

inline void configure(Voice& voice, const I32* words, unsigned modes) {
    if (modes == use_scratch_modes) modes = packed_modes([&](unsigned i) { return words[4 + i]; });
    const Choice stack = static_cast<Choice>(modes & 3u);
    voice.link = static_cast<Choice>((modes >> 2) & 3u);
    voice.response = static_cast<Choice>((modes >> 4) & 3u);
    const Choice route = static_cast<Choice>((modes >> 6) & 3u);
    for (unsigned i = 0; i < 4; ++i) {
        const I32 target = control(words[i]) << 8;
        voice.smooth[i] = voice.initialized
            ? voice.smooth[i] + qmul(target - voice.smooth[i], sid_tables::smoothing16)
            : target;
    }
    const I32 pulse_target = route == Second ? one << 8 : 0;
    const I32 wire_target = route == Third ? one << 8 : 0;
    voice.pulse_route = voice.initialized
        ? voice.pulse_route + qmul(pulse_target - voice.pulse_route, sid_tables::smoothing16)
        : pulse_target;
    voice.wire_route = voice.initialized
        ? voice.wire_route + qmul(wire_target - voice.wire_route, sid_tables::smoothing16)
        : wire_target;
    const I32 wave = voice.smooth[0] >> 8;
    const I32 duty = voice.smooth[1] >> 8;
    const I32 cutoff = voice.smooth[2] >> 8;
    const unsigned linked_column = voice.link == First ? 0 : 4;
    for (unsigned i = 0; i < 4; ++i) {
        voice.weight[i] = table(sid_tables::wave, wave, linked_column + i);
    }
    voice.base_ratio = stack == Second ? 2048u : stack == First ? 4089u : 4096u;
    voice.donor_ratio = stack == Third ? 6137u : stack == First ? 4103u : 4096u;
    if (voice.link != First) {
        const auto donor = static_cast<U32>(table(sid_tables::wave, wave, 8));
        voice.donor_ratio = voice.donor_ratio * donor >> 12;
    }
    voice.duty = static_cast<U32>(table(sid_tables::duty_mean, duty, 0)) << 17;
    voice.mean = table(sid_tables::duty_mean, duty, 1);
    voice.a = table(sid_tables::filter, cutoff, 0);
    voice.b = table(sid_tables::filter, cutoff, 1);
    voice.c = table(sid_tables::filter, cutoff, 2);
    voice.inverse_q = table(sid_tables::filter, cutoff, 3);
    const I32 drive = table(sid_tables::drive, voice.smooth[3] >> 8);
    voice.drive_amount = 4 * drive + qmul(drive, 22938);
    voice.drive_gain = one + qmul(13107, voice.drive_amount);
    voice.route_pulse = qmul(qmul(drive, voice.pulse_route >> 8), 20972);
    voice.route_wire = qmul(qmul(drive, voice.wire_route >> 8), 23593);
    voice.initialized = true;
}

inline I32 sample_steps(Voice& voice, U32 pitch, U32 base_step, U32 donor_step) {
    const U32 donor_before = voice.phase[2];
    voice.phase[0] += base_step;
    voice.phase[1] += pitch;
    voice.phase[2] += donor_step;
    if (voice.link == Second && donor_before < half_turn && half_turn - donor_before <= donor_step) {
        const U32 until = half_turn - donor_before;
        const U32 after = until == donor_step ? 0
            : phase_scale(base_step, one - static_cast<U32>(fraction(until, donor_step)));
        const U32 before = voice.phase[0] - after;
        voice.residue += triangle(before) - triangle(0);
        voice.phase[0] = after;
    }
    I32 tri = triangle(voice.phase[0]);
    I32 ring_sign = 1;
    if (voice.link == Third) {
        const I32 old_sign = donor_before < half_turn ? 1 : -1;
        ring_sign = voice.phase[2] < half_turn ? 1 : -1;
        if (old_sign != ring_sign) voice.residue += tri * (old_sign - ring_sign);
        tri *= ring_sign;
    }
    const I32 raw_tri = tri;
    tri += voice.residue;
    voice.residue = qmul(voice.residue, 13763);
    I32 pulse_part = 0;
    if (voice.weight[1]) {
        const U32 pulse_phase = voice.phase[1];
        const I32 pulse = (pulse_phase < voice.duty ? one : -one) +
                          blep(pulse_phase, pitch) - blep(pulse_phase - voice.duty, pitch);
        pulse_part = qmul(voice.weight[1], pulse);
    }
    I32 saw_part = 0;
    if (voice.weight[2]) {
        const I32 saw = static_cast<I32>(voice.phase[2] >> 16) - one - blep(voice.phase[2], donor_step);
        saw_part = qmul(voice.weight[2], saw);
    }
    const U32 phase = voice.phase[0];
    U32 combined_bits = 0;
    if (phase < voice.duty) {
        const U32 tri_bits = static_cast<U32>(raw_tri + one) * 4095u >> 16;
        // floor(phase * (4096 - 1) / 2^32), without a wide product.
        const U32 saw_bits = (phase >> 20) - (((phase & 0xfffffu) << 12) < phase);
        combined_bits = tri_bits & saw_bits;
    }
    const I32 combined = 4 * (static_cast<I32>((combined_bits << 3) + (combined_bits >> 9)) - voice.mean);
    voice.combined_lp += qmul(combined - voice.combined_lp, 14746);
    const I32 combined_part = qmul(voice.weight[3], voice.combined_lp);
    const I32 bypass = qmul(voice.route_pulse, pulse_part) +
                       qmul(voice.route_wire, saw_part + combined_part);
    I32 input = qmul(voice.weight[0], tri) + pulse_part + saw_part + combined_part - bypass;
    input = qmul(input, reciprocal(one + qmul(5898, magnitude(input))));
    if (voice.drive_amount) {
        input = qmul(qmul(input, voice.drive_gain),
                     reciprocal(one + qmul(voice.drive_amount, magnitude(input))));
    }
    const I32 v3 = input - voice.ic2;
    const I32 band = qmul(voice.a, voice.ic1) + qmul(voice.b, v3);
    const I32 low = voice.ic2 + qmul(voice.b, voice.ic1) + qmul(voice.c, v3);
    voice.ic1 = clip(2 * band - voice.ic1, -4 * one, 4 * one);
    voice.ic2 = clip(2 * low - voice.ic2, -4 * one, 4 * one);
    const I32 filtered = voice.response == Second ? band : voice.response == Third
        ? input - qmul(voice.inverse_q, band) - low : low;
    const I32 output = qmul(filtered + bypass, 2458);
    return clip(output, -one, one - 1) * 65536;
}

inline I32 sample(Voice& voice, U32 pitch) {
    pitch = pitch > maximum_step ? maximum_step : pitch;
    return sample_steps(voice, pitch, phase_ratio(pitch, voice.base_ratio), phase_ratio(pitch, voice.donor_ratio));
}

inline void render_with_modes(Voice& voice, I32* out, const U32* pitch,
                              const I32* words, unsigned frames, unsigned modes) {
    if (!voice.active) {
        for (unsigned i = 0; i < frames * 2; ++i) out[i] = 0;
        return;
    }
    for (unsigned i = 0; i < frames; ++i) {
        if ((voice.tick++ & 15) == 0) configure(voice, words, modes);
        const I32 value = sample(voice, pitch[i]);
        out[2 * i] = value;
        out[2 * i + 1] = value;
    }
}

inline void render(Voice& voice, I32* out, const U32* pitch,
                   const I32* words, unsigned frames) {
    render_with_modes(voice, out, pitch, words, frames, use_scratch_modes);
}

}
