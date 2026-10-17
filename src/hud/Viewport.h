// Window-to-viewport transformation + a pixel canvas on which the HUD is scan-converted by our own
// raster algorithms (midpoint / Wu lines, midpoint circles, scan-line fills). OpenGL only receives pixels.
#pragma once
#include <string>
#include <vector>
#include "color/ColorModels.h"
#include "math/Transform2D.h"
#include "raster/Midpoint.h"

namespace og {

struct WindowToViewport {
    double wx0, wy0, wx1, wy1;  // world window
    double vx0, vy0, vx1, vy1;  // viewport, pixels
    // M = T(v0) * S((v1-v0)/(w1-w0)) * T(-w0): composed 2D homogeneous transforms
    Mat3 matrix() const;
    Vec2 map(Vec2 w) const { return matrix().apply(w); }
};

class Canvas {
public:
    bool aa = true;  // Wu lines (true) or midpoint lines (false)
    ColorMode mode = ColorMode::RGB;

    void begin(int W, int H);
    void end();
    RGB col(RGB c) const;
    void plot(const std::vector<Pixel>& px, RGB c, float a = 1.0f);
    void line(double x0, double y0, double x1, double y1, RGB c, float a = 1.0f);
    void circle(int cx, int cy, int r, RGB c, float a = 1.0f);
    void fillPolygon(const std::vector<Vec2>& poly, RGB c, float a);
    void fillRect(double x0, double y0, double x1, double y1, RGB c, float a);
    void text(int x, int y, const std::string& s, RGB c, bool big = false);
    static int textWidth(const std::string& s, bool big = false);

private:
    std::vector<Pixel> scratch_;
};

}  // namespace og
