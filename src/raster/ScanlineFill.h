// Scan-line polygon fill with an edge table (ET) and active edge table (AET).
#pragma once
#include <vector>
#include "math/Transform2D.h"

namespace og {

struct Span { int y, x0, x1; };  // inclusive pixel range on row y

// Even-odd rule, pixel-center sampling. Works for concave polygons.
void scanlineFill(const std::vector<Vec2>& poly, std::vector<Span>& out);

}  // namespace og
