// Keep the display routines compact. The audio callback uses the package's -O3 setting.
#if defined(__bfin__) && defined(__GNUC__)
#pragma GCC optimize ("O2")
#endif
#include "pose_motion.hpp"
#include "dsp/source/effects/modulation_math.hpp"

namespace pong_display_motion {
namespace {

constexpr int32_t one = 1024;
constexpr int32_t blade_depth = 11 * one;
constexpr int32_t roll_cosine = 920;
constexpr int32_t roll_sine = -449;
constexpr int32_t pitch_sine = -160;
struct Point { int32_t x, y; };
struct PackedPoint { int16_t x, y; }; // Q6 pixel positions from the authored paths.
struct Line { PackedPoint start, end; };
struct Mark { PackedPoint start, foot; uint8_t surface, slack[4]; };
struct PosePoints { Line joins[8]; Mark marks[6]; };
#include "pose_motion_points.inc"

int32_t fixed_product(int32_t a, int32_t b) {
    return (a * b + one / 2) >> 10;
}

int32_t rounded_ratio(int32_t numerator, int32_t denominator) {
    const uint32_t positive = denominator < 0 ? -denominator : denominator;
    const uint32_t magnitude = numerator < 0 ? -numerator : numerator;
    const auto quotient = static_cast<int32_t>(mod_fx::divide_unsigned(magnitude + (positive >> 1), positive));
    return (numerator < 0) != (denominator < 0) ? -quotient : quotient;
}

int32_t absolute(int32_t value) { return value < 0 ? -value : value; }

int32_t axis(uint32_t control, int32_t low, int32_t middle, int32_t high) {
    if (control == 32767u) return high;
    if (control < 16384u) return low + (((middle - low) * int32_t(control) + 8192) >> 14);
    return middle + (((high - middle) * int32_t(control - 16384u) + 8192) >> 14);
}

int32_t sine(int32_t degrees_q10) {
    const int32_t sign = degrees_q10 < 0 ? -1 : 1;
    int32_t angle = absolute(degrees_q10);
    if (angle >= 90 * one) angle = 180 * one - angle;
    const unsigned degree = static_cast<unsigned>(angle) >> 10;
    if (degree == 90u) return sign * one;
    return sign * (quarter_sine[degree] + fixed_product(quarter_sine[degree + 1u] - quarter_sine[degree], angle & 1023));
}

Point linear(const Matrix& matrix, Point point) {
    return {fixed_product(matrix.words[0], point.x) + fixed_product(matrix.words[3], point.y),
            fixed_product(matrix.words[1], point.x) + fixed_product(matrix.words[4], point.y)};
}

Point transform(const Matrix& matrix, Point point) {
    const Point value = linear(matrix, point);
    return {value.x + matrix.words[6], value.y + matrix.words[7]};
}

Matrix at(Matrix matrix, Point original, Point desired) {
    const Point value = linear(matrix, original);
    matrix.words[6] = desired.x - value.x;
    matrix.words[7] = desired.y - value.y;
    return matrix;
}

Point depth(int32_t si, int32_t co, int32_t scale) {
    const int32_t x = fixed_product(blade_depth, si);
    const int32_t y = fixed_product(fixed_product(-blade_depth, co), pitch_sine);
    return {fixed_product(scale, fixed_product(roll_cosine, x) - fixed_product(roll_sine, y)),
            fixed_product(scale, fixed_product(roll_sine, x) + fixed_product(roll_cosine, y))};
}

Point unpack(PackedPoint point) { return {point.x * 16, point.y * 16}; }

int32_t vector_length(Point p) {
    uint32_t remaining = p.x * p.x + p.y * p.y;
    uint32_t result = 0;
    uint32_t bit = 1u << 30;
    for (; bit > remaining; bit >>= 2) {}
    for (; bit; bit >>= 2) {
        if (remaining >= result + bit) {
            remaining -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
    }
    return static_cast<int32_t>(result);
}

} // namespace

Matrix spin_needle(uint32_t spin) {
    const int32_t angle = axis(spin, -75 * one, 0, 75 * one);
    const int32_t si = sine(angle), co = sine(angle + 90 * one);
    constexpr Point center{434 * one, 63038}; // The original SVG pivot, y = 61.561.
    return at({{co, si, 0, -si, co, 0, 0, 0, one}}, center, center);
}

Paddle paddle(pong_live_pose::Controls controls, bool doubles) {
    const int32_t scales[] = {963, one, 1085};
    const int32_t angles[] = {-50 * one, 25 * one, 70 * one};
    const int32_t scale = axis(controls.court, scales[0], scales[1], scales[2]);
    const int32_t angle = axis(controls.spin, angles[0], angles[1], angles[2]);
    const int32_t authored_scale = scales[(controls.court + 8192u) >> 14];
    const int32_t authored_angle = angles[(controls.spin + 8192u) >> 14];
    const Matrix identity{{one, 0, 0, 0, one, 0, 0, 0, one}};
    if (scale == authored_scale && angle == authored_angle) return {{identity, identity}, {identity, identity}};

    const int32_t si = sine(angle), co = sine(angle + 90 * one);
    const int32_t old_si = sine(authored_angle), old_co = sine(authored_angle + 90 * one);
    const int32_t vertical = rounded_ratio(scale * one, authored_scale);
    const int32_t horizontal = rounded_ratio(co * vertical, old_co);
    const int32_t shear = fixed_product(pitch_sine, fixed_product(si, vertical) - fixed_product(old_si, horizontal));
    // Change the authored orthographic projection in its own coordinate
    // system, then conjugate by the original -26 degree rotation.
    const int32_t cc = fixed_product(roll_cosine, roll_cosine);
    const int32_t ss = one - cc;
    const int32_t sc = fixed_product(roll_sine, roll_cosine);
    const int32_t cross = fixed_product(horizontal - vertical, sc);
    const Matrix projection{{
        vertical + fixed_product(horizontal - vertical, cc) - fixed_product(shear, sc),
        cross + fixed_product(shear, cc), 0,
        cross - fixed_product(shear, ss),
        vertical + fixed_product(horizontal - vertical, ss) + fixed_product(shear, sc), 0,
        0, 0, one
    }};
    const Point rear_shift = depth(si, co, scale);
    const Point old_shift = depth(old_si, old_co, authored_scale);
    const Point origins[] = {{(doubles ? 231 : 252) * one, (doubles ? 71 : 81) * one}, {268 * one, 85 * one}};
    Paddle result{};
    const unsigned bodies = doubles ? 2u : 1u;
    for (unsigned body = 0; body < bodies; ++body) {
        const Point origin = origins[body];
        result.face[body] = at(projection, origin, origin);
        result.rear[body] = at(projection, {origin.x + old_shift.x, origin.y + old_shift.y},
                                           {origin.x + rear_shift.x, origin.y + rear_shift.y});
    }
    return result;
}

Matrix connector(const Paddle& paddle, unsigned pose, unsigned body, unsigned index) {
    const Line& join = pose_points[pose].joins[body * 4u + index];
    const Point start = unpack(join.start), end = unpack(join.end);
    const Point front = transform(paddle.face[body], start);
    const Point rear = transform(paddle.rear[body], end);
    const int32_t dx = join.end.x - join.start.x, dy = join.end.y - join.start.y;
    const int32_t tx = (rear.x - front.x + 8) >> 4, ty = (rear.y - front.y + 8) >> 4;
    const int32_t length_squared = dx * dx + dy * dy;
    // Rotate and scale the authored line to connect the two projected sides.
    // The similarity matrix stays invertible when the rim is nearly edge-on.
    int32_t a = rounded_ratio((dx * tx + dy * ty) * one, length_squared);
    int32_t b = rounded_ratio((dx * ty - dy * tx) * one, length_squared);
    if (absolute(a) < 32 && absolute(b) < 32) {
        if (absolute(a) >= absolute(b)) a = a < 0 ? -32 : 32;
        else b = b < 0 ? -32 : 32;
    }
    return at({{a, b, 0, -b, a, 0, 0, 0, one}}, start, front);
}

Matrix rally_mark(const Paddle& paddle, unsigned pose, unsigned index) {
    const Mark& mark = pose_points[pose].marks[index];
    const unsigned body = mark.surface >> 1u;
    const Matrix& panel = (mark.surface & 1u) ? paddle.rear[body] : paddle.face[body];
    if (panel.words[0] == one && panel.words[4] == one && !panel.words[1] && !panel.words[3]) return panel;
    const Point start = unpack(mark.start), foot = unpack(mark.foot);
    const Point normal{start.x - foot.x, start.y - foot.y};
    // The cofactor matrix preserves a surface normal through nonuniform
    // scaling. Keep its original length, then rotate the mark without scale.
    const Point turned{
        fixed_product(panel.words[4], normal.x) - fixed_product(panel.words[1], normal.y),
        fixed_product(panel.words[0], normal.y) - fixed_product(panel.words[3], normal.x)
    };
    const int32_t before_length = vector_length(normal), after_length = vector_length(turned);
    const Point before{rounded_ratio(normal.x * one, before_length), rounded_ratio(normal.y * one, before_length)};
    const Point after{rounded_ratio(turned.x * one, after_length), rounded_ratio(turned.y * one, after_length)};
    const int32_t co = fixed_product(before.x, after.x) + fixed_product(before.y, after.y);
    const int32_t si = fixed_product(before.x, after.y) - fixed_product(before.y, after.x);
    const Point target = transform(panel, foot);
    int32_t margin = 0;
    for (unsigned surface = 0; surface < 4u; ++surface) {
        if (surface == mark.surface || mark.slack[surface] == 255u) continue;
        const auto& other = (surface & 1u) ? paddle.rear[surface >> 1u] : paddle.face[surface >> 1u];
        const int32_t encroachment = fixed_product(other.words[6] - panel.words[6], after.x) +
            fixed_product(other.words[7] - panel.words[7], after.y) - mark.slack[surface] * 64;
        if (encroachment > margin) margin = encroachment;
    }
    const int32_t clearance = before_length + margin;
    return at({{co, si, 0, -si, co, 0, 0, 0, one}}, start,
              {target.x + fixed_product(after.x, clearance), target.y + fixed_product(after.y, clearance)});
}

} // namespace pong_display_motion
