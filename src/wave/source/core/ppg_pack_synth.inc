#include "ppg_native_synth.hpp"
#include "ppg_word_math.hpp"
#include "field-trial/ppg_stock_voice.hpp"

extern "C" ppg_reader::i32 ppg_v2_step_fast(ppg_reader::Voice*, const ppg_reader::BankFrame*,
                                            const ppg_reader::OscillatorFrame*, const ppg_reader::PulseDuty*);

namespace ppg_synth {
#include "native-check/ppg_synth_tables.inc"

constexpr i32 i32_max = __INT32_MAX__;
constexpr i32 i32_min = -i32_max - 1;
constexpr u32 q16 = ppg_reader::one_q16;
constexpr u32 fade_length = 441;
constexpr u32 cycle_half = 176400;

static inline i32 add(i32 a, i32 b) {
#if defined(__bfin__)
    i32 result;
    asm("%0 = %1 + %2 (S);" : "=d"(result) : "d"(a), "d"(b) : "ASTAT");
    return result;
#else
    const i32 result = static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b));
    if (((a ^ result) & (b ^ result)) < 0) return a < 0 ? i32_min : i32_max;
    return result;
#endif
}

static inline i32 subtract(i32 a, i32 b) {
#if defined(__bfin__)
    i32 result;
    asm("%0 = %1 - %2 (S);" : "=d"(result) : "d"(a), "d"(b) : "ASTAT");
    return result;
#else
    const i32 result = static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b));
    if (((a ^ b) & (a ^ result)) < 0) return a < 0 ? i32_min : i32_max;
    return result;
#endif
}

static u32 magnitude(i32 x) {
    const auto bits = static_cast<u32>(x);
    return x < 0 ? 0u - bits : bits;
}

// BP02's Blackfin+ uses the same full signed register-pair multiply in its
// timing arithmetic. The host forms the same exact pair portably.
template<bool saturate>
static inline i32 rounded_product(i32 a, i32 b) {
#if defined(__bfin__)
    u32 low;
    i32 high;
    // Bind both halves of the product to the actual hardware destinations.
    asm("(R1:0) = %2 * %3 (IS,NS);" : "=q0"(low), "=q1"(high) : "d"(a), "d"(b) : "ASTAT");
#else
    const auto product = static_cast<__INT64_TYPE__>(a) * b;
    const u32 low = static_cast<u32>(product);
    const i32 high = static_cast<i32>(product >> 32);
#endif
    const u32 biased = low + (1u << 29) - (high < 0);
    const i32 upper = high + (biased < low);
    if (saturate && upper > i32_max / 4) return i32_max;
    if (saturate && upper < i32_min / 4) return i32_min;
    return static_cast<i32>((static_cast<u32>(upper) << 2) | (biased >> 30));
}

static inline i32 mul(i32 a, i32 b) { return rounded_product<true>(a, b); }

// Normalized controls, their gains and interpolation endpoints keep every
// product used here in the signed result's range.
static inline i32 mul_bounded(i32 a, i32 b) { return rounded_product<false>(a, b); }
static inline i32 smooth_bounded(i32 old, i32 target, i32 rate) {
    return old + mul_bounded(target - old, rate);
}

#ifndef PPG_PACK_ENTRY_ONLY
static u32 divide(u32 numerator, u32 denominator) {
    // Note-event preparation only. All callers use nonzero denominators
    // small enough for a left shift of the partial remainder.
    u32 remainder = 0;
    u32 quotient = 0;
    for (u32 bit = 32; bit > 0; --bit) {
        remainder = (remainder << 1) | ((numerator >> (bit - 1)) & 1);
        quotient <<= 1;
        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= 1;
        }
    }
    return quotient;
}

#endif
static i32 clamp(i32 v, i32 lo, i32 hi) { return v < lo ? lo : v > hi ? hi : v; }

#ifndef PPG_PACK_ENTRY_ONLY
static i32 smooth(i32 old, i32 target, i32 rate) {
    return add(old, mul(subtract(target, old), rate));
}

static i32 soft_clip(i32 sample) {
    // The input has 28 fraction bits. Each table step spans 1/256.
    const u32 absolute = magnitude(sample);
    const u32 index = absolute >> 20;
    const i32 value = index >= 2048 ? soft_round[2048] :
        ppg_reader::interpolate(soft_round[index], soft_round[index + 1], (absolute >> 4) & 65535u);
    return sample < 0 ? -value : value;
}

