// Midpoint (Bresenham-family) line and circle scan conversion. Integer arithmetic only.
#pragma once
#include <vector>

namespace og {

struct Pixel { int x, y; };

void midpointLine(int x0, int y0, int x1, int y1, std::vector<Pixel>& out);
void midpointCircle(int cx, int cy, int r, std::vector<Pixel>& out);

}  // namespace og
