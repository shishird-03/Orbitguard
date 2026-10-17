#include "raster/ScanlineFill.h"
#include <algorithm>
#include <cmath>

namespace og {

namespace {
struct Edge { double yMax, x, invSlope; };
}

void scanlineFill(const std::vector<Vec2>& poly, std::vector<Span>& out) {
    size_t n = poly.size();
    if (n < 3) return;
    double yLo = 1e30, yHi = -1e30;
    for (auto& p : poly) { yLo = std::min(yLo, p.y); yHi = std::max(yHi, p.y); }
    int rowLo = int(std::ceil(yLo - 0.5)), rowHi = int(std::ceil(yHi - 0.5)) - 1;
    if (rowHi < rowLo) return;

    // Edge table: bucket each non-horizontal edge by the first scanline (row center) it crosses.
    std::vector<std::vector<Edge>> et(rowHi - rowLo + 1);
    for (size_t i = 0; i < n; ++i) {
        Vec2 a = poly[i], b = poly[(i + 1) % n];
        if (a.y == b.y) continue;
        if (a.y > b.y) std::swap(a, b);
        int first = int(std::ceil(a.y - 0.5));
        if (first > rowHi || b.y <= first + 0.5) continue;
        double inv = (b.x - a.x) / (b.y - a.y);
        double x = a.x + (first + 0.5 - a.y) * inv;
        if (first < rowLo) { x += (rowLo - first) * inv; first = rowLo; }
        et[first - rowLo].push_back({b.y, x, inv});
    }

    std::vector<Edge> aet;
    for (int y = rowLo; y <= rowHi; ++y) {
        auto& bucket = et[y - rowLo];
        aet.insert(aet.end(), bucket.begin(), bucket.end());
        double yc = y + 0.5;
        aet.erase(std::remove_if(aet.begin(), aet.end(), [&](const Edge& e) { return e.yMax <= yc; }), aet.end());
        std::sort(aet.begin(), aet.end(), [](const Edge& a, const Edge& b) { return a.x < b.x; });
        for (size_t k = 0; k + 1 < aet.size(); k += 2) {
            int x0 = int(std::ceil(aet[k].x - 0.5));
            int x1 = int(std::ceil(aet[k + 1].x - 0.5)) - 1;
            if (x1 >= x0) out.push_back({y, x0, x1});
        }
        for (auto& e : aet) e.x += e.invSlope;  // incremental update: coherence along the edge
    }
}

}  // namespace og
