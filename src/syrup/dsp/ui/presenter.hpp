#pragma once

#include "../integration/field_slot.hpp"

#include <cstdint>

namespace syrup::ui {

static constexpr int32_t unit = 1024;
static constexpr unsigned lane_count = 4;
static constexpr unsigned tint_count = 12;
static constexpr unsigned meter_count = 10;
static constexpr unsigned flow_count = 16;
static constexpr unsigned waist_count = 17;
static constexpr unsigned spill_count = 4;
static constexpr uint32_t clip_hold_ms = 160;

enum class Overlay : uint8_t { none, fx_off, browser, octave, other };
enum class Lane : uint8_t { cut, res, drive, mix };

struct Matrix {
    // Q10 affine lanes in the retained Field setter's column-major order.
    int32_t xx = unit, xy = 0, yx = 0, yy = unit, x = 0, y = 0;
};

inline bool operator==(const Matrix& a, const Matrix& b) noexcept {
    return a.xx == b.xx && a.xy == b.xy && a.yx == b.yx && a.yy == b.yy &&
           a.x == b.x && a.y == b.y;
}

struct Text {
    char data[24]{};
};

struct Input {
    field::Owner owner{};
    const field::Snapshot* snapshot = nullptr;
    uint32_t now_ms = 0;
    bool shifted = false;
    Overlay native_overlay = Overlay::none;
    bool trim_available = false;
};

struct Frame {
    bool admitted = false;
    bool native_fx_off = false;
    Overlay native_overlay = Overlay::none;
    Parameters displayed{};
    const char* labels[lane_count]{};
    Text values[lane_count]{};
    Matrix needle{};
    unsigned mix_ticks = 0;
    Matrix bottle{}, pinch_cues{}, pool{}, butter{}, melt{}, drop{};
    unsigned tint = 0, flow = 0, waist = 0, spill = 0;
    bool show_melt = false, show_drop = false;
    bool show_wet = false;
    bool show_drip = false;
    bool show_pinch = false;
    bool show_clip = false;
};

namespace detail {

inline int32_t multiply(int32_t a, int32_t b) noexcept {
    return (a * b + (a * b < 0 ? -unit / 2 : unit / 2)) / unit;
}

inline int32_t normalized(uint16_t value) noexcept {
    return int32_t((uint32_t(value) * unit + 32767u) / 65535u);
}

inline Matrix anchored(int32_t xx, int32_t xy, int32_t yx, int32_t yy,
                       int32_t at_x, int32_t at_y) noexcept {
    return {xx, xy, yx, yy, at_x * (unit - xx) - at_y * yx,
            at_y * (unit - yy) - at_x * xy};
}

inline Matrix translated(int32_t x, int32_t y) noexcept {
    return {unit, 0, 0, unit, x, y};
}

struct Writer {
    Text& target;
    unsigned used = 0;

    void put(char c) noexcept {
        if (used + 1 < sizeof(target.data)) target.data[used++] = c;
    }

    void append(const char* value) noexcept {
        while (*value) put(*value++);
    }

