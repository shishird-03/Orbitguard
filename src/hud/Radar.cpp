// Radar scope centred on the selected object, looking down its local horizontal plane.
// Range rings: midpoint circles. Sweep: a rotated line (2D homogeneous rotate + translate).
// Blips: km offsets mapped to pixels by a window-to-viewport transform.
#include <cmath>
#include <cstdio>
#include "hud/Hud.h"

namespace og {

void drawRadar(Canvas& cv, const AppState& s, int cx, int cy, int R) {
    const auto& p = s.sim.pop;
    const auto& c = s.sim.cfg;
    const RGB green{0.25, 1.0, 0.45}, dim{0.1, 0.45, 0.2};

    std::vector<Vec2> disc;
    for (int k = 0; k < 64; ++k) disc.push_back({cx + (R + 6) * std::cos(TWO_PI * k / 64), cy + (R + 6) * std::sin(TWO_PI * k / 64)});
    cv.fillPolygon(disc, {0.0, 0.08, 0.04}, 0.82f);
    for (int k = 1; k <= 4; ++k) cv.circle(cx, cy, R * k / 4, k == 4 ? green : dim, k == 4 ? 1.0f : 0.8f);
    cv.line(cx - R, cy, cx + R, cy, dim, 0.6f);
    cv.line(cx, cy - R, cx, cy + R, dim, 0.6f);

    // sweep with afterglow
    double theta = std::fmod(s.realTime * PI, TWO_PI);  // one turn every 2 s
    for (int k = 12; k >= 0; --k) {
        Vec2 tip = (translate2D(cx, cy) * rotate2D(theta - k * 0.045)).apply({double(R), 0});
        cv.line(cx, cy, tip.x, tip.y, green, k == 0 ? 1.0f : 0.35f * (1.0f - k / 13.0f));
    }

    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "RADAR  +/-%.0f km", c.radarRangeKm);
    cv.text(cx - R, cy + R + 12, hdr, green);
    if (s.selected < 0 || !p.alive[s.selected]) {
        cv.text(cx - 30, cy - R - 18, "NO TRACK", dim);
        return;
    }
    cv.text(cx - R, cy - R - 18, p.name[s.selected].substr(0, 30), green);

    // local frame: up = radial, forward = along-track, right completes it
    Vec3 r0 = p.pos[s.selected], v0 = p.vel[s.selected];
    Vec3 up = normalize(r0);
    Vec3 fwd = normalize(v0 - up * dot(v0, up));
    Vec3 right = cross(fwd, up);
    WindowToViewport w2v{-c.radarRangeKm, -c.radarRangeKm, c.radarRangeKm, c.radarRangeKm,
                         double(cx - R), double(cy - R), double(cx + R), double(cy + R)};
    Mat3 M = w2v.matrix();

    std::vector<int> threat(p.size(), 0);
    for (auto& a : s.sim.alerts)
        if (!a.passed && (a.a == s.selected || a.b == s.selected)) threat[a.a == s.selected ? a.b : a.a] = 1;

    int contacts = 0;
    const double R2 = c.radarRangeKm * c.radarRangeKm;
    for (int i = 0; i < int(p.size()); ++i) {
        if (!p.alive[i] || i == s.selected) continue;
        Vec3 d = p.pos[i] - r0;
        if (std::fabs(d.x) > c.radarRangeKm || std::fabs(d.y) > c.radarRangeKm || std::fabs(d.z) > c.radarRangeKm) continue;
        if (dot(d, d) > R2) continue;
        ++contacts;
        double x = dot(d, right), y = dot(d, fwd);
        Vec2 px = M.apply({x, y});
        double phi = std::atan2(y, x);
        double age = std::fmod(theta - phi + 2 * TWO_PI, TWO_PI) / TWO_PI;  // 0 just swept, ~1 about to be
        float a = float(1.0 - 0.85 * age);
        RGB bc = threat[i] ? RGB{1.0, 0.25, 0.2} : green;
        int rad = threat[i] ? 3 : 2;
        cv.circle(int(px.x), int(px.y), rad, bc, a);
        cv.plot({{int(px.x), int(px.y)}}, bc, a);
        if (threat[i]) cv.text(int(px.x) + 5, int(px.y) + 4, p.name[i].substr(0, 18), bc);
    }
    cv.circle(cx, cy, 3, {1, 1, 1});
    char ct[48];
    std::snprintf(ct, sizeof ct, "%d contacts", contacts);
    cv.text(cx + R - Canvas::textWidth(ct), cy + R + 12, ct, green);
}

}  // namespace og
