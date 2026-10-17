#include "raster/Clipping.h"

namespace og {

enum { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

int outcode(double x, double y, const ClipRect& r) {
    int c = INSIDE;
    if (x < r.xmin) c |= LEFT; else if (x > r.xmax) c |= RIGHT;
    if (y < r.ymin) c |= BOTTOM; else if (y > r.ymax) c |= TOP;
    return c;
}

bool cohenSutherland(double& x0, double& y0, double& x1, double& y1, const ClipRect& r) {
    int c0 = outcode(x0, y0, r), c1 = outcode(x1, y1, r);
    while (true) {
        if (!(c0 | c1)) return true;   // trivially accept
        if (c0 & c1) return false;     // trivially reject: both share an outside half-plane
        int c = c0 ? c0 : c1;
        double x, y;
        if (c & TOP)         { x = x0 + (x1 - x0) * (r.ymax - y0) / (y1 - y0); y = r.ymax; }
        else if (c & BOTTOM) { x = x0 + (x1 - x0) * (r.ymin - y0) / (y1 - y0); y = r.ymin; }
        else if (c & RIGHT)  { y = y0 + (y1 - y0) * (r.xmax - x0) / (x1 - x0); x = r.xmax; }
        else                 { y = y0 + (y1 - y0) * (r.xmin - x0) / (x1 - x0); x = r.xmin; }
        if (c == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0, r); }
        else         { x1 = x; y1 = y; c1 = outcode(x1, y1, r); }
    }
}

// Clip against each of the four edges in turn; each pass emits the polygon for the next.
std::vector<Vec2> sutherlandHodgman(const std::vector<Vec2>& poly, const ClipRect& r) {
    std::vector<Vec2> out = poly;
    for (int edge = 0; edge < 4 && !out.empty(); ++edge) {
        auto inside = [&](const Vec2& p) {
            switch (edge) {
                case 0: return p.x >= r.xmin;
                case 1: return p.x <= r.xmax;
                case 2: return p.y >= r.ymin;
                default: return p.y <= r.ymax;
            }
        };
        auto intersect = [&](const Vec2& a, const Vec2& b) {
            double t;
            switch (edge) {
                case 0: t = (r.xmin - a.x) / (b.x - a.x); break;
                case 1: t = (r.xmax - a.x) / (b.x - a.x); break;
                case 2: t = (r.ymin - a.y) / (b.y - a.y); break;
                default: t = (r.ymax - a.y) / (b.y - a.y); break;
            }
            return Vec2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        };
        std::vector<Vec2> in = std::move(out);
        out.clear();
        for (size_t i = 0; i < in.size(); ++i) {
            const Vec2& cur = in[i];
            const Vec2& prev = in[(i + in.size() - 1) % in.size()];
            bool ci = inside(cur), pi = inside(prev);
            if (ci) {
                if (!pi) out.push_back(intersect(prev, cur));
                out.push_back(cur);
            } else if (pi) {
                out.push_back(intersect(prev, cur));
            }
        }
    }
    return out;
}

}  // namespace og
