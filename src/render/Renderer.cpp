#include "render/Renderer.h"
#include <algorithm>
#include <cstdio>
#include "geom/Curves.h"
#include "geom/Fractal.h"
#include "math/Transform3D.h"
#include "render/GL.h"
#include "render/Lighting.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace og {

static unsigned loadTexture(const std::string& path, ColorMode mode) {
    int w = 0, h = 0, n = 0;
    stbi_set_flip_vertically_on_load(1);  // row 0 = south pole = v 0
    unsigned char* px = stbi_load(resolvePath(path).c_str(), &w, &h, &n, 3);
    std::vector<unsigned char> fallback;
    if (!px) {
        std::fprintf(stderr, "texture %s missing, using flat ocean blue\n", path.c_str());
        w = h = 2;
        fallback = {20, 40, 90, 20, 40, 90, 20, 40, 90, 20, 40, 90};
    }
    unsigned char* data = px ? px : fallback.data();
    if (mode != ColorMode::RGB) {
        int total = w * h;
#pragma omp parallel for
        for (int i = 0; i < total; ++i) {
            unsigned char* p = data + i * 3;
            RGB c = applyColorMode(mode, {p[0] / 255.0, p[1] / 255.0, p[2] / 255.0});
            p[0] = (unsigned char)std::clamp(c.r * 255.0 + 0.5, 0.0, 255.0);
            p[1] = (unsigned char)std::clamp(c.g * 255.0 + 0.5, 0.0, 255.0);
            p[2] = (unsigned char)std::clamp(c.b * 255.0 + 0.5, 0.0, 255.0);
        }
    }
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
    if (px) stbi_image_free(px);
    return t;
}

// Bakes a mesh into a display list; per-vertex colors (if any) are passed through the color model.
static unsigned compileMesh(const Mesh& m, ColorMode mode) {
    unsigned list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (unsigned idx : m.tri) {
        if (!m.col.empty()) glColor(applyColorMode(mode, {m.col[idx * 3], m.col[idx * 3 + 1], m.col[idx * 3 + 2]}));
        glNormal3d(m.nrm[idx].x, m.nrm[idx].y, m.nrm[idx].z);
        glVertex(m.pos[idx]);
    }
    glEnd();
    glEndList();
    return list;
}

bool Renderer::init(const Config& c) {
    cfg_ = &c;
    earth_ = uvSphere(c.earthStacks, c.earthSlices);
    earthWire_ = uvSphere(18, 36);
    dish_ = disc(16, 0.25);
    Mesh sat = satelliteModel();
    for (int m = 0; m < 4; ++m) {
        dayTex_[m] = loadTexture("assets/textures/earth_day.jpg", ColorMode(m));
        nightTex_[m] = loadTexture("assets/textures/earth_night.jpg", ColorMode(m));
        satList_[m] = compileMesh(sat, ColorMode(m));
        rockList_[m] = compileMesh(fractalRock(3, 0.55, 1000 + m), ColorMode::RGB);
    }
    dishList_ = compileMesh(dish_, ColorMode::RGB);
    stars_.build(c.starCount, 4242);
    return true;
}

RGB Renderer::clearColor(const AppState& s) const {
    if (s.lab.color == ColorMode::CMYPrint) return {0.97, 0.96, 0.92};  // paper
    return applyColorMode(s.lab.color, rgbOf(s.sim.cfg.colBackground));
}

void Renderer::applyLabState(const AppState& s) {
    if (s.lab.aa) {
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_POINT_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    } else {
        glDisable(GL_MULTISAMPLE);
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POINT_SMOOTH);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(s.lab.flat ? GL_FLAT : GL_SMOOTH);
}

void Renderer::computeObjectColors(const AppState& s) {
    const auto& p = s.sim.pop;
    const auto& c = s.sim.cfg;
    size_t N = p.size();
    objColor_.resize(N);
    objRisk_.assign(N, -1.0f);
    for (auto& a : s.sim.alerts) {
        if (a.passed || int(a.tier) < s.alertFilter) continue;
        for (int i : {a.a, a.b})
            if (objRisk_[i] < 0 || a.missKm < objRisk_[i]) objRisk_[i] = float(a.missKm);
    }
    ColorMode mode = s.lab.color;
    for (size_t i = 0; i < N; ++i) {
        if (!p.alive[i]) continue;
        RGB base;
        if (mode == ColorMode::HSVHeat) {
            base = altitudeHeat(length(p.pos[i]) - RE);
        } else {
            const double* k = p.kind[i] == Kind::Payload ? c.colPayload
                            : p.kind[i] == Kind::Debris ? c.colDebris
                            : p.kind[i] == Kind::RocketBody ? c.colRocket : c.colFragment;
            base = applyColorMode(mode, rgbOf(k));
            if (mode == ColorMode::CMYPrint) base = {base.r * 0.5, base.g * 0.5, base.b * 0.5};  // ink on paper
        }
        if (objRisk_[i] >= 0) {
            RGB r = riskColor(objRisk_[i], c.yellowKm);
            base = (mode == ColorMode::RGB || mode == ColorMode::HSVHeat) ? r : applyColorMode(mode, r);
        }
        objColor_[i] = base;
    }
}

