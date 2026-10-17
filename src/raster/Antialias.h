// Xiaolin Wu antialiased lines: each step lights two pixels weighted by coverage.
#pragma once
#include <vector>

namespace og {

struct APixel { int x, y; float a; };

void wuLine(double x0, double y0, double x1, double y1, std::vector<APixel>& out);

}  // namespace og
