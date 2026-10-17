// Composite 3D transforms built from Mat4 primitives.
#pragma once
#include "math/Mat4.h"

namespace og {

// Perifocal (PQW) -> ECI: R = Rz(raan) * Rx(inc) * Rz(argp). The orbital-element chain.
Mat4 perifocalToECI(double raan, double inc, double argp);

// Model matrix whose columns are the given orthonormal axes, placed at origin `o`.
Mat4 fromBasis(const Vec3& xAxis, const Vec3& yAxis, const Vec3& zAxis, const Vec3& o);

// Attitude frame for a satellite: x along velocity, z radial up, y completes the triad.
Mat4 velocityFrame(const Vec3& pos, const Vec3& vel, double size);

// Rotation about an arbitrary unit axis (Rodrigues), used by the arcball.
Mat4 rotateAxis(const Vec3& axis, double rad);

}  // namespace og
