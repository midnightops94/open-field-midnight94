#pragma once

#include "pose_control.hpp"

namespace pong_display_motion {

// Stock Node::matrix uses Q10 column-major affine words.
struct Matrix {
    int32_t words[9];
};

struct Paddle {
    Matrix face[2];
    Matrix rear[2];
};

Matrix spin_needle(uint32_t spin);
Paddle paddle(pong_live_pose::Controls controls, bool doubles);
Matrix connector(const Paddle&, unsigned pose, unsigned body, unsigned index);
Matrix rally_mark(const Paddle&, unsigned pose, unsigned index);

} // namespace pong_display_motion
