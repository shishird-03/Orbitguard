// RGB <-> HSV / YIQ / CMY round trips and anchor values.
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
#include "color/ColorModels.h"

using namespace og;

static bool near(RGB a, RGB b) {
    return std::fabs(a.r - b.r) < 1e-6 && std::fabs(a.g - b.g) < 1e-6 && std::fabs(a.b - b.b) < 1e-6;
}

int main() {
    for (int ri = 0; ri <= 10; ++ri)
        for (int gi = 0; gi <= 10; ++gi)
            for (int bi = 0; bi <= 10; ++bi) {
                RGB c{ri / 10.0, gi / 10.0, bi / 10.0};
                assert(near(hsvToRgb(rgbToHsv(c)), c));
                assert(near(yiqToRgb(rgbToYiq(c)), c));
                assert(near(cmyToRgb(rgbToCmy(c)), c));
            }
    HSV red = rgbToHsv({1, 0, 0});
    assert(red.h == 0 && red.s == 1 && red.v == 1);
    assert(std::fabs(rgbToHsv({0, 0, 1}).h - 240) < 1e-9);
    YIQ w = rgbToYiq({1, 1, 1});
    assert(std::fabs(w.y - 1) < 1e-9 && std::fabs(w.i) < 1e-9 && std::fabs(w.q) < 1e-9);
    CMY k = rgbToCmy({0, 1, 0});
    assert(k.c == 1 && k.m == 0 && k.y == 1);
    assert(near(riskColor(0, 25), {1, 0, 0}) && near(riskColor(25, 25), {0, 1, 0}));
    std::printf("test_color OK\n");
    return 0;
}
