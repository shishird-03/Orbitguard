// Fractal debris shapes by recursive midpoint-displacement subdivision of an octahedron.
#pragma once
#include <cstdint>
#include "geom/Mesh.h"

namespace og {

// Each level splits every triangle in 4 and pushes new edge midpoints in/out radially by a
// random amount that shrinks by `roughness` per level (fractal self-similarity).
// Deterministic for a given seed; shared edges reuse the same midpoint, so no cracks.
Mesh fractalRock(int levels, double roughness, uint32_t seed);

}  // namespace og