#endif
static i32 reflect(i32 position) {
    // Position arrives within [0,45] Q16 after adding the largest sweep.
    if (position >= 30 * static_cast<i32>(q16)) position -= 30 * q16;
    if (position > 15 * static_cast<i32>(q16)) position = 30 * q16 - position;
    return position;
}

template<class Derived>
static void derive_twin(i32 twin, Derived& frame) {
    const i32 detune = 1789570 + mul_bounded(mul_bounded(twin, twin), 14316558); // (2 + 16 Twin²) / 1200
    const i32 exponent = mul_bounded(detune, natural_log_two);
    frame.detune_ratio = one + mul_bounded(exponent, one + mul_bounded(exponent, one / 2 + mul_bounded(exponent, one / 6)));
    frame.detune_mip_q16 = (detune + 8192) >> 14;
    frame.twin_mix_q16 = (mul_bounded(twin, twin_scale) + 8192) >> 14;
    frame.spread_q16 = mul_bounded(twin, 4 * q16);
}

template<class Derived>
static void derive_macros(const Controls& controls, const Preset& patch, Derived& frame) {
    frame.base_q16 = mul_bounded(controls.q30[0], 15 * q16);
    frame.amount_q16 = mul_bounded(mul_bounded(controls.q30[1], controls.q30[1]), 30 * q16);
    derive_twin(controls.q30[3], frame);
    frame.motion = patch.motion;
}

#ifndef PPG_PACK_ENTRY_ONLY
template <typename State>
static void prepare(State& s, Frame& frame) {
    for (u32 index = 0; index < 4; ++index) {
        s.actual.q30[index] = smooth(s.actual.q30[index], s.target.q30[index], parameter_smooth);
    }
    s.master = smooth(s.master, s.master_target, parameter_smooth);
    ppg_v2_bank_prepare_88200(&s.bank, &frame.bank, 1);

    const auto& patch = patches[s.preset];
    frame.duty = ppg_reader::make_duty(static_cast<u32>(s.actual.q30[2]));
    derive_macros(s.actual, patch, frame);
    frame.master = s.master;
}

void prepare_frame(Synth& s, Frame& frame) { prepare(s, frame); }

static void advance_motion(Voice& v) {
    v.falling = smooth(v.falling, 0, fall_decay);
    v.rising = smooth(v.rising, one, rise_decay);
    u32 step = one / cycle_half;
    v.cycle_carry += one % cycle_half;
    if (v.cycle_carry >= cycle_half) {
        ++step;
        v.cycle_carry -= cycle_half;
    }
    const auto delta = static_cast<i32>(step);
    v.cycle += v.cycle_position < cycle_half ? delta : -delta;
    if (++v.cycle_position == 2 * cycle_half) v.cycle_position = 0;
}

static bool envelope_step(Voice& v) {
    if (v.releasing) {
        v.envelope -= v.release_step;
        if (--v.release_left == 0 || v.envelope <= 11) {
            v.active = false;
            v.last = 0;
            return false;
        }
    } else if (v.in_decay) {
        v.envelope = smooth(v.envelope, v.sustain, v.decay_rate);
    } else {
        v.envelope += v.attack_step;
        if (v.envelope >= one) {
            v.envelope = one;
            v.in_decay = true;
        }
    }
    return true;
}

static i32 spectral_sample(Voice& v, const Frame& f) {
    const i32 travel = f.motion == 0 ? v.falling : f.motion == 1 ? v.rising : v.cycle;
    const i32 destination = reflect(f.base_q16 + mul(f.amount_q16, travel));
    if (--v.hold_left == 0) {
        const i32 quarter = (destination + 8192) >> 14;
        v.held_position_q16 = quarter << 14;
        v.hold_left = 882;
    }
    const i32 wanted = smooth(destination, v.held_position_q16, scrub_mix);
    v.position_q16 = smooth(v.position_q16, wanted, position_smooth);
    const ppg_reader::OscillatorFrame oscillator{
        v.pitch.step, static_cast<u32>(mul(static_cast<i32>(v.pitch.step), f.detune_ratio)),
        static_cast<u32>(clamp(v.pitch.mip_q16 + static_cast<i32>(f.duty.mip), 0, 5 * q16)),
        static_cast<u32>(clamp(v.pitch.mip_q16 + f.detune_mip_q16 + static_cast<i32>(f.duty.mip), 0, 5 * q16)),
        static_cast<u32>(v.position_q16), static_cast<u32>(reflect(v.position_q16 + f.spread_q16)),
        v.pitch.grain_q16, f.twin_mix_q16};
    const auto* table = wave_tables_ready();
    if (!table) return 0;
    const i32 sample = edge_reader::step(table, &v.oscillator, &f.bank, &oscillator, &f.duty) >> 2;
    advance_motion(v);
    return soft_clip(sample);
}

