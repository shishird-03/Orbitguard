#include "math/Transform3D.h"

namespace og {

Mat4 perifocalToECI(double raan, double inc, double argp) {
    return rotateZ(raan) * rotateX(inc) * rotateZ(argp);
}

Mat4 fromBasis(const Vec3& x, const Vec3& y, const Vec3& z, const Vec3& o) {
    Mat4 r;
    r.at(0, 0) = x.x; r.at(1, 0) = x.y; r.at(2, 0) = x.z;
    r.at(0, 1) = y.x; r.at(1, 1) = y.y; r.at(2, 1) = y.z;
    r.at(0, 2) = z.x; r.at(1, 2) = z.y; r.at(2, 2) = z.z;
    r.at(0, 3) = o.x; r.at(1, 3) = o.y; r.at(2, 3) = o.z;
    return r;
}

Mat4 velocityFrame(const Vec3& pos, const Vec3& vel, double size) {
    Vec3 z = normalize(pos);
    Vec3 y = normalize(cross(z, vel));
    Vec3 x = cross(y, z);
    return fromBasis(x, y, z, pos) * scale({size, size, size});
}

Mat4 rotateAxis(const Vec3& axisIn, double a) {
    Vec3 k = normalize(axisIn);
    double c = std::cos(a), s = std::sin(a), t = 1 - c;
    Mat4 r;
    r.at(0, 0) = t * k.x * k.x + c;       r.at(0, 1) = t * k.x * k.y - s * k.z; r.at(0, 2) = t * k.x * k.z + s * k.y;
    r.at(1, 0) = t * k.x * k.y + s * k.z; r.at(1, 1) = t * k.y * k.y + c;       r.at(1, 2) = t * k.y * k.z - s * k.x;
    r.at(2, 0) = t * k.x * k.z - s * k.y; r.at(2, 1) = t * k.y * k.z + s * k.x; r.at(2, 2) = t * k.z * k.z + c;
    return r;
}

}  // namespace og
