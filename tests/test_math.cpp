// Own Mat4 / Transform2D vs GLM reference, tolerance 1e-6.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "math/Mat4.h"
#include "math/Transform2D.h"
#include "math/Transform3D.h"

using namespace og;

static void same(const Mat4& a, const glm::dmat4& g, const char* what) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::fabs(a.at(r, c) - g[c][r]) > 1e-6) {
                std::printf("FAIL %s at (%d,%d): %f vs %f\n", what, r, c, a.at(r, c), g[c][r]);
                assert(false);
            }
}

int main() {
    const double R = 0.7, I = 1.1, W = -2.3;
    glm::dmat4 g = glm::rotate(glm::dmat4(1), R, {0, 0, 1}) * glm::rotate(glm::dmat4(1), I, {1, 0, 0}) *
                   glm::rotate(glm::dmat4(1), W, {0, 0, 1});
    same(perifocalToECI(R, I, W), g, "Rz*Rx*Rz chain");
    same(rotateY(0.4), glm::rotate(glm::dmat4(1), 0.4, {0, 1, 0}), "rotateY");
    same(rotateAxis({1, 2, 3}, 0.9), glm::rotate(glm::dmat4(1), 0.9, glm::normalize(glm::dvec3(1, 2, 3))), "rotateAxis");
    same(translate({1, -2, 3}) * scale({2, 3, 4}),
         glm::scale(glm::translate(glm::dmat4(1), {1, -2, 3}), {2, 3, 4}), "translate*scale");
    same(perspective(0.8, 1.6, 0.01, 100), glm::perspective(0.8, 1.6, 0.01, 100.0), "perspective");
    same(ortho(-2, 3, -1, 4, 0.1, 50), glm::ortho(-2.0, 3.0, -1.0, 4.0, 0.1, 50.0), "ortho");
    same(lookAt({3, 4, 5}, {0, 0, 0}, {0, 0, 1}),
         glm::lookAt(glm::dvec3(3, 4, 5), glm::dvec3(0), glm::dvec3(0, 0, 1)), "lookAt");
    Mat4 m = perspective(0.8, 1.6, 0.01, 100) * lookAt({3, 4, 5}, {0, 0, 0}, {0, 0, 1}) * rotateX(0.3);
    glm::dmat4 gm;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) gm[c][r] = m.at(r, c);
    same(inverse(m), glm::inverse(gm), "inverse");
    same(m * inverse(m), glm::dmat4(1), "m * inverse(m)");

    // 2D: rotate 90 deg then translate maps (1,0) -> (5,6)
    Vec2 p = (translate2D(5, 5) * rotate2D(PI / 2) * scale2D(1, 1)).apply({1, 0});
    assert(std::fabs(p.x - 5) < 1e-12 && std::fabs(p.y - 6) < 1e-12);
    std::printf("test_math OK\n");
    return 0;
}