i32 voice_sample(Voice& v, const Frame& f) {
    if (!v.active || !envelope_step(v)) return 0;
    i32 value = mul(mul(mul(spectral_sample(v, f), v.envelope), v.velocity), voice_scale);
    if (v.steal_mix_q16 > 0) {
        value = ppg_reader::interpolate(value, v.steal_tail, v.steal_mix_q16);
        u32 drop = q16 / fade_length;
        v.steal_carry += q16 % fade_length;
        if (v.steal_carry >= fade_length) {
            ++drop;
            v.steal_carry -= fade_length;
        }
        v.steal_mix_q16 -= drop;
    }
    v.last = value;
    return value;
}

static void begin_release(Voice& voice, u32 frames) {
    voice.releasing = true;
    voice.release_left = frames;
    voice.release_step = static_cast<i32>(divide(voice.envelope + frames / 2, frames));
}

#endif
static void clear_bytes(void* state, u32 size) {
    auto* next = static_cast<volatile unsigned char*>(state);
    for (u32 index = 0; index < size; ++index) next[index] = 0;
}

static u32 read_choice(int value, u32 maximum) {
    if (value < 0) return 0;
    const u32 index = static_cast<u32>(value) >> 28;
    return index > maximum ? maximum : index;
}

static void stock_controls(StockVoice& v, const int* words, bool immediate) {
    v.controls_pending = 0;
    for (u32 index = 0; index < 4; ++index) {
        const i32 target = clamp(words[index] >> 1, 0, one);
        v.target.q30[index] = target;
        if (immediate) v.actual.q30[index] = target;
        else if (target != v.actual.q30[index]) v.controls_pending |= 1u << index;
    }
    const u32 desired_bank = read_choice(words[4], 7);
    ppg_v2_bank_select(&v.bank, desired_bank, immediate);
    const auto preset = read_choice(words[5], 4);
    if (preset != v.preset) v.control_tick = 0;
    v.preset = preset;
}

static Pitch field_pitch(u32 step) {
    if (!step || step > static_cast<u32>(i32_max)) return {0, 0, 13107};
    u32 leading = step, exponent = 0;
    for (unsigned shift = 16; shift; shift >>= 1) {
        if (leading >> shift) {
            leading >>= shift;
            exponent += shift;
        }
    }
    const u32 mantissa = (step << (31 - exponent)) & 0x7fffffffu;
    const u32 index = mantissa >> 23;
    const u32 fraction = (mantissa >> 7) & 65535;
    const i32 log = static_cast<i32>(exponent * q16) +
                    ppg_reader::interpolate(pitch_log2[index], pitch_log2[index + 1], fraction);
    const u32 grain = step <= pitch_grain_low ? 13107 : step >= pitch_grain_high ? 0 :
        static_cast<u32>(mul(static_cast<i32>(pitch_grain_high - step), pitch_grain_slope));
    return {step, log - pitch_reference_log2, grain};
}

void stock_initialize(StockVoice& v) {
    clear_bytes(&v, sizeof(StockVoice));
    v.actual = v.target = patches[0].controls;
    ppg_v2_bank_reset(&v.bank);
}

void stock_start(StockVoice& state, const int* saved) {
    if (saved) stock_controls(state, saved, true);
    auto& v = state.spectral;
    clear_bytes(&v, sizeof(StockSpectrum));
    v.position_q16 = v.held_position_q16 = mul(state.actual.q30[0], 15 * q16);
    v.hold_left = 1;
    v.falling = one;
    v.cycle_carry = (cycle_half / 2) / 2;
    state.control_tick = 0;
    state.last_pitch = 0;
    state.cached_pair_step = 0;
    state.active = true;
}

