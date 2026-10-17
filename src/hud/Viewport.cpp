#include "hud/Viewport.h"
#include "raster/Antialias.h"
#include "raster/ScanlineFill.h"
#include "render/GL.h"

namespace og {

Mat3 WindowToViewport::matrix() const {
    double sx = (vx1 - vx0) / (wx1 - wx0), sy = (vy1 - vy0) / (wy1 - wy0);
    return translate2D(vx0, vy0) * scale2D(sx, sy) * translate2D(-wx0, -wy0);
}

void Canvas::begin(int W, int H) {
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(ortho(0, W, 0, H, -1, 1).m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_MULTISAMPLE);  // the HUD is our own rasterization: one GL point = one pixel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(1.0f);
    glLineWidth(1.0f);
}

void Canvas::end() { glEnable(GL_DEPTH_TEST); }

RGB Canvas::col(RGB c) const { return mode == ColorMode::YIQNight ? applyColorMode(mode, c) : c; }

void Canvas::plot(const std::vector<Pixel>& px, RGB c, float a) {
    glColor(col(c), a);
    glBegin(GL_POINTS);
    for (auto& p : px) glVertex2f(p.x + 0.5f, p.y + 0.5f);
    glEnd();
}

void Canvas::line(double x0, double y0, double x1, double y1, RGB c, float a) {
    RGB k = col(c);
    if (aa) {
        static std::vector<APixel> ap;
        ap.clear();
        wuLine(x0, y0, x1, y1, ap);
        glBegin(GL_POINTS);
        for (auto& p : ap) {
            glColor(k, a * p.a);
            glVertex2f(p.x + 0.5f, p.y + 0.5f);
        }
        glEnd();
    } else {
        scratch_.clear();
        midpointLine(int(std::lround(x0)), int(std::lround(y0)), int(std::lround(x1)), int(std::lround(y1)), scratch_);
        plot(scratch_, c, a);
    }
}

void Canvas::circle(int cx, int cy, int r, RGB c, float a) {
    scratch_.clear();
    midpointCircle(cx, cy, r, scratch_);
    plot(scratch_, c, a);
}

void Canvas::fillPolygon(const std::vector<Vec2>& poly, RGB c, float a) {
    static std::vector<Span> spans;
    spans.clear();
    scanlineFill(poly, spans);
    glColor(col(c), a);
    glBegin(GL_LINES);  // each span is one run of pixels on a scanline
    for (auto& s : spans) {
        glVertex2f(float(s.x0), s.y + 0.5f);
        glVertex2f(float(s.x1 + 1), s.y + 0.5f);
    }
    glEnd();
}

void Canvas::fillRect(double x0, double y0, double x1, double y1, RGB c, float a) {
    fillPolygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, c, a);
}

void Canvas::text(int x, int y, const std::string& s, RGB c, bool big) {
    glColor(col(c), 1.0f);
    glRasterPos2i(x, y);
    glutBitmapString(big ? GLUT_BITMAP_HELVETICA_18 : GLUT_BITMAP_HELVETICA_12, (const unsigned char*)s.c_str());
}

int Canvas::textWidth(const std::string& s, bool big) {
    return glutBitmapLength(big ? GLUT_BITMAP_HELVETICA_18 : GLUT_BITMAP_HELVETICA_12, (const unsigned char*)s.c_str());
}

}  // namespace og
