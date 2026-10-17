#include "math/Transform2D.h"
#include <cmath>

namespace og {

Mat3::Mat3() {
    for (int i = 0; i < 9; ++i) m[i] = (i % 4 == 0) ? 1.0 : 0.0;
}

Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += m[i * 3 + k] * o.m[k * 3 + j];
            r.m[i * 3 + j] = s;
        }
    return r;
}

Vec2 Mat3::apply(Vec2 p) const {
    double x = m[0] * p.x + m[1] * p.y + m[2];
    double y = m[3] * p.x + m[4] * p.y + m[5];
    double w = m[6] * p.x + m[7] * p.y + m[8];
    return {x / w, y / w};
}

Mat3 translate2D(double tx, double ty) { Mat3 r; r.m[2] = tx; r.m[5] = ty; return r; }

Mat3 rotate2D(double a) {
    Mat3 r; double c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[1] = -s; r.m[3] = s; r.m[4] = c;
    return r;
}

Mat3 scale2D(double sx, double sy) { Mat3 r; r.m[0] = sx; r.m[4] = sy; return r; }

}  // namespace og
