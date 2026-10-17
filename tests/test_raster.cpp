// Midpoint line/circle, scan-line fill, Cohen-Sutherland, Sutherland-Hodgman, Wu.
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include "raster/Antialias.h"
#include "raster/Clipping.h"
#include "raster/Midpoint.h"
#include "raster/ScanlineFill.h"

using namespace og;

static long filled(const std::vector<Vec2>& poly) {
    std::vector<Span> s;
    scanlineFill(poly, s);
    long n = 0;
    for (auto& sp : s) n += sp.x1 - sp.x0 + 1;
    return n;
}

int main() {
    // lines in all octants: endpoints hit, 8-connected, pixel count = major axis + 1
    int ends[][4] = {{0, 0, 10, 3}, {0, 0, 3, 10}, {0, 0, -10, 3}, {0, 0, -3, -10},
                     {5, 5, 5, -5}, {-4, 2, 7, 2}, {0, 0, 7, 7}};
    for (auto& e : ends) {
        std::vector<Pixel> px;
        midpointLine(e[0], e[1], e[2], e[3], px);
        assert(px.front().x == e[0] && px.front().y == e[1] && px.back().x == e[2] && px.back().y == e[3]);
        assert(int(px.size()) == std::max(std::abs(e[2] - e[0]), std::abs(e[3] - e[1])) + 1);
        for (size_t i = 1; i < px.size(); ++i)
            assert(std::abs(px[i].x - px[i - 1].x) <= 1 && std::abs(px[i].y - px[i - 1].y) <= 1);
    }
    // circle: every pixel near the radius, 8-way symmetric
    std::vector<Pixel> c;
    midpointCircle(0, 0, 37, c);
    std::set<std::pair<int, int>> cs;
    for (auto& p : c) {
        cs.insert({p.x, p.y});
        assert(std::abs(p.x * p.x + p.y * p.y - 37 * 37) <= 37);
    }
    for (auto& p : c) assert(cs.count({p.y, -p.x}) && cs.count({-p.x, -p.y}));
    assert(cs.count({37, 0}) && cs.count({0, -37}));

    // scan-line fill areas
    assert(filled({{0, 0}, {10, 0}, {10, 5}, {0, 5}}) == 50);
    assert(filled({{0, 0}, {6, 0}, {6, 2}, {2, 2}, {2, 6}, {0, 6}}) == 6 * 2 + 2 * 4);  // concave L
    long tri = filled({{0, 0}, {100, 0}, {0, 100}});
    assert(std::abs(tri - 5000) < 100);

    // Cohen-Sutherland
    ClipRect r{0, 0, 100, 50};
    double x0 = 10, y0 = 10, x1 = 20, y1 = 20;
    assert(cohenSutherland(x0, y0, x1, y1, r) && x0 == 10 && y1 == 20);  // inside: untouched
    x0 = -10; y0 = 60; x1 = -5; y1 = 80;
    assert(!cohenSutherland(x0, y0, x1, y1, r));                          // outside: rejected
    x0 = -50; y0 = 25; x1 = 150; y1 = 25;
    assert(cohenSutherland(x0, y0, x1, y1, r) && x0 == 0 && x1 == 100 && y0 == 25);
    x0 = -10; y0 = -10; x1 = 110; y1 = 110;
    assert(cohenSutherland(x0, y0, x1, y1, r) && std::fabs(x0) < 1e-9 && std::fabs(y1 - 50) < 1e-9);

    // Sutherland-Hodgman: rectangle sticking out to the right -> only the inside part remains
    auto clipped = sutherlandHodgman({{50, 10}, {150, 10}, {150, 40}, {50, 40}}, r);
    double area = 0;
    for (size_t i = 0; i < clipped.size(); ++i) {
        auto& a = clipped[i];
        auto& b = clipped[(i + 1) % clipped.size()];
        area += a.x * b.y - b.x * a.y;
    }
    assert(std::fabs(std::fabs(area) / 2 - 50 * 30) < 1e-9);
    assert(sutherlandHodgman({{200, 200}, {300, 200}, {250, 300}}, r).empty());
    assert(sutherlandHodgman({{1, 1}, {9, 1}, {5, 9}}, r).size() == 3);

    // Wu: coverage weights in (0,1]
    std::vector<APixel> w;
    wuLine(0.0, 0.0, 20.0, 7.3, w);
    assert(!w.empty());
    for (auto& p : w) assert(p.a > 0 && p.a <= 1.0001f);
    std::printf("test_raster OK\n");
    return 0;
}
