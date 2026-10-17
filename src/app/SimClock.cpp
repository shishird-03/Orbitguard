#include "app/SimClock.h"
#include <algorithm>
#include <cmath>

namespace og {

int SimClock::advance(double realDt) {
    if (paused) {
        acc_ = 0.0;
        int n = pendingSteps;
        pendingSteps = 0;
        return n;
    }
    acc_ += std::min(realDt, 0.25);
    int n = int(acc_ / tickDt);
    if (n > maxTicksPerFrame) {  // can't keep up: drop time rather than spiral
        n = maxTicksPerFrame;
        acc_ = 0.0;
    } else {
        acc_ -= n * tickDt;
    }
    return n;
}

// 1, 2, 5, 10, 20, 50 ... steps
void SimClock::warpUp() {
    double e = std::pow(10.0, std::floor(std::log10(warp) + 1e-9)), m = warp / e;
    warp = std::min(warpMax, (m < 1.5 ? 2 : m < 3.5 ? 5 : 10) * e);
}

void SimClock::warpDown() {
    double e = std::pow(10.0, std::floor(std::log10(warp) + 1e-9)), m = warp / e;
    warp = std::max(1.0, (m > 7 ? 5 : m > 3 ? 2 : m > 1.5 ? 1 : 0.5) * e);
}

}  // namespace og
