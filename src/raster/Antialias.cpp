#include "raster/Antialias.h"
#include <cmath>
#include <utility>

namespace og {

static double fpart(double x) { return x - std::floor(x); }
static double rfpart(double x) { return 1.0 - fpart(x); }

void wuLine(double x0, double y0, double x1, double y1, std::vector<APixel>& out) {
    bool steep = std::fabs(y1 - y0) > std::fabs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    double dx = x1 - x0, dy = y1 - y0;
    double grad = dx == 0.0 ? 1.0 : dy / dx;
    auto plot = [&](int x, int y, double a) {
        if (a <= 0.0) return;
        if (steep) out.push_back({y, x, float(a)});
        else out.push_back({x, y, float(a)});
    };
    // first endpoint
    double xend = std::round(x0), yend = y0 + grad * (xend - x0), xgap = rfpart(x0 + 0.5);
    int xp1 = int(xend), yp1 = int(std::floor(yend));
    plot(xp1, yp1, rfpart(yend) * xgap);
    plot(xp1, yp1 + 1, fpart(yend) * xgap);
    double intery = yend + grad;
    // second endpoint
    xend = std::round(x1); yend = y1 + grad * (xend - x1); xgap = fpart(x1 + 0.5);
    int xp2 = int(xend), yp2 = int(std::floor(yend));
    plot(xp2, yp2, rfpart(yend) * xgap);
    plot(xp2, yp2 + 1, fpart(yend) * xgap);
    for (int x = xp1 + 1; x < xp2; ++x) {
        plot(x, int(std::floor(intery)), rfpart(intery));
        plot(x, int(std::floor(intery)) + 1, fpart(intery));
        intery += grad;
    }
}

}  // namespace og
