#include "render/Starfield.h"
#include <random>
#include "render/GL.h"

namespace og {

void Starfield::build(int count, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> N(0, 1);
    std::uniform_real_distribution<double> U(0, 1);
    xyz_.clear(); rgb_.clear();
    for (int i = 0; i < count; ++i) {
        Vec3 d = normalize({N(rng), N(rng), N(rng)}) * 50.0;
        double b = 0.25 + 0.75 * std::pow(U(rng), 3.0);  // few bright, many faint
        double tint = U(rng);
        RGB c = hsvToRgb({tint < 0.5 ? 220.0 : 40.0, 0.15 * U(rng), b});
        xyz_.insert(xyz_.end(), {float(d.x), float(d.y), float(d.z)});
        rgb_.insert(rgb_.end(), {float(c.r), float(c.g), float(c.b)});
    }
}

void Starfield::draw(const Mat4& view, ColorMode mode) const {
    Mat4 rot = view;
    rot.at(0, 3) = rot.at(1, 3) = rot.at(2, 3) = 0.0;  // drop translation: stars live at infinity
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(rot.m);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    std::vector<float> col(rgb_.size());
    for (size_t i = 0; i < rgb_.size(); i += 3) {
        RGB c{rgb_[i], rgb_[i + 1], rgb_[i + 2]};
        c = mode == ColorMode::CMYPrint ? RGB{0.1, 0.1, 0.15} : applyColorMode(mode, c);  // ink specks on paper
        col[i] = float(c.r); col[i + 1] = float(c.g); col[i + 2] = float(c.b);
    }
    glPointSize(1.5f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, xyz_.data());
    glColorPointer(3, GL_FLOAT, 0, col.data());
    glDrawArrays(GL_POINTS, 0, GLsizei(xyz_.size() / 3));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
}

}  // namespace og