void Renderer::drawEarth(const AppState& s) {
    glPushMatrix();
    glMultMatrixd(earthModel_.m);
    int mode = int(s.lab.color);
    if (s.lab.wireframe) {
        // hidden-line removal: depth-only-ish fill in background color, pushed back, then edges
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_DOUBLE, sizeof(Vec3), earthWire_.pos.data());
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glColor(clearColor(s));
        glDrawElements(GL_TRIANGLES, GLsizei(earthWire_.tri.size()), GL_UNSIGNED_INT, earthWire_.tri.data());
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor(s.lab.color == ColorMode::CMYPrint ? RGB{0.1, 0.2, 0.4} : applyColorMode(s.lab.color, {0.25, 0.7, 1.0}), 0.9f);
        glDrawElements(GL_TRIANGLES, GLsizei(earthWire_.tri.size()), GL_UNSIGNED_INT, earthWire_.tri.data());
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        return;
    }
    // day pass: lit + textured
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glDisable(GL_COLOR_MATERIAL);
    setMaterial(1.0f, 1.0f, 0.25f, 24.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, dayTex_[mode]);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_DOUBLE, sizeof(Vec3), earth_.pos.data());
    glNormalPointer(GL_DOUBLE, sizeof(Vec3), earth_.nrm.data());
    glTexCoordPointer(2, GL_FLOAT, 0, earth_.uv.data());
    glDrawElements(GL_TRIANGLES, GLsizei(earth_.tri.size()), GL_UNSIGNED_INT, earth_.tri.data());
    // night pass: city lights added where the surface faces away from the sun
    Vec3 sunModel = rotateZ(-gmst(s.sim.nowJD)).transformDir(sunEci_);
    nightWeights(earth_, sunModel, nightRGBA_);
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, nightTex_[mode]);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, nightRGBA_.data());
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDrawElements(GL_TRIANGLES, GLsizei(earth_.tri.size()), GL_UNSIGNED_INT, earth_.tri.data());
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

void Renderer::drawPoints(const AppState& s, const std::vector<int>& subset, bool useSubset) {
    const auto& p = s.sim.pop;
    ptXYZ_.clear();
    ptRGB_.clear();
    auto push = [&](int i) {
        const Vec3& r = s.renderPos[i];
        ptXYZ_.insert(ptXYZ_.end(), {float(r.x / RE), float(r.y / RE), float(r.z / RE)});
        const RGB& c = objColor_[i];
        ptRGB_.insert(ptRGB_.end(), {float(c.r), float(c.g), float(c.b)});
    };
    if (useSubset) for (int i : subset) push(i);
    else for (int i = 0; i < int(p.size()); ++i) if (p.alive[i]) push(i);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(float(s.sim.cfg.pointSize));
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, ptXYZ_.data());
    glColorPointer(3, GL_FLOAT, 0, ptRGB_.data());
    glDrawArrays(GL_POINTS, 0, GLsizei(ptXYZ_.size() / 3));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    // emphasize alert objects and the selection
    glPointSize(float(s.sim.cfg.pointSize) * 2.2f);
    glBegin(GL_POINTS);
    bool selectedHere = false;
    auto emphasize = [&](int i) {
        if (i == s.selected) selectedHere = true;
        if (!p.alive[i] || objRisk_[i] < 0) return;
        glColor(objColor_[i]);
        glVertex(s.renderPos[i] / RE);
    };
    if (useSubset) for (int i : subset) emphasize(i);
    else for (int i = 0; i < int(p.size()); ++i) emphasize(i);
    glEnd();
    if (selectedHere && p.alive[s.selected]) {
        glPointSize(9.0f);
        glBegin(GL_POINTS);
        glColor(applyColorMode(s.lab.color, rgbOf(s.sim.cfg.colSelected)));
        glVertex(s.renderPos[s.selected] / RE);
        glEnd();
    }
}

