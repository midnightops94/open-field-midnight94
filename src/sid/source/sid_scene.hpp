#pragma once

#include "sid_core.hpp"
#include "scene_tables.hpp"

namespace sid_scene {

enum PathRole { BodyVoice, EdgeVoice, BodyPulse, EdgePulse, Filter, CutTick, Circuit, PathCount };
constexpr int pixel = 1024;
constexpr unsigned path_points[PathCount] = {121, 121, 5, 5, 33, 2, 15};
constexpr unsigned card_duration = 922;  // 900 ms in the stock Q10 frame clock.
constexpr unsigned no_layer = 255;
constexpr unsigned no_slot = 8;

struct Point { int x, y; };
struct Values { int control[4]; unsigned mode[4]; };
struct Geometry {
    int duty;
    int weight[4];
    unsigned cutoff;
    unsigned response;
    unsigned drive;
    unsigned pulse_layer;
    unsigned wire_layer;
};

inline int normalized_float(unsigned bits, int base) {
    const unsigned exponent = (bits >> 23) & 255u;
    if (exponent == 255u) return base;
    if (bits & 0x80000000u || exponent < 111u) return 0;
    if (bits >= 0x3f800000u) return 32768;
    const unsigned shift = 135u - exponent;
    return static_cast<int>(((bits & 0x7fffffu) + 0x800000u + (1u << (shift - 1))) >> shift);
}

inline Values values(const int* saved, const unsigned* displayed) {
    Values result;
    for (unsigned i = 0; i < 4; ++i) {
        int control = sid::control(saved[i]);
        result.control[i] = displayed ? normalized_float(displayed[i], control) : control;
        result.mode[i] = sid::choice(saved[i + 4]);
    }
    return result;
}

inline bool same(const Values& a, const Values& b) {
    for (unsigned i = 0; i < 4; ++i) {
        if (a.control[i] != b.control[i] || a.mode[i] != b.mode[i]) return false;
    }
    return true;
}

constexpr unsigned paths(PathRole first, PathRole second) {
    return (1u << first) | (1u << second);
}

inline unsigned dirty_paths(const Values& prior, const Values& next) {
    unsigned dirty = 0;
    const bool duty = prior.control[1] != next.control[1];
    if (duty || prior.control[0] != next.control[0] || (prior.mode[1] == 0) != (next.mode[1] == 0))
        dirty |= paths(BodyVoice, EdgeVoice);
    if (duty) dirty |= paths(BodyPulse, EdgePulse);
    if (prior.control[2] != next.control[2]) dirty |= paths(Filter, CutTick);
    if (prior.mode[2] != next.mode[2]) dirty |= 1u << Filter;
    if (prior.control[3] != next.control[3]) dirty |= 1u << Circuit;
    return dirty;
}

inline unsigned percent(int value) {
    return (static_cast<unsigned>(value) * 100u + 16384u) >> 15;
}

inline unsigned bypass_layer(int amount) {
    return (amount >= 2949) + (amount >= 8847) + (amount >= 14745) + (amount >= 20643);
}

inline Geometry geometry(const Values& v) {
    Geometry result;
    result.duty = sid::table(sid_tables::duty_mean, v.control[1], 0);
    const unsigned bank = v.mode[1] == 0 ? 0 : 4;
    for (unsigned i = 0; i < 4; ++i) result.weight[i] = sid::table(sid_tables::wave, v.control[0], bank + i);
    result.cutoff = v.control[2];
    result.drive = v.control[3];
    result.response = v.mode[2];
    const int depth = sid::table(sid_tables::drive, v.control[3]);
    const int pulse = sid::qmul(sid::qmul(depth, 20972), result.weight[1]);
    const int wire = sid::qmul(sid::qmul(depth, 23593), result.weight[2] + result.weight[3]);
    result.pulse_layer = v.mode[3] == 1 ? bypass_layer(pulse) : no_layer;
    result.wire_layer = v.mode[3] == 2 ? bypass_layer(wire) : no_layer;
    return result;
}

inline int lerp(int a, int b, unsigned position) {
    return a + ((b - a) * static_cast<int>(position & 1023u) + 512) / 1024;
}

inline Point point(PathRole role, unsigned i, const Geometry& g) {
    using namespace sid_scene_tables;
    if (role == BodyVoice || role == EdgeVoice) {
        const bool body = role == BodyVoice;
        const int t = wave_time[i];
        const bool high = t < g.duty;
        int value = sid::qmul(g.weight[0], wave[i][0]) + (high ? g.weight[1] : -g.weight[1]);
        value += sid::qmul(g.weight[2], wave[i][1]);
        if (high) value += sid::qmul(g.weight[3], wave[i][2]);
        const int x = (body ? 200 : 10) * pixel + ((body ? 80 : 66) * t >> 5);
        return {x, (body ? 115 : 52) * pixel - ((body ? 10 : 7) * value >> 5)};
    }
    if (role == BodyPulse || role == EdgePulse) {
        const bool body = role == BodyPulse;
        const int start = (body ? 207 : 10) * pixel;
        const int level = (body ? 143 : 189) * pixel;
        const int width = 66 * pixel;
        const int split = start + (66 * g.duty >> 5);
        return {i < 2 ? start : i < 4 ? split : start + width,
                i == 1 || i == 2 ? level - (body ? 10 : 15) * pixel : level};
    }
    if (role == Filter || role == CutTick) {
        const unsigned row = g.cutoff >> 10;
        const unsigned successor = row + (row != 32);
        if (role == CutTick) {
            return {4 * pixel + lerp(cut_tick[row], cut_tick[successor], g.cutoff), (i ? 78 : 70) * pixel};
        }
        const int a = filter[row][g.response][i];
        const int b = filter[successor][g.response][i];
        const int y = 42 * pixel + lerp(a * 128, b * 128, g.cutoff);
        return {405 * pixel + 2080 * static_cast<int>(i), y};
    }
    const unsigned row = g.drive >> 10;
    const unsigned successor = row + (row != 32);
    const int tip = lerp(circuit_tip[row], circuit_tip[successor], g.drive) * 128;
    return {circuit_x[i] + 4 * pixel, 204 * pixel + sid::qmul(tip - 184 * pixel, circuit_bend[i])};
}

constexpr const char* title[4] = {"stack", "link", "response", "route"};
constexpr const char* choice_name[4][3] = {
    {"tight", "sub", "fifth"}, {"off", "sync", "ring"},
    {"lp", "bp", "hp"}, {"blend", "pulse", "wire"},
};

inline unsigned next_notice(unsigned prior, unsigned slot, unsigned axis, unsigned mode) {
    if (slot >= 8 || axis >= 4 || mode >= 3) return prior;
    return ((prior + 128) & ~127u) | (slot << 4) | (axis << 2) | mode;
}

struct Card {
    unsigned consumed;
    unsigned remaining;
    unsigned slot;
    unsigned axis;
    unsigned mode;

    void clear(unsigned current_notice) {
        consumed = current_notice;
        remaining = 0;
    }

    bool update(unsigned notice, unsigned selected_slot, const Values& v, unsigned delta) {
        if (notice != consumed) {
            consumed = notice;
            slot = (notice >> 4) & 7u;
            axis = (notice >> 2) & 3u;
            mode = notice & 3u;
            remaining = card_duration;
        } else {
            remaining = delta < remaining ? remaining - delta : 0;
        }
        if (slot != selected_slot || mode != v.mode[axis]) remaining = 0;
        return remaining != 0;
    }
};
}
