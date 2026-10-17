#include "ssa/Kessler.h"
#include <algorithm>
#include <random>

namespace og {

int spawnCollision(Population& p, int a, int b, double jd, double vrelKms, const Config& c, KesslerStats& s) {
    int event = s.collisions++;
    std::mt19937_64 rng(uint64_t(c.seed) * 1000003ull + uint64_t(event));
    std::normal_distribution<double> N(0.0, 1.0);

    Vec3 ra, va, rb, vb;
    propagateOne(p, a, jd, ra, va);
    Vec3 point = ra;
    if (b >= 0) {
        propagateOne(p, b, jd, rb, vb);
        point = (ra + rb) * 0.5;
        p.alive[b] = 0;
    }
    p.alive[a] = 0;

    int n = int(std::lround(c.fragK * vrelKms * vrelKms));
    n = std::clamp(n, c.fragMin, c.fragMax);
    // ponytail: hard cap on population size; beyond it fragments are not tracked (would need SoA compaction).
    n = std::min(n, std::max(0, c.maxObjects - int(p.size())));

    int made = 0;
    for (int k = 0; k < n; ++k) {
        const Vec3& vParent = (b >= 0 && (k & 1)) ? vb : va;
        Vec3 dv{N(rng), N(rng), N(rng)};
        Vec3 dr{N(rng), N(rng), N(rng)};
        Vec3 v = vParent + dv * c.dispersionKms;
        Vec3 r = point + dr * 0.05;
        Elements e = rv2coe(r, v, jd);
        if (e.e >= 1.0 || e.a * (1.0 - e.e) - RE < c.reentryAltKm) { ++s.reentered; continue; }
        p.add("FRAG E" + std::to_string(event) + "-" + std::to_string(k), 900000 + s.fragments + made,
              Kind::Fragment, e, event);
        ++made;
    }
    s.fragments += made;
    return made;
}

}  // namespace og
