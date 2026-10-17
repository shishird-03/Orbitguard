// Kessler cascade: collisions turn two objects into a seeded fragment cloud that is screened like
// everything else, so chain collisions can emerge.
#pragma once
#include <vector>
#include "io/Config.h"
#include "orbit/Propagator.h"

namespace og {

struct KesslerStats {
    int collisions = 0, fragments = 0, reentered = 0;
    std::vector<float> history;   // live object count, sampled once per sim minute
    double lastSampleJD = 0;
};

// Breaks up a (and b, if b >= 0; b < 0 means an ASAT kill vehicle hitting a) at time jd.
// Fragment count scales with relative-speed squared (impact energy), clamped to [frag_min, frag_max].
// Deterministic: RNG is seeded from cfg.seed and the event index.
int spawnCollision(Population& p, int a, int b, double jd, double vrelKms, const Config& c, KesslerStats& s);

}  // namespace og