__attribute__((noinline)) static void refresh_stock(StockVoice& s, u32 tick, u32 pending) {
    if (pending) {
        for (u32 index = 0; index < 4; ++index) {
            const auto flag = 1u << index;
            if (!(pending & flag)) continue;
            auto& actual = s.actual.q30[index];
            const auto target = s.target.q30[index];
            const i32 difference = target - actual;
            if (difference >= -64 && difference <= 64) {
                actual = target;
                s.controls_pending &= ~flag;
            } else {
                actual += mul_bounded(difference, stock_control_smooth);
            }
        }
    }
    constexpr u32 duty_lane = 2;
    if (!tick || (pending & (1u << duty_lane))) {
        const u32 prior_mip = s.frame.duty.mip;
        s.frame.duty = ppg_reader::make_duty(static_cast<u32>(s.actual.q30[duty_lane]));
        if (s.frame.duty.mip != prior_mip) s.cached_pair_step = 0;
    }
    // The derived terms depend on distinct lanes. A first tick or a motion
    // selection still establishes a complete frame.
    if (!tick || (pending & 1u)) s.frame.base_q16 = mul_bounded(s.actual.q30[0], 15 * q16);
    if (!tick || (pending & 2u)) {
        const auto sweep = s.actual.q30[1];
        s.frame.amount_q16 = mul_bounded(mul_bounded(sweep, sweep), 30 * q16);
    }
    if (!tick) s.frame.motion = patches[s.preset].motion;
    if (!tick || (pending & 8u)) {
        const i32 ratio = s.frame.detune_ratio;
        const i32 mip = s.frame.detune_mip_q16;
        derive_twin(s.actual.q30[3], s.frame);
        if (s.frame.detune_ratio != ratio || s.frame.detune_mip_q16 != mip) s.cached_pair_step = 0;
    }
}

// Most samples only advance the fade and cadence. Keep the refresh call for
// a control tick that actually updates a lane, while preserving note/reset timing.
__attribute__((always_inline)) static inline void prepare_stock(StockVoice& s) {
    ppg_reader::advance_fade<221>(s.bank);
    const u32 tick = s.control_tick++;
    if (tick & 3) return;
    const u32 pending = s.controls_pending;
    if (!tick || pending) refresh_stock(s, tick, pending);
}

// This path receives a signed oscillator word reduced by two bits. Even
// INT32_MIN after this shift has magnitude 2^29. Indexing both edges of a
// segment therefore needs only entries 0 through 513 inclusive.
#ifdef PPG_PACK_ENTRY_ONLY
static_assert(sizeof(soft_round) / sizeof(*soft_round) == 514, "Wave pack clip table covers every native segment");
#endif
static inline i32 stock_soft_clip(i32 sample) {
    const u32 absolute = magnitude(sample);
    const u32 index = absolute >> 20;
    const i32 scaled = ppg_reader::interpolate_wave(soft_round[index], soft_round[index + 1],
                                               (absolute >> 4) & 65535u);
    return sample < 0 ? -scaled : scaled;
}

template<bool have_table>
__attribute__((always_inline)) static inline i32 stock_sample(StockVoice& state, u32 step, const unsigned* table) {
    auto& v = state.spectral;
    const auto& f = state.frame;
    const i32 travel = f.motion == 0 ? v.falling : f.motion == 1 ? v.rising : v.cycle;
    const i32 destination = reflect(f.base_q16 + mul_bounded(f.amount_q16, travel));
    if (--v.hold_left == 0) {
        v.held_position_q16 = ((destination + 8192) >> 14) << 14;
        v.hold_left = 441;
    }
    const i32 wanted = smooth_bounded(destination, v.held_position_q16, scrub_mix);
    v.position_q16 = smooth_bounded(v.position_q16, wanted, stock_position_smooth);

    if (state.cached_pair_step != step) {
        state.cached_pair_step = step;
        state.twin_step = static_cast<u32>(mul(static_cast<i32>(step), f.detune_ratio));
        const i32 boost = static_cast<i32>(f.duty.mip);
        state.primary_mip = static_cast<u32>(clamp(v.pitch.mip_q16 + boost, 0, 5 * q16));
        state.twin_mip = static_cast<u32>(clamp(v.pitch.mip_q16 + f.detune_mip_q16 + boost, 0, 5 * q16));
    }
    i32 sample = 0;
    if (have_table) {
        const ppg_reader::OscillatorFrame oscillator{
            step, state.twin_step, state.primary_mip, state.twin_mip,
            static_cast<u32>(v.position_q16), static_cast<u32>(reflect(v.position_q16 + f.spread_q16)),
            v.pitch.grain_q16, f.twin_mix_q16};
        sample = edge_reader::step(table, &v.oscillator, &state.bank.frame, &oscillator, &f.duty) >> 2;
    } else {
        v.oscillator.phase += step;
        v.oscillator.twin_phase += state.twin_step;
    }

    if (!(state.control_tick & 3)) {
        v.falling = smooth_bounded(v.falling, 0, stock_fall_decay);
        v.rising = smooth_bounded(v.rising, one, stock_rise_decay);
    }
    constexpr u32 half = cycle_half / 2;
    u32 cycle_step = one / half;
    v.cycle_carry += one % half;
    if (v.cycle_carry >= half) {
        ++cycle_step;
        v.cycle_carry -= half;
    }
    const auto delta = static_cast<i32>(cycle_step);
    v.cycle += v.cycle_position < half ? delta : -delta;
    if (++v.cycle_position == 2 * half) v.cycle_position = 0;
    return have_table ? mul_bounded(stock_soft_clip(sample), stock_trim) * 2 : 0;
}

