// 2D homogeneous transforms (3x3) for the HUD: radar blips, minimap pan/zoom.
#pragma once

namespace og {

struct Vec2 { double x = 0, y = 0; };

struct Mat3 {
    double m[9];  // row-major: m[row*3 + col]
    Mat3();       // identity
    Mat3 operator*(const Mat3& o) const;
    Vec2 apply(Vec2 p) const;
};

Mat3 translate2D(double tx, double ty);
Mat3 rotate2D(double rad);
Mat3 scale2D(double sx, double sy);

}  // namespace og