void Renderer::drawSatMesh(const AppState& s, int i, const Vec3& eye) {
    const auto& p = s.sim.pop;
    Vec3 pos = s.renderPos[i] / RE;
    double size = s.sim.cfg.meshScale * length(pos - eye);
    glPushMatrix();
    glMultMatrixd(velocityFrame(pos, p.vel[i], size).m);
    if (p.kind[i] == Kind::Payload) {
        glCallList(satList_[int(s.lab.color)]);
    } else {
        // tumbling fractal rock
        glMultMatrixd(rotateAxis({std::sin(i * 1.7), std::cos(i * 0.9), 0.5}, s.realTime * (0.5 + (i % 7) * 0.2)).m);
        glColor(objColor_[i]);
        glCallList(rockList_[i % 4]);
    }
    glPopMatrix();
}

void Renderer::drawLodMeshes(const AppState& s, const Vec3& eye, std::vector<int>* listOut) {
    const auto& p = s.sim.pop;
    const double lod = s.sim.cfg.meshLodDist;
    std::vector<std::pair<double, int>> nearby;
    for (int i = 0; i < int(p.size()); ++i) {
        if (!p.alive[i]) continue;
        Vec3 d = s.renderPos[i] / RE - eye;
        if (std::fabs(d.x) > lod || std::fabs(d.y) > lod || std::fabs(d.z) > lod) continue;
        double l = length(d);
        if (l < lod) nearby.emplace_back(l, i);
    }
    size_t k = std::min(nearby.size(), size_t(s.sim.cfg.meshLodMax));
    std::partial_sort(nearby.begin(), nearby.begin() + k, nearby.end());
    nearby.resize(k);
    if (listOut) {
        for (auto& n : nearby) listOut->push_back(n.second);
        return;
    }
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    setMaterial(0.3f, 1.0f, 0.5f, 40.0f);
    // models are tiny next to the Earth; a little extra ambient keeps their night side readable
    GLfloat amb[4] = {0.35f, 0.35f, 0.4f, 1}, base[4] = {0.03f, 0.03f, 0.04f, 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    if (s.lab.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (auto& n : nearby) drawSatMesh(s, n.second, eye);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, base);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
}

void Renderer::drawOrbit(const Elements& e0, const RGB& c, float alpha, bool stipple) const {
    int n = cfg_->orbitSamples;
    if (stipple) {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(3, 0x5555);
    }
    glColor(c, alpha);
    glBegin(GL_LINE_LOOP);
    Elements e = e0;
    for (int k = 0; k < n; ++k) {
        e.M = TWO_PI * k / n;
        Vec3 r, v;
        coe2rv(e, r, v);
        glVertex(r / RE);
    }
    glEnd();
    if (stipple) glDisable(GL_LINE_STIPPLE);
}

void Renderer::drawStations(const AppState& s, bool cones) {
    const auto& c = s.sim.cfg;
    glPushMatrix();
    glMultMatrixd(earthModel_.m);
    if (!cones) {
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        for (auto& st : c.stations) {
            Vec3 up = normalize(latLonToEcef(st.latDeg, st.lonDeg, 0));
            Vec3 east = normalize(cross({0, 0, 1}, up)), north = cross(up, east);
            glPushMatrix();
            glMultMatrixd((fromBasis(east, north, up, up * 1.002) * scale({0.02, 0.02, 0.02})).m);
            glColor(applyColorMode(s.lab.color, {0.95, 0.95, 0.95}));
            glCallList(dishList_);
            glPopMatrix();
        }
        glDisable(GL_COLOR_MATERIAL);
        glDisable(GL_LIGHTING);
        glPopMatrix();
        return;
    }
    // translucent line-of-sight cones (apex at the station, base where elevation == mask at cone altitude),
    // sorted back to front (list priority) because blending is order-dependent
    Mat4 mv;
    glGetDoublev(GL_MODELVIEW_MATRIX, mv.m);
    std::vector<std::pair<double, const Station*>> order;
    for (auto& st : c.stations) order.push_back({mv.transformPoint(latLonToEcef(st.latDeg, st.lonDeg, 0) / RE).z, &st});
    std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first < b.first; });
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);
    RGB col = applyColorMode(s.lab.color, {0.3, 1.0, 0.55});
    for (auto& [z, st] : order) {
        Vec3 up = normalize(latLonToEcef(st->latDeg, st->lonDeg, 0));
        Vec3 east = normalize(cross({0, 0, 1}, up)), north = cross(up, east);
        double eps = st->maskDeg * DEG, rr = (RE + c.stationConeAltKm) / RE;
        double lam = std::acos(std::cos(eps) / rr) - eps;
        std::vector<Vec3> ring;
        for (int k = 0; k <= 48; ++k) {
            double phi = TWO_PI * k / 48;
            ring.push_back((up * std::cos(lam) + (east * std::cos(phi) + north * std::sin(phi)) * std::sin(lam)) * rr);
        }
        glColor(col, 0.10f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex(up * 1.001);
        for (auto& q : ring) glVertex(q);
        glEnd();
        glColor(col, 0.55f);
        glBegin(GL_LINE_STRIP);
        for (auto& q : ring) glVertex(q);
        glEnd();
    }
    glDepthMask(GL_TRUE);
    glPopMatrix();
}

