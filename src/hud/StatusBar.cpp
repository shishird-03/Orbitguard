// Bottom status bar: performance, population, alerts, sim clock, screening coverage, view state.
#include <cstdio>
#include "hud/Hud.h"

namespace og {

void drawStatusBar(Canvas& cv, const AppState& s, int W) {
    cv.fillRect(0, 0, W, 22, {0.03, 0.06, 0.1}, 0.92f);
    cv.line(0, 22, W, 22, {0.3, 0.55, 0.9});
    int r = 0, a = 0, y = 0;
    for (auto& al : s.sim.alerts) {
        if (al.passed) continue;
        if (al.tier == Tier::Red) ++r; else if (al.tier == Tier::Amber) ++a; else if (al.tier == Tier::Yellow) ++y;
    }
    double ahead = (s.sim.screener.horizonJD() - s.sim.nowJD) * SEC_PER_DAY;
    double cover = std::min(100.0, 100.0 * ahead / s.sim.cfg.lookaheadS);
    char buf[512];
    std::snprintf(buf, sizeof buf,
                  "FPS %.0f  |  objects %d  |  alerts R%d A%d Y%d  |  %s UTC  |  warp %.0fx%s  |  screening +%02d:%02d (%.0f%%) %.1f ms  |  %s %s  |  %s",
                  s.fps, s.sim.pop.aliveCount(), r, a, y, jdToUTC(s.sim.nowJD).c_str(), s.clock.warp,
                  s.clock.paused ? " PAUSED" : "", int(ahead) / 60, int(ahead) % 60, cover, s.screenMs,
                  s.touring ? "TOUR" : camModeName(s.camera.mode), s.camera.ortho ? "ORTHO" : "PERSP", labSummary(s.lab).c_str());
    cv.text(8, 6, buf, {0.75, 0.88, 1.0});
}

}  // namespace og
