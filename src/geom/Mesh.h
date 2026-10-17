// Polygon meshes (indexed triangles). Pure geometry: the renderer turns these into display lists.
#pragma once
#include <vector>
#include "math/Mat4.h"

namespace og {

struct Mesh {
    std::vector<Vec3> pos, nrm;
    std::vector<float> uv;           // 2 per vertex (may be empty)
    std::vector<float> col;          // 3 per vertex (may be empty)
    std::vector<unsigned> tri;       // 3 indices per triangle
    void append(const Mesh& o, const Mat4& xf, const Vec3& color);
    size_t triangles() const { return tri.size() / 3; }
};

Mesh uvSphere(int stacks, int slices);  // unit sphere, u = longitude, v = latitude
Mesh box(const Vec3& half);
Mesh disc(int segments, double depth);  // shallow paraboloid dish facing +z
Mesh satelliteModel();                   // bus + two solar wings + dish antenna, ~unit size

}  // namespace og