void Renderer::drawOverlays(const AppState& s) {
    const auto& p = s.sim.pop;
    const auto& c = s.sim.cfg;
    const double jd = s.sim.nowJD;
    ColorMode mode = s.lab.color;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    if (s.lab.painter) glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glLineWidth(1.5f);

    if (s.showOrbits) {
        int shown = 0;
        for (auto& a : s.sim.alerts) {
            if (a.passed || int(a.tier) < std::max(s.alertFilter, 2) || shown >= 12) continue;
            ++shown;
            RGB rc = riskColor(a.missKm, c.yellowKm);
            if (mode != ColorMode::RGB) rc = applyColorMode(mode, rc);
            for (int i : {a.a, a.b})
                if (p.alive[i]) drawOrbit(elementsAt(p, i, jd), rc, 0.35f, false);
        }
        if (s.selected >= 0 && p.alive[s.selected] && !(s.planShown && s.plan.obj == s.selected))
            drawOrbit(elementsAt(p, s.selected, jd), applyColorMode(mode, rgbOf(c.colSelected)), 0.8f, false);
    }

    // fading history trail
    if (s.trail.size() > 1) {
        glBegin(GL_LINE_STRIP);
        RGB tc = applyColorMode(mode, rgbOf(c.colSelected));
        for (size_t k = 0; k < s.trail.size(); ++k) {
            glColor(tc, float(k) / s.trail.size());
            glVertex(s.trail[k] / RE);
        }
        glEnd();
    }

    // maneuver: old orbit dashed (stippled), new orbit solid, Bezier transfer arc
    if (s.planShown && s.plan.valid) {
        drawOrbit(s.plan.before, applyColorMode(mode, {1.0, 0.7, 0.2}), 0.9f, true);
        drawOrbit(s.plan.after, applyColorMode(mode, {0.2, 0.9, 1.0}), 0.95f, false);
        Vec3 r0, v0, r1, v1;
        coe2rv(s.plan.before, r0, v0);
        Elements q = s.plan.after;
        q.M = wrapTwoPi(q.M + PI / 2);  // a quarter orbit downstream on the new orbit
        coe2rv(q, r1, v1);
        double L = length(r1 - r0) / 3.0;
        auto arc = bezierPolyline(r0 / RE, (r0 + normalize(v0) * L) / RE, (r1 - normalize(v1) * L) / RE, r1 / RE, 48);
        glLineWidth(3.0f);
        glColor(applyColorMode(mode, {1.0, 0.3, 1.0}), 1.0f);
        glBegin(GL_LINE_STRIP);
        for (auto& v : arc) glVertex(v);
        glEnd();
        glLineWidth(1.5f);
        glPointSize(10.0f);
        glBegin(GL_POINTS);
        glVertex(r0 / RE);
        glEnd();
    }

    // threat lines between conjunction pairs, HSV-graded by miss distance
    if (s.showThreatLines) {
        glBegin(GL_LINES);
        int drawn = 0;
        for (auto& a : s.sim.alerts) {
            if (a.passed || int(a.tier) < s.alertFilter || !p.alive[a.a] || !p.alive[a.b] || drawn++ > 64) continue;
            RGB rc = riskColor(a.missKm, c.yellowKm);
            if (mode != ColorMode::RGB) rc = applyColorMode(mode, rc);
            glColor(rc, 0.75f);
            glVertex(s.renderPos[a.a] / RE);
            glVertex(s.renderPos[a.b] / RE);
        }
        glEnd();
    }

    // predicted TCA point of the focused conjunction
    if (int fi = s.focusIndex(); fi >= 0) {
        const auto& a = s.sim.alerts[fi];
        if (p.alive[a.a]) {
            Vec3 r, v;
            propagateOne(p, a.a, a.tcaJD, r, v);
            double pulse = 8.0 + 4.0 * std::sin(s.realTime * 6.0);
            glPointSize(float(pulse));
            glColor(applyColorMode(mode, {1, 0.1, 0.1}), 0.9f);
            glBegin(GL_POINTS);
            glVertex(r / RE);
            glEnd();
        }
    }

    // ground-station links to the selected object when it is above the elevation mask
    if (s.showStations && s.selected >= 0 && p.alive[s.selected]) {
        Vec3 satEcef = eciToEcef(p.pos[s.selected], jd);
        glBegin(GL_LINES);
        for (auto& st : c.stations) {
            Vec3 se = latLonToEcef(st.latDeg, st.lonDeg, st.altKm);
            if (elevationDeg(se, satEcef) < st.maskDeg) continue;
            glColor(applyColorMode(mode, {0.3, 1.0, 0.4}), 0.9f);
            glVertex(ecefToEci(se, jd) / RE);
            glVertex(s.renderPos[s.selected] / RE);
        }
        glEnd();
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

void Renderer::drawPainter(const AppState& s, const Mat4& view, const Vec3& eye) {
    // List-priority algorithm: no depth buffer; sort everything by eye-space depth, draw far to nearby.
    struct Item { double z; int type; int idx; };  // type 0 earth, 1 point, 2 mesh
    std::vector<Item> items;
    const auto& p = s.sim.pop;
    items.push_back({view.transformPoint({0, 0, 0}).z, 0, -1});
    for (int i = 0; i < int(p.size()); ++i)
        if (p.alive[i]) items.push_back({view.transformPoint(s.renderPos[i] / RE).z, 1, i});
    std::vector<int> lod;
    drawLodMeshes(s, eye, &lod);
    for (int i : lod) items.push_back({view.transformPoint(s.renderPos[i] / RE).z + 1e-9, 2, i});
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.z < b.z; });

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);  // no z-buffer: the sphere's back half must not overdraw its front half
    std::vector<int> run;
    auto flush = [&]() {
        if (!run.empty()) drawPoints(s, run, true);
        run.clear();
    };
    for (auto& it : items) {
        if (it.type == 1) { run.push_back(it.idx); continue; }
        flush();
        if (it.type == 0) {
            drawEarth(s);
            if (s.showStations) drawStations(s, false);
        } else {
            glEnable(GL_LIGHTING);
            glEnable(GL_NORMALIZE);
            glEnable(GL_COLOR_MATERIAL);
            glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            drawSatMesh(s, it.idx, eye);
            glDisable(GL_COLOR_MATERIAL);
            glDisable(GL_LIGHTING);
        }
    }
    flush();
    glDisable(GL_CULL_FACE);
}

