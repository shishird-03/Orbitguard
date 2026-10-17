// Parametric curves: cubic Bezier (maneuver arcs) and uniform cubic B-spline (camera tour).
#pragma once
#include <vector>
#include "math/Mat4.h"

namespace og {

// de Casteljau evaluation, t in [0,1]
Vec3 bezierCubic(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, double t);
std::vector<Vec3> bezierPolyline(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, int segments);

// Uniform cubic B-spline through control points; u in [0,1] spans the whole curve.
// Endpoints are clamped by tripling the first/last control point so the path starts/ends on them.
Vec3 bsplineEval(const std::vector<Vec3>& ctrl, double u);

}  // namespace og
