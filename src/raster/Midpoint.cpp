#include "raster/Midpoint.h"
#include <cstdlib>

namespace og {

// Generalized to all octants: step along the major axis, decision variable picks the minor step.
void midpointLine(int x0, int y0, int x1, int y1, std::vector<Pixel>& out) {
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int x = x0, y = y0;
    if (dx >= dy) {
        int d = 2 * dy - dx;  // decision at the first midpoint
        for (int i = 0; i <= dx; ++i) {
            out.push_back({x, y});
            if (d > 0) { y += sy; d -= 2 * dx; }
            d += 2 * dy;
            x += sx;
        }
    } else {
        int d = 2 * dx - dy;
        for (int i = 0; i <= dy; ++i) {
            out.push_back({x, y});
            if (d > 0) { x += sx; d -= 2 * dy; }
            d += 2 * dx;
            y += sy;
        }
    }
}

// Eight-way symmetry: compute one octant (x from 0 to y), mirror the rest.
void midpointCircle(int cx, int cy, int r, std::vector<Pixel>& out) {
    int x = 0, y = r;
    int d = 1 - r;
    while (x <= y) {
        Pixel o[8] = {{cx + x, cy + y}, {cx - x, cy + y}, {cx + x, cy - y}, {cx - x, cy - y},
                      {cx + y, cy + x}, {cx - y, cy + x}, {cx + y, cy - x}, {cx - y, cy - x}};
        out.insert(out.end(), o, o + 8);
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; --y; }
        ++x;
    }
}

}  // namespace og