void Renderer::drawScene(const AppState& s, const Mat4& view, const Mat4& proj) {
    cfg_ = &s.sim.cfg;
    earthModel_ = rotateZ(gmst(s.sim.nowJD));
    sunEci_ = sunDirectionECI(s.sim.nowJD);
    computeObjectColors(s);
    applyLabState(s);
    stars_.draw(view, s.lab.color);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(proj.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(view.m);
    setupSunLight(sunEci_, s.lab.flat);
    Vec3 eye = inverse(view).transformPoint({0, 0, 0});

    if (s.lab.painter) {
        drawPainter(s, view, eye);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        drawEarth(s);
        if (s.showStations) drawStations(s, false);
        drawPoints(s, {}, false);
        drawLodMeshes(s, eye, nullptr);
    }
    drawOverlays(s);
    if (s.showStations) {
        if (s.lab.painter) glDisable(GL_DEPTH_TEST);
        drawStations(s, true);
        glEnable(GL_DEPTH_TEST);
    }
}

void Renderer::drawIdBuffer(const AppState& s, const Mat4& view, const Mat4& proj) {
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(proj.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(view.m);
    // Earth as a black occluder so objects behind it can't be picked
    glColor3ub(0, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_DOUBLE, sizeof(Vec3), earth_.pos.data());
    glDrawElements(GL_TRIANGLES, GLsizei(earth_.tri.size()), GL_UNSIGNED_INT, earth_.tri.data());
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    const auto& p = s.sim.pop;
    for (int i = 0; i < int(p.size()); ++i) {
        if (!p.alive[i]) continue;
        unsigned id = unsigned(i) + 1;
        glColor3ub(GLubyte(id & 255), GLubyte((id >> 8) & 255), GLubyte((id >> 16) & 255));
        glVertex(s.renderPos[i] / RE);
    }
    glEnd();
    glEnable(GL_DITHER);
}

}  // namespace og