#ifndef PPG_PACK_ENTRY_ONLY
bool stock_render(StockVoice& state, int* output, const unsigned* pitch, const int* effective, u32 frames) {
    if (!output || !pitch || !effective || (frames & 1) || frames > 2 * output_block) return false;
    const auto* table = wave_tables_ready();
    stock_controls(state, effective, false);
    for (u32 index = 0; index < frames; index += 2) {
        i32 audio = 0;
        if (state.active) {
            prepare_stock(state);
            const u32 first = pitch[index], second = pitch[index + 1];
            const u32 step = first + second;
            if (step && step <= static_cast<u32>(i32_max) && first <= step && second <= step) {
                const u32 mean = (step + 1) >> 1;
                const u32 previous = state.last_pitch;
                const u32 difference = mean > previous ? mean - previous : previous - mean;
                // Use the exact pair step for both oscillator phases. Recompute
                // only mip/grain tuning when pitch moves about 1.7 cents.
                if (difference > (previous >> 10)) {
                    state.last_pitch = mean;
                    state.spectral.pitch = field_pitch(mean);
                    state.cached_pair_step = 0;
                }
                audio = table ? stock_sample<true>(state, step, table) : stock_sample<false>(state, step, nullptr);
            }
        }
        // The unchanged Field mixer and stock half-band still see both native
        // frames; a 44.1 kHz sample is held for exactly one pair.
        output[index * 2] = audio;
        output[index * 2 + 1] = audio;
        output[index * 2 + 2] = audio;
        output[index * 2 + 3] = audio;
    }
    return true;
}

static i32 inner_sample(Synth& synth) {
    Frame frame;
    prepare_frame(synth, frame);
    i32 sum = 0;
    for (auto& voice : synth.playing) sum += voice_sample(voice, frame);
    return mul(sum, frame.master);
}

static void fir_push(Fir& fir, i32 sample) {
    if (++fir.head == 31) fir.head = 0;
    fir.history[fir.head] = sample;
}

static i32 fir_output(const Fir& fir) {
    i32 sum = 0;
    u32 index = fir.head;
    for (const auto coefficient : fir_coefficients) {
        sum += mul(fir.history[index], coefficient);
        index = index == 0 ? 30 : index - 1;
    }
    return add(sum, sum);
}

static i32 output_sample(Synth& synth) {
    fir_push(synth.decimator, inner_sample(synth));
    fir_push(synth.decimator, inner_sample(synth));
    return fir_output(synth.decimator);
}
#endif
}

#ifndef PPG_PACK_ENTRY_ONLY
using namespace ppg_synth;

extern "C" void ppg_v2_synth_initialize_44100(Synth* synth) {
    if (!synth) return;
    clear_bytes(synth, sizeof(Synth));
    synth->master = synth->master_target = master_default;
    ppg_v2_bank_reset(&synth->bank);
    ppg_v2_synth_preset(synth, 0, true);
}

extern "C" bool ppg_v2_synth_preset(Synth* synth, u32 preset, bool immediate) {
    if (!synth || preset >= presets) return false;
    const auto& patch = patches[preset];
    synth->preset = preset;
    synth->target = patch.controls;
    ppg_v2_bank_select(&synth->bank, patch.bank, immediate);
    if (immediate) synth->actual = synth->target;
    return true;
}

