// Alert feed (timestamped event log) and the flashing conjunction-warning banner.
// Panels and tier chips are filled with the scan-line algorithm.
#include <cmath>
#include <cstdio>
#include "hud/Hud.h"

namespace og {

static RGB tierColor(Tier t) {
    switch (t) {
        case Tier::Red: return {1.0, 0.2, 0.15};
        case Tier::Amber: return {1.0, 0.65, 0.1};
        case Tier::Yellow: return {1.0, 0.95, 0.3};
        default: return {0.3, 1.0, 0.4};
    }
}

void drawAlertFeed(Canvas& cv, const AppState& s, int x, int y, int w) {
    const int rows = 7, rowH = 16;
    int h = rows * rowH + 24;
    cv.fillRect(x, y, x + w, y + h, {0.02, 0.04, 0.07}, 0.8f);
    cv.line(x, y + h, x + w, y + h, {0.4, 0.7, 1.0});
    cv.text(x + 6, y + h - 15, "ALERT FEED", {0.6, 0.85, 1.0});
    const auto& ev = s.sim.events;
    int row = 0;
    for (int k = int(ev.size()) - 1; k >= 0 && row < rows; --k, ++row) {
        int ry = y + h - 24 - (row + 1) * rowH + 4;
        RGB tc = tierColor(ev[k].tier);
        cv.fillRect(x + 6, ry, x + 14, ry + 9, tc, 1.0f);
        std::string line = "[" + jdToUTC(ev[k].jd).substr(11) + "] " + ev[k].text;
        while (line.size() > 4 && Canvas::textWidth(line) > w - 26) line.pop_back();
        cv.text(x + 20, ry, line, row == 0 ? RGB{1, 1, 1} : RGB{0.75, 0.8, 0.85});
    }
    if (ev.empty()) cv.text(x + 20, y + h - 40, "no events yet", {0.5, 0.55, 0.6});
}

void drawWarningBanner(Canvas& cv, const AppState& s, int W, int H) {
    int red = 0;
    double nextTca = 1e30;
    for (auto& a : s.sim.alerts)
        if (!a.passed && a.tier == Tier::Red) { ++red; nextTca = std::min(nextTca, a.tcaJD); }
    if (!red) return;
    float pulse = float(0.55 + 0.35 * std::sin(s.realTime * 5.0));
    int bw = 560, bx = (W - bw) / 2, by = H - 46;
    cv.fillPolygon({{double(bx), double(by)}, {double(bx + bw), double(by)}, {double(bx + bw - 14), double(by + 34)},
                    {double(bx + 14), double(by + 34)}},
                   {0.75, 0.05, 0.05}, pulse);
    int secs = int((nextTca - s.sim.nowJD) * SEC_PER_DAY);
    char t[160];
    std::snprintf(t, sizeof t, "CONJUNCTION WARNING  %d RED   next TCA T-%02d:%02d   [A] plan avoidance", red,
                  std::max(0, secs) / 60, std::max(0, secs) % 60);
    cv.text(bx + (bw - Canvas::textWidth(t, true)) / 2, by + 11, t, {1, 1, 1}, true);
}

}  // namespace og
