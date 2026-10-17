// 2D ground-track minimap (equirectangular). lat/lon -> pixels through a window-to-viewport transform;
// track segments split at the dateline and clipped with Cohen-Sutherland; station coverage footprints
// clipped with Sutherland-Hodgman and filled with the scan-line algorithm.
#include <algorithm>
#include <cstdio>
#include "hud/Hud.h"
#include "raster/Clipping.h"
#include "render/GL.h"

namespace og {

void mapWindow(const MapView& m, double& lon0, double& lat0, double& lon1, double& lat1) {
    double hw = 180.0 / m.zoom, hh = 90.0 / m.zoom;
    double cl = std::clamp(m.centerLon, -180.0 + hw, 180.0 - hw);
    double ct = std::clamp(m.centerLat, -90.0 + hh, 90.0 - hh);
    lon0 = cl - hw; lon1 = cl + hw; lat0 = ct - hh; lat1 = ct + hh;
}

namespace {
struct MapCtx {
    Canvas& cv;
    Mat3 M;
    ClipRect clip;
    void seg(double lon0, double lat0, double lon1, double lat1, RGB c, float a) {
        Vec2 p = M.apply({lon0, lat0}), q = M.apply({lon1, lat1});
        if (cohenSutherland(p.x, p.y, q.x, q.y, clip)) cv.line(p.x, p.y, q.x, q.y, c, a);
    }
    // polyline in lat/lon; splits at the +/-180 meridian
    void track(const std::vector<Vec2>& ll, RGB c, float a) {
        for (size_t k = 1; k < ll.size(); ++k) {
            Vec2 u = ll[k - 1], v = ll[k];
            if (std::fabs(v.x - u.x) <= 180.0) { seg(u.x, u.y, v.x, v.y, c, a); continue; }
            double vx = v.x + (v.x < u.x ? 360.0 : -360.0);  // unwrap
            double edge = u.x > 0 ? 180.0 : -180.0;
            double t = (edge - u.x) / (vx - u.x);
            double latc = u.y + t * (v.y - u.y);
            seg(u.x, u.y, edge, latc, c, a);
            seg(-edge, latc, v.x, v.y, c, a);
        }
    }
};

std::vector<Vec2> groundTrack(const Population& p, int i, double jd0, double jd1, int n) {
    std::vector<Vec2> ll;
    for (int k = 0; k <= n; ++k) {
        double jd = jd0 + (jd1 - jd0) * k / n;
        Vec3 r, v;
        propagateOne(p, i, jd, r, v);
        double lat, lon, alt;
        ecefToLatLon(eciToEcef(r, jd), lat, lon, alt);
        ll.push_back({lon, lat});
    }
    return ll;
}

// Coverage footprint of a station for satellites at `altKm`, as a lat/lon polygon.
std::vector<Vec2> footprint(const Station& st, double altKm, bool& containsPole) {
    double eps = st.maskDeg * DEG, lam = std::acos(RE / (RE + altKm) * std::cos(eps)) - eps;
    Vec3 up = normalize(latLonToEcef(st.latDeg, st.lonDeg, 0));
    Vec3 east = normalize(cross({0, 0, 1}, up)), north = cross(up, east);
    std::vector<Vec2> ll;
    for (int k = 0; k < 72; ++k) {
        double phi = TWO_PI * k / 72;
        Vec3 q = up * std::cos(lam) + (east * std::cos(phi) + north * std::sin(phi)) * std::sin(lam);
        double lat, lon, alt;
        ecefToLatLon(q * RE, lat, lon, alt);
        ll.push_back({lon, lat});
    }
    containsPole = std::fabs(st.latDeg) + lam / DEG > 90.0;
    if (containsPole) {  // close the cap along the map's top/bottom edge
        double pole = st.latDeg > 0 ? 90.0 : -90.0;
        std::sort(ll.begin(), ll.end(), [](const Vec2& a, const Vec2& b) { return a.x < b.x; });
        ll.push_back({180.0, ll.back().y});
        ll.push_back({180.0, pole});
        ll.push_back({-180.0, pole});
        ll.insert(ll.begin(), {-180.0, ll.front().y});
    } else {  // unwrap longitudes around the station so the ring is continuous
        for (auto& v : ll) {
            while (v.x - st.lonDeg > 180) v.x -= 360;
            while (v.x - st.lonDeg < -180) v.x += 360;
        }
    }
    return ll;
}
}  // namespace

void drawGroundTrackMap(Canvas& cv, const AppState& s, unsigned tex, int x, int y, int w, int h) {
    const auto& p = s.sim.pop;
    const auto& c = s.sim.cfg;
    double lon0, lat0, lon1, lat1;
    mapWindow(s.map, lon0, lat0, lon1, lat1);
    WindowToViewport w2v{lon0, lat0, lon1, lat1, double(x), double(y), double(x + w), double(y + h)};
    MapCtx m{cv, w2v.matrix(), {double(x), double(y), double(x + w - 1), double(y + h - 1)}};

    // background map (texture sub-window matching the lat/lon window)
    cv.fillRect(x - 3, y - 3, x + w + 3, y + h + 18, {0.02, 0.05, 0.08}, 0.9f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
    float u0 = float((lon0 + 180) / 360), u1 = float((lon1 + 180) / 360);
    float v0 = float((lat0 + 90) / 180), v1 = float((lat1 + 90) / 180);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2i(x, y);
    glTexCoord2f(u1, v0); glVertex2i(x + w, y);
    glTexCoord2f(u1, v1); glVertex2i(x + w, y + h);
    glTexCoord2f(u0, v1); glVertex2i(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // graticule
    for (int lon = -180; lon <= 180; lon += 30) m.seg(lon, -90, lon, 90, {0.4, 0.6, 0.8}, 0.25f);
    for (int lat = -60; lat <= 60; lat += 30) m.seg(-180, lat, 180, lat, {0.4, 0.6, 0.8}, lat == 0 ? 0.45f : 0.25f);

    // station coverage footprints: shift copies by +/-360, clip (Sutherland-Hodgman), scan-line fill
    if (s.showStations) {
        for (auto& st : c.stations) {
            bool pole;
            auto ll = footprint(st, c.stationConeAltKm, pole);
            for (int off : {-360, 0, 360}) {
                if (pole && off) continue;
                std::vector<Vec2> px;
                for (auto& v : ll) px.push_back(m.M.apply({v.x + off, v.y}));
                auto clipped = sutherlandHodgman(px, m.clip);
                if (clipped.size() < 3) continue;
                cv.fillPolygon(clipped, {0.3, 1.0, 0.5}, 0.18f);
                for (size_t k = 0; k < clipped.size(); ++k) {
                    const Vec2 &a = clipped[k], &b = clipped[(k + 1) % clipped.size()];
                    cv.line(a.x, a.y, b.x, b.y, {0.3, 1.0, 0.5}, 0.6f);
                }
            }
            Vec2 sp = m.M.apply({st.lonDeg, st.latDeg});
            if (sp.x >= x && sp.x < x + w && sp.y >= y && sp.y < y + h) {
                cv.fillRect(sp.x - 2, sp.y - 2, sp.x + 3, sp.y + 3, {1, 1, 1}, 1.0f);
                cv.text(int(sp.x) + 5, int(sp.y) - 4, st.name.substr(0, st.name.find(' ')), {0.8, 1.0, 0.85});
            }
        }
    }

    double jd = s.sim.nowJD;
    // alert objects' sub-satellite points
    for (auto& a : s.sim.alerts) {
        if (a.passed || int(a.tier) < s.alertFilter) continue;
        for (int i : {a.a, a.b}) {
            if (!p.alive[i]) continue;
            double lat, lon, alt;
            ecefToLatLon(eciToEcef(p.pos[i], jd), lat, lon, alt);
            Vec2 q = m.M.apply({lon, lat});
            if (q.x >= x && q.x < x + w && q.y >= y && q.y < y + h)
                cv.circle(int(q.x), int(q.y), 2, riskColor(a.missKm, c.yellowKm));
        }
    }

    // selected object's ground track: past half-orbit dim, next 1.5 orbits bright
    if (s.selected >= 0 && p.alive[s.selected]) {
        double P = periodS(p.el[s.selected].a) / SEC_PER_DAY;
        RGB sc = rgbOf(c.colSelected);
        m.track(groundTrack(p, s.selected, jd - 0.5 * P, jd, 60), sc, 0.4f);
        m.track(groundTrack(p, s.selected, jd, jd + 1.5 * P, 180), sc, 0.95f);
        if (s.planShown && s.plan.valid && s.plan.obj == s.selected) {
            Population tmp;
            tmp.add("plan", 0, Kind::Payload, s.plan.after);
            m.track(groundTrack(tmp, 0, jd, jd + 1.5 * P, 180), {0.2, 0.9, 1.0}, 0.9f);
        }
        double lat, lon, alt;
        ecefToLatLon(eciToEcef(p.pos[s.selected], jd), lat, lon, alt);
        Vec2 q = m.M.apply({lon, lat});
        if (q.x >= x && q.x < x + w && q.y >= y && q.y < y + h) {
            cv.circle(int(q.x), int(q.y), 5, sc);
            cv.line(q.x - 9, q.y, q.x + 9, q.y, sc);
            cv.line(q.x, q.y - 9, q.x, q.y + 9, sc);
        }
    }

    // frame + title
    RGB fr{0.4, 0.7, 1.0};
    cv.line(x - 1, y - 1, x + w, y - 1, fr);
    cv.line(x + w, y - 1, x + w, y + h, fr);
    cv.line(x + w, y + h, x - 1, y + h, fr);
    cv.line(x - 1, y + h, x - 1, y - 1, fr);
    char t[96];
    std::snprintf(t, sizeof t, "GROUND TRACK  zoom %.0fx  (wheel / drag)", s.map.zoom);
    cv.text(x + 2, y + h + 5, t, fr);
}

}  // namespace og
