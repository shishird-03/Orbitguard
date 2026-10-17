#include "geom/Curves.h"
#include <algorithm>

namespace og {

Vec3 bezierCubic(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, double t) {
    Vec3 a = lerp(p0, p1, t), b = lerp(p1, p2, t), c = lerp(p2, p3, t);
    Vec3 d = lerp(a, b, t), e = lerp(b, c, t);
    return lerp(d, e, t);
}

std::vector<Vec3> bezierPolyline(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, int segs) {
    std::vector<Vec3> out;
    for (int i = 0; i <= segs; ++i) out.push_back(bezierCubic(p0, p1, p2, p3, double(i) / segs));
    return out;
}

Vec3 bsplineEval(const std::vector<Vec3>& in, double u) {
    if (in.empty()) return {};
    std::vector<Vec3> c;
    c.push_back(in.front()); c.push_back(in.front());
    c.insert(c.end(), in.begin(), in.end());
    c.push_back(in.back()); c.push_back(in.back());
    int segs = int(c.size()) - 3;
    double s = std::clamp(u, 0.0, 1.0) * segs;
    int i = std::min(int(s), segs - 1);
    double t = s - i, t2 = t * t, t3 = t2 * t;
    // uniform cubic B-spline basis (1/6)[(1-t)^3, 3t^3-6t^2+4, -3t^3+3t^2+3t+1, t^3]
    double b0 = (1 - t) * (1 - t) * (1 - t) / 6.0;
    double b1 = (3 * t3 - 6 * t2 + 4) / 6.0;
    double b2 = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6.0;
    double b3 = t3 / 6.0;
    return c[i] * b0 + c[i + 1] * b1 + c[i + 2] * b2 + c[i + 3] * b3;
}

}  // namespace og
