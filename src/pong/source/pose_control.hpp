#pragma once

#include <stdint.h>

namespace pong_live_pose {

// The owner supplies one coherent snapshot of normalized unsigned Q15 knobs.
// No stored bank or audio state is fetched by this policy.
struct Controls {
    uint32_t court;
    uint32_t rally;
    uint32_t spin;
};

struct Selection {
    unsigned material;
    unsigned pose;
    unsigned ball;
    unsigned arrow;
    unsigned marks;
};

constexpr unsigned first_pose = 2;
constexpr unsigned first_ball = first_pose + 54;
constexpr unsigned first_arrow = first_ball + 3;
constexpr unsigned no_layer = first_arrow + 6;

inline bool select(Controls controls, unsigned wall, unsigned players,
                   unsigned texture, Selection& out) {
    if (controls.court > 32767 || controls.rally > 32767 || controls.spin > 32767 ||
        wall >= 3 || players >= 2 || texture >= 3) {
        return false;
    }
    constexpr unsigned middle = 16384;
    // Nearest of the three approved authoring positions, with ties upwards.
    const unsigned court_pose = (controls.court + 8192u) >> 14u;
    const unsigned spin_pose = (controls.spin + 8192u) >> 14u;
    // Round each half of the Rally control to interpolate precisely through
    // the approved 1/3/6 mark anchors, including the center and endpoints.
    const unsigned marks = controls.rally < middle
        ? 1u + ((2u * controls.rally + 8192u) >> 14u)
        : 3u + ((3u * (controls.rally - middle) + 8192u) >> 14u);
    const unsigned material = wall == 0 ? no_layer : wall - 1;
    const unsigned pose = first_pose + ((texture * 2 + players) * 3 + court_pose) * 3 + spin_pose;
    const unsigned ball = first_ball + texture;
    // The arrow is directional and disappears only at the exact center,
    // independently of the paddle's coarser pose selection.
    const unsigned arrow = controls.spin == middle ? no_layer
        : first_arrow + texture * 2 + static_cast<unsigned>(controls.spin > middle);
    out = {material, pose, ball, arrow, marks};
    return true;
}

} // namespace pong_live_pose
