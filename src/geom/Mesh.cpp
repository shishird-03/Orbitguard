#include "geom/Mesh.h"

namespace og {

void Mesh::append(const Mesh& o, const Mat4& xf, const Vec3& c) {
    unsigned base = unsigned(pos.size());
    for (size_t i = 0; i < o.pos.size(); ++i) {
        pos.push_back(xf.transformPoint(o.pos[i]));
        nrm.push_back(normalize(xf.transformDir(o.nrm[i])));
        col.push_back(float(c.x)); col.push_back(float(c.y)); col.push_back(float(c.z));
    }
    for (unsigned t : o.tri) tri.push_back(base + t);
}

Mesh uvSphere(int stacks, int slices) {
    Mesh m;
    for (int i = 0; i <= stacks; ++i) {
        double lat = -PI / 2 + PI * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            double lon = -PI + 2 * PI * j / slices;
            Vec3 p{std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat)};
            m.pos.push_back(p);
            m.nrm.push_back(p);
            m.uv.push_back(float(j) / slices);
            m.uv.push_back(float(i) / stacks);
        }
    }
    int row = slices + 1;
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            unsigned a = i * row + j, b = a + 1, c = a + row, d = c + 1;
            m.tri.insert(m.tri.end(), {a, b, d, a, d, c});
        }
    return m;
}

Mesh box(const Vec3& h) {
    Mesh m;
    const Vec3 n[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto& f : n) {
        // two in-plane axes for this face, ordered so the quad winds counter-clockwise from outside
        Vec3 u = std::fabs(f.x) > 0.5 ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
        Vec3 v = cross(f, u);
        unsigned base = unsigned(m.pos.size());
        const double s[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (auto& q : s) {
            Vec3 p = f + u * q[0] + v * q[1];
            m.pos.push_back({p.x * h.x, p.y * h.y, p.z * h.z});
            m.nrm.push_back(f);
        }
        m.tri.insert(m.tri.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

Mesh disc(int seg, double depth) {
    Mesh m;
    m.pos.push_back({0, 0, 0});
    m.nrm.push_back({0, 0, 1});
    for (int i = 0; i <= seg; ++i) {
        double a = 2 * PI * i / seg;
        m.pos.push_back({std::cos(a), std::sin(a), depth});
        m.nrm.push_back(normalize({-std::cos(a) * depth, -std::sin(a) * depth, 1}));
    }
    for (int i = 1; i <= seg; ++i) m.tri.insert(m.tri.end(), {0u, unsigned(i), unsigned(i + 1)});
    return m;
}

Mesh satelliteModel() {
    Mesh m;
    m.append(box({0.5, 0.35, 0.35}), Mat4(), {0.85, 0.7, 0.3});                          // gold-foil bus
    Mesh wing = box({0.3, 1.1, 0.02});
    m.append(wing, translate({0, 1.5, 0}), {0.15, 0.25, 0.6});                          // solar wings
    m.append(wing, translate({0, -1.5, 0}), {0.15, 0.25, 0.6});
    m.append(box({0.03, 0.06, 0.03}), translate({0, 0.38, 0}), {0.6, 0.6, 0.6});        // boom stubs
    m.append(box({0.03, 0.06, 0.03}), translate({0, -0.38, 0}), {0.6, 0.6, 0.6});
    m.append(box({0.03, 0.03, 0.15}), translate({0, 0, -0.45}), {0.8, 0.8, 0.8});       // antenna mast (nadir)
    m.append(disc(16, 0.12), translate({0, 0, -0.6}) * rotateX(PI) * scale({0.3, 0.3, 0.3}), {0.9, 0.9, 0.9});
    return m;
}

}  // namespace og
