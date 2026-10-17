#include "geom/Fractal.h"
#include <algorithm>
#include <map>
#include <random>

namespace og {

Mesh fractalRock(int levels, double roughness, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(-1.0, 1.0);
    std::vector<Vec3> v = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto& p : v) p = p * (1.0 + 0.3 * U(rng));  // irregular base shape
    std::vector<unsigned> t = {0, 2, 4, 2, 1, 4, 1, 3, 4, 3, 0, 4, 2, 0, 5, 1, 2, 5, 3, 1, 5, 0, 3, 5};

    double amp = 0.35;
    for (int l = 0; l < levels; ++l) {
        std::map<std::pair<unsigned, unsigned>, unsigned> mid;
        auto midpoint = [&](unsigned a, unsigned b) {
            auto key = std::make_pair(std::min(a, b), std::max(a, b));
            auto it = mid.find(key);
            if (it != mid.end()) return it->second;
            Vec3 m = (v[a] + v[b]) * 0.5;
            m = m * (1.0 + amp * U(rng));  // the displacement
            v.push_back(m);
            return mid[key] = unsigned(v.size() - 1);
        };
        std::vector<unsigned> nt;
        for (size_t i = 0; i < t.size(); i += 3) {
            unsigned a = t[i], b = t[i + 1], c = t[i + 2];
            unsigned ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            nt.insert(nt.end(), {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca});
        }
        t.swap(nt);
        amp *= roughness;
    }

    // Unshared vertices with face normals: a faceted, rocky look.
    Mesh m;
    for (size_t i = 0; i < t.size(); i += 3) {
        Vec3 a = v[t[i]], b = v[t[i + 1]], c = v[t[i + 2]];
        Vec3 n = normalize(cross(b - a, c - a));
        unsigned base = unsigned(m.pos.size());
        for (const Vec3& p : {a, b, c}) { m.pos.push_back(p); m.nrm.push_back(n); }
        m.tri.insert(m.tri.end(), {base, base + 1, base + 2});
    }
    return m;
}

}  // namespace og