    void number(unsigned value) noexcept {
        char digits[10];
        unsigned count = 0;
        do {
            digits[count++] = char('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count != 0) put(digits[--count]);
    }

    void hundredths(unsigned value) noexcept {
        number(value / 100);
        const unsigned remainder = value % 100;
        if (remainder == 0) return;
        put('.');
        put(char('0' + remainder / 10));
        if (remainder % 10 != 0) put(char('0' + remainder % 10));
    }
};

inline Text format(Lane lane, const Parameters& values, bool trim) noexcept {
    Text text{};
    Writer writer{text};
    if (lane == Lane::drive) {
        if (!trim) {
            const unsigned tenths = (uint32_t(values.drive) * 180u + 32767u) / 65535u;
            writer.number(tenths / 10);
            writer.put('.');
            writer.put(char('0' + tenths % 10));
            return text;
        }
        const int millidecibels = wet_trim_millidecibels(values.wet_trim);
        const int value = (millidecibels + (millidecibels < 0 ? -5 : 5)) / 10;
        if (value < 0) writer.put('-');
        if (value > 0) writer.put('+');
        writer.hundredths(unsigned(value < 0 ? -value : value));
        writer.append(" db");
    } else if (lane == Lane::res) {
        writer.number((uint32_t(values.resonance) * 100 + 32767u) / 65535u);
    }
    return text;
}

inline const char* title(Lane lane, bool shift) noexcept {
    constexpr const char* fine[] = {"freq fine", "res fine", "wet trim", "mix fine"};
    const unsigned index = static_cast<unsigned>(lane);
    return shift ? fine[index] : field::lfo_label(index);
}

inline Parameters select_values(const field::Snapshot& snapshot, bool current_activity) noexcept {
    Parameters output = snapshot.stored;
    if (current_activity) {
        const unsigned mask = snapshot.effective_mask;
        if (mask & 1u) output.cutoff = snapshot.effective.cutoff;
        if (mask & 2u) output.resonance = snapshot.effective.resonance;
        if (mask & 4u) output.drive = snapshot.effective.drive;
        if (mask & 8u) output.mix = snapshot.effective.mix;
    }
    return output;
}

// Pong Spin uses a 150-degree sweep with one-degree Q10 sine samples. The
// authored group translation places the pivot; only the needle rotates here.
inline Matrix frequency_needle(uint16_t cutoff) noexcept {
    static constexpr int16_t quarter_sine[] = {
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213,
    230, 248, 265, 282, 299, 316, 333, 350, 367, 384, 400, 416, 433,
    449, 465, 481, 496, 512, 527, 543, 558, 573, 587, 602, 616, 630,
    644, 658, 672, 685, 698, 711, 724, 737, 749, 761, 773, 784, 796,
    807, 818, 828, 839, 849, 859, 868, 878, 887, 896, 904, 912, 920,
    928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989, 994, 998,
    1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023, 1024, 1024,
};
    const unsigned q15 = cutoff >> 1;
    const int angle = q15 == 32767u ? 75 * unit :
                      -75 * unit + int((75 * unit * q15 + 8192u) >> 14);
    const auto sine = [&](int degrees) -> int {
        const int sign = degrees < 0 ? -1 : 1;
        unsigned phase = degrees < 0 ? unsigned(-degrees) : unsigned(degrees);
        if (phase > 90 * unit) phase = 180 * unit - phase;
        const unsigned index = phase >> 10;
        if (index == 90) return sign * unit;
        const int next = quarter_sine[index + 1] - quarter_sine[index];
        return sign * (quarter_sine[index] + ((next * int(phase & 1023) + 512) >> 10));
    };
    const int si = sine(angle), co = sine(angle + 90 * unit);
    return {co,si,-si,co,0,0};
}

inline void set_visuals(Frame& frame, uint32_t now_ms) noexcept {
    const auto& values = frame.displayed;
    const int32_t drive = normalized(values.drive);
    const int32_t res = normalized(values.resonance);
    static constexpr int16_t sine[] = {
        0,265,512,724,887,989,1024,989,887,724,512,265,
        0,-265,-512,-724,-887,-989,-1024,-989,-887,-724,-512,-265,0
    };
    // The fixed outlet is the rotation pivot. The cap inherits precisely this matrix.
    const auto sin_phase = [&](unsigned start, unsigned span) {
        const unsigned phase = span * values.cutoff;
        const unsigned step = (start + phase / 65535u) % 24;
        const unsigned fraction = (phase % 65535u) * 1024u / 65535u;
        return sine[step] + multiply(sine[step+1]-sine[step],fraction);
    };
    frame.needle = frequency_needle(values.cutoff);
    const int sn = sin_phase(22,4), cs = sin_phase(4,4);
    frame.bottle = anchored(cs,sn,-sn,cs,249,124);
    // Retained contours narrow the center only. The two end regions stay
    // fixed in bottle coordinates, and every part inherits the same Freq tilt.
    frame.waist = (drive*(waist_count-1) + unit/2)/unit;
    frame.pinch_cues = anchored(unit-multiply(87,drive),multiply(174,drive),
                                multiply(174,drive),unit-multiply(348,drive),184,93);

    // Res controls the pour rate. Drive adds pressure as the body narrows.
    const int volume = (3*res + 2*drive) / 5;
    frame.flow = (volume*(flow_count-1) + unit/2) / unit;
    frame.pool = anchored(420 + multiply(690,volume),0,0,
                          540 + multiply(510,volume),283,148);
    frame.show_wet = !frame.native_fx_off && values.mix != 0;
    frame.show_drip = frame.show_wet && volume >= 500;
    frame.mix_ticks = (meter_count * values.mix + 32767u) / 65535u;
    frame.tint = frame.show_wet ? (uint32_t(values.mix)*tint_count + 65534u)/65535u : 0;
    frame.show_pinch = values.drive > 2621;

    const int32_t mix = normalized(values.mix);
    const int32_t heat = drive < mix ? drive : mix;
    const int32_t melt = !frame.native_fx_off && heat > 704 ? (heat-704)*unit/320 : 0;
    frame.show_melt = melt != 0;
    frame.butter = anchored(unit+multiply(100,melt),0,0,unit-multiply(235,melt),313,140);
    frame.melt = anchored(160+multiply(864,melt),0,0,220+multiply(804,melt),312,137);
    const int32_t wettest = res < heat ? res : heat;
    if (!frame.native_fx_off && wettest >= 720)
        frame.spill = 1 + (wettest-720)*spill_count/305;
    // At a full overflow a drop forms just below the ceramic rim once per second.
    const unsigned phase = now_ms & 1023u;
    frame.show_drop = frame.spill == spill_count && phase < 730u;
    frame.drop = translated(0,static_cast<int32_t>(phase*phase/32u));
}

} // namespace detail

// Instance belongs to the GUI owner. Every render receives fresh stored and
// displayed data from that owner; no LFO packet or knob value is retained here.
class Presenter {
public:
    void close() noexcept { *this = Presenter{}; }