extern "C" bool ppg_v2_synth_bank(Synth* synth, u32 bank, bool immediate) {
    return synth && ppg_v2_bank_select(&synth->bank, bank, immediate);
}

extern "C" bool ppg_v2_synth_controls(Synth* synth, const Controls* controls) {
    if (!synth || !controls) return false;
    for (u32 i = 0; i < 4; ++i) synth->target.q30[i] = clamp(controls->q30[i], 0, one);
    return true;
}

extern "C" bool ppg_v2_synth_master(Synth* synth, i32 value) {
    if (!synth) return false;
    synth->master_target = clamp(value, 0, one);
    return true;
}

extern "C" bool ppg_v2_synth_note_on(Synth* synth, u32 id, u32 midi, u32 velocity) {
    if (!synth || midi > 127 || velocity > 127) return false;
    ppg_v2_synth_note_off(synth, id);
    if (velocity == 0) return true;
    Voice* selected = &synth->playing[0];
    for (auto& voice : synth->playing) {
        if (!voice.active) {
            selected = &voice;
            break;
        }
        const auto& best = *selected;
        if ((voice.releasing && !best.releasing) ||
            (voice.releasing == best.releasing && (voice.envelope < best.envelope ||
            (voice.envelope == best.envelope && voice.order < best.order)))) selected = &voice;
    }
    const i32 tail = selected->active ? selected->last : 0;
    auto& v = *selected;
    clear_bytes(&v, sizeof(Voice));
    const auto& patch = patches[synth->preset];
    v.active = true;
    v.id = id;
    v.order = ++synth->serial;
    v.pitch = midi_pitch[midi];
    v.velocity = static_cast<i32>(velocity * (one / 127) + divide(velocity * (one % 127) + 63, 127));
    v.attack_step = patch.attack_step;
    v.decay_rate = patch.decay_rate;
    v.sustain = patch.sustain;
    v.release_frames = patch.release_frames;
    v.position_q16 = v.held_position_q16 = mul(synth->actual.q30[0], 15 * q16);
    v.hold_left = 1;
    v.falling = one;
    v.cycle_carry = cycle_half / 2;
    v.steal_tail = tail;
    v.steal_mix_q16 = q16;
    v.steal_carry = fade_length / 2;
    return true;
}

extern "C" void ppg_v2_synth_note_off(Synth* synth, u32 id) {
    if (!synth) return;
    for (auto& voice : synth->playing) {
        if (voice.active && voice.id == id && !voice.releasing) begin_release(voice, voice.release_frames);
    }
}

extern "C" void ppg_v2_synth_panic(Synth* synth) {
    if (!synth) return;
    for (auto& voice : synth->playing) if (voice.active) begin_release(voice, 706);
}

extern "C" bool ppg_v2_synth_render_44100(Synth* synth, i32* left, i32* right, u32 frames) {
    if (!synth || !left || frames > output_block) return false;
    for (u32 index = 0; index < frames; ++index) {
        left[index] = output_sample(*synth);
        if (right) right[index] = left[index];
    }
    return true;
}

extern "C" bool ppg_v2_synth_render_88200(Synth* synth, i32* stereo, u32 frames) {
    if (!synth || !stereo || frames > 2 * output_block) return false;
    for (u32 index = 0; index < frames; ++index) {
        const i32 sample = inner_sample(*synth) * 2;
        stereo[2 * index] = sample;
        stereo[2 * index + 1] = sample;
    }
    return true;
}

extern "C" void ppg_v2_fir_reset(StereoFir* filters) {
    if (filters) clear_bytes(filters, sizeof(StereoFir));
}

extern "C" bool ppg_v2_fir_in_place_128(StereoFir* filters, i32* stereo) {
    if (!filters || !stereo || filters->left.head >= 31 || filters->right.head >= 31) return false;
    for (u32 index = 0; index < output_block; ++index) {
        const u32 input = 4 * index;
        fir_push(filters->left, stereo[input] >> 1);
        fir_push(filters->right, stereo[input + 1] >> 1);
        fir_push(filters->left, stereo[input + 2] >> 1);
        fir_push(filters->right, stereo[input + 3] >> 1);
        stereo[2 * index] = fir_output(filters->left);
        stereo[2 * index + 1] = fir_output(filters->right);
    }
    return true;
}
#endif