    Frame render(const Input& input) noexcept {
        Frame frame{};
        if (!field::valid(input.owner) || !input.snapshot ||
            !field::same(input.owner, input.snapshot->owner)) {
            close();
            return frame;
        }
        const auto& snapshot = *input.snapshot;
        if (!field::same(input.owner, owner_)) {
            close();
            owner_ = input.owner;
        }

        frame.admitted = true;
        frame.native_fx_off = snapshot.bypass;
        frame.native_overlay = input.native_overlay;
        const bool current_activity = snapshot.playing && !snapshot.bypass &&
                                      field::same(input.owner, snapshot.activity.owner);
        frame.displayed = detail::select_values(snapshot, current_activity);
        for (unsigned index = 0; index < lane_count; ++index) {
            const auto lane = static_cast<Lane>(index);
            const bool shifted_lane = input.shifted && (lane != Lane::drive || input.trim_available);
            frame.labels[index] = detail::title(lane, shifted_lane);
            frame.values[index] = detail::format(lane, frame.displayed, shifted_lane);
        }

        detail::set_visuals(frame,input.now_ms);
        if (current_activity) {
            const uint32_t serial = snapshot.activity.clip_serial;
            const uint32_t advance = serial - clip_serial_;
            const bool new_event = have_clip_serial_ ? advance != 0 && advance < 0x80000000u
                                                     : snapshot.activity.clipped;
            if (new_event) {
                clip_ms_ = input.now_ms;
                clip_ = true;
            }
            if (!have_clip_serial_ || new_event) clip_serial_ = serial;
            have_clip_serial_ = true;
        } else {
            clip_ = false;
            have_clip_serial_ = false;
        }
        frame.show_clip = clip_ && uint32_t(input.now_ms - clip_ms_) < clip_hold_ms;
        if (!frame.show_clip) clip_ = false;

        return frame;
    }

private:
    field::Owner owner_{};
    uint32_t clip_serial_ = 0, clip_ms_ = 0;
    bool have_clip_serial_ = false, clip_ = false;
};

} // namespace syrup::ui
