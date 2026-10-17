#include "app/App.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_glut.h>
#include <imgui_impl_opengl2.h>
#include "app/Input.h"
#include "geom/Curves.h"
#include "hud/Hud.h"
#include "io/ReportExporter.h"
#include "render/GL.h"
#include "render/Picking.h"
#include "render/Renderer.h"
#include "ui/Panels.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace og {

namespace {
AppState g;
Renderer renderer;
Config baseCfg;
double lastT = -1;
bool keysApplied = false;

// bench accumulators
struct Bench { int frames = 0; double frame = 0, sim = 0, prop = 0, screen = 0, render = 0, slice = 0; } bench;

double nowSeconds() { return glutGet(GLUT_ELAPSED_TIME) / 1000.0; }
}  // namespace

AppState& app() { return g; }

std::vector<std::string> listScenarios() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(resolvePath("scenarios"), ec))
        if (e.path().extension() == ".json") out.push_back(e.path().string());
    std::sort(out.begin(), out.end());
    return out;
}

static void resetViewState(AppState& s) {
    s.prevPos = s.sim.pop.pos;
    s.renderPos = s.sim.pop.pos;
    s.trail.clear();
    s.plan = BurnPlan();
    s.planShown = false;
    s.focusA = s.focusB = -1;
    s.selected = -1;
    s.map = MapView();
    s.touring = false;
    s.camera.overrideOn = false;
    s.clock.tickDt = 1.0 / s.sim.cfg.tickHz;
    s.clock.warpMax = s.sim.cfg.warpMax;
    s.clock.maxTicksPerFrame = s.sim.cfg.maxTicksPerFrame;
}

void actLoadScenario(AppState& s, const std::string& path) {
    s.sim = Simulation();
    s.sim.cfg = baseCfg;
    s.scenario = ScenarioInfo();
    s.scenarioPath = path;
    if (path.empty()) {
        double epoch = loadCatalog(s.sim.pop, s.sim.cfg.catalog, s.sim.cfg.maxObjects);
        if (s.sim.pop.size() == 0) {
            std::fprintf(stderr, "catalog %s not found or empty\n", s.sim.cfg.catalog.c_str());
            epoch = jdFromCalendar(2026, 10, 15, 12, 0, 0);
        }
        s.sim.start(epoch);
        s.scenario.name = "Full catalog snapshot";
    } else {
        std::string err;
        if (!loadScenario(path, s.sim, s.scenario, err)) {
            std::fprintf(stderr, "scenario %s: %s\n", path.c_str(), err.c_str());
            s.notify("Scenario failed: " + err, 6);
            actLoadScenario(s, "");
            return;
        }
    }
    resetViewState(s);
    s.clock.warp = s.scenario.warp > 0 ? s.scenario.warp : s.sim.cfg.warpInit;
    s.camera = Camera();
    int follow = s.sim.pop.findByName(s.scenario.follow.empty() ? "ISS (ZARYA)" : s.scenario.follow);
    if (follow >= 0) actSelect(s, follow);
    if (follow >= 0 && !s.scenario.follow.empty()) {
        s.camera.mode = CamMode::Follow;
        s.camera.dist = 0.12;
    }
    std::printf("loaded %s: %zu objects, epoch %s\n", s.scenario.name.c_str(), s.sim.pop.size(), jdToUTC(s.sim.nowJD).c_str());
    s.notify("Loaded: " + s.scenario.name + " (" + std::to_string(s.sim.pop.size()) + " objects)");
}

void actSelect(AppState& s, int i) {
    if (i < 0 || i >= int(s.sim.pop.size())) return;
    s.selected = i;
    s.trail.clear();
}

void actFocusAlert(AppState& s, int k) {
    if (k < 0 || k >= int(s.sim.alerts.size())) return;
    const auto& a = s.sim.alerts[k];
    s.focusA = a.a;
    s.focusB = a.b;
    s.focusTca = a.tcaJD;
    actSelect(s, s.sim.pop.kind[a.a] == Kind::Payload ? a.a : a.b);
}

void actPlan(AppState& s) {
    int k = s.focusIndex();
    if (k < 0 && s.selected >= 0) {  // worst conjunction of the selected object
        double best = 1e30;
        for (int j = 0; j < int(s.sim.alerts.size()); ++j) {
            const auto& a = s.sim.alerts[j];
            if (!a.passed && (a.a == s.selected || a.b == s.selected) && a.missKm < best) { best = a.missKm; k = j; }
        }
    }
    if (k < 0) {  // otherwise the worst conjunction overall
        double best = 1e30;
        for (int j = 0; j < int(s.sim.alerts.size()); ++j)
            if (!s.sim.alerts[j].passed && s.sim.alerts[j].missKm < best) { best = s.sim.alerts[j].missKm; k = j; }
    }
    if (k < 0) { s.notify("No conjunction to plan for"); return; }
    actFocusAlert(s, k);
    auto t0 = std::chrono::steady_clock::now();
    s.plan = planAvoidance(s.sim.pop, s.sim.alerts[k], s.sim.nowJD, s.sim.cfg, true);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    s.planShown = true;
    if (s.plan.obj >= 0) actSelect(s, s.plan.obj);
    std::printf("plan (%.0f ms): %s\n", ms, s.plan.note.c_str());
    s.notify(s.plan.note);
}

void actCommit(AppState& s) {
    if (!s.planShown || !s.plan.valid) return;
    if (!s.sim.pop.alive[s.plan.obj]) { s.notify("Object no longer exists"); return; }
    // the plan was computed at plan.burnJD; re-anchor the burn if time moved on since
    if (std::fabs(s.sim.nowJD - s.plan.burnJD) * SEC_PER_DAY > 1.0) {
        int k = s.focusIndex();
        if (k < 0) { s.notify("Conjunction no longer tracked"); return; }
        s.plan = planAvoidance(s.sim.pop, s.sim.alerts[k], s.sim.nowJD, s.sim.cfg, true);
        if (!s.plan.valid) { s.notify(s.plan.note); return; }
    }
    commitBurn(s.sim.pop, s.plan);
    s.sim.afterBurn(s.plan.obj);
    char buf[200];
    std::snprintf(buf, sizeof buf, "BURN: %s %+.3f m/s, miss now %.2f km", s.sim.pop.name[s.plan.obj].c_str(), s.plan.dvMps,
                  s.plan.newMissKm);
    s.sim.log(Tier::Green, buf);
    s.notify(buf);
    s.prevPos = s.sim.pop.pos;
    s.planShown = false;
    s.focusA = s.focusB = -1;
    s.trail.clear();
}

void actDiscardPlan(AppState& s) { s.planShown = false; }

void actKessler(AppState& s) {
    if (s.selected < 0 || !s.sim.pop.alive[s.selected]) { s.notify("Select a target first"); return; }
    s.sim.cfg.kesslerEnabled = true;  // the fragments should be able to cascade
    int n = s.sim.asat(s.selected);
    s.prevPos = s.sim.pop.pos;
    s.renderPos = s.sim.pop.pos;
    s.notify("ASAT test: " + std::to_string(n) + " fragments; Kessler mode ON");
    s.selected = -1;
    if (s.camera.mode == CamMode::Follow) s.camera.mode = CamMode::Arcball, s.camera.target = {0, 0, 0}, s.camera.dist = 3.0;
}

void actExport(AppState& s) {
    std::string path = exportConjunctionReport(s.sim, resolvePath("reports"));
    s.notify(path.empty() ? "Export failed" : "Report: " + path, 5);
    std::printf("export: %s\n", path.c_str());
}

void actScreenshot(AppState& s) {
    s.shotPending = true;
    if (s.shotPath.empty()) {
        std::string ts = jdToUTC(s.sim.nowJD);
        for (char& c : ts) if (c == ' ' || c == ':') c = '-';
        s.shotPath = resolvePath("reports") + "/screenshot_" + ts + ".png";
    }
}

void actJump(AppState& s, double jd) {
    s.sim.jumpTo(jd);
    s.prevPos = s.sim.pop.pos;
    s.renderPos = s.sim.pop.pos;
    s.trail.clear();
    s.planShown = false;
}

void actQuit(AppState& s) { s.quit = true; }

void actTour(AppState& s) {
    const auto& p = s.sim.pop;
    int sat = s.selected >= 0 && p.alive[s.selected] ? s.selected : p.findByName("ISS (ZARYA)");
    if (sat < 0) for (int i = 0; i < int(p.size()); ++i) if (p.alive[i]) { sat = i; break; }
    if (sat < 0) return;
    int conj = -1;
    double best = 1e30;
    for (int k = 0; k < int(s.sim.alerts.size()); ++k) {
        const auto& a = s.sim.alerts[k];
        if (!a.passed && a.tier >= Tier::Amber && a.missKm < best) { best = a.missKm; conj = k; }
    }
    int cobj = sat;
    if (conj >= 0) {
        actFocusAlert(s, conj);
        cobj = s.selected;
    }
    auto frame = [&](int i, double out, double side) {
        Vec3 n = normalize(s.renderPos[i]), sd = normalize(cross(n, {0, 0, 1}));
        return n * out + sd * side;
    };
    s.tour.clear();
    s.tour.push_back({-1, s.camera.eye(), s.camera.center()});
    s.tour.push_back({-1, {0, -5.5, 2.2}, {0, 0, 0}});
    s.tour.push_back({sat, frame(sat, 0.6, 0.4), {0, 0, 0}});
    s.tour.push_back({sat, frame(sat, 0.07, 0.05), {0, 0, 0}});
    s.tour.push_back({cobj, frame(cobj, 0.25, -0.2), {0, 0, 0}});
    s.tour.push_back({cobj, frame(cobj, 0.12, 0.12), {0, 0, 0}});
    s.tour.push_back({-1, {3.4, 3.2, 1.8}, {0, 0, 0}});
    s.touring = true;
    s.tourPlanned = conj < 0;
    s.tourT = 0;
    s.tourWarpBefore = s.clock.warp;
    s.clock.warp = 2;
    s.clock.paused = false;
    s.notify("Cinematic tour (Esc to stop)");
}

static void updateTour(AppState& s, double dt) {
    if (!s.touring) return;
    s.tourT += dt / s.sim.cfg.tourDurationS;
    if (s.tourT >= 1.0) {
        s.touring = false;
        s.camera.overrideOn = false;
        s.clock.warp = s.tourWarpBefore;
        return;
    }
    std::vector<Vec3> eyes, centers;
    for (auto& k : s.tour) {
        Vec3 base = (k.anchor >= 0 && s.sim.pop.alive[k.anchor]) ? s.renderPos[k.anchor] / RE : Vec3{0, 0, 0};
        eyes.push_back(base + k.eyeOff);
        centers.push_back(k.anchor >= 0 ? base + k.centerOff : k.centerOff);
    }
    s.camera.overrideOn = true;
    s.camera.ovEye = bsplineEval(eyes, s.tourT);
    s.camera.ovCenter = bsplineEval(centers, s.tourT);
    if (!s.tourPlanned && s.tourT > 0.66) {  // the "burn" beat of the tour
        s.tourPlanned = true;
        actPlan(s);
    }
}

static void saveScreenshot(AppState& s) {
    int W = s.winW, H = s.winH;
    std::vector<unsigned char> px(size_t(W) * H * 3), flip(px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < H; ++y) std::copy_n(&px[size_t(H - 1 - y) * W * 3], W * 3, &flip[size_t(y) * W * 3]);
    std::filesystem::create_directories(std::filesystem::path(s.shotPath).parent_path());
    bool ok = stbi_write_png(s.shotPath.c_str(), W, H, 3, flip.data(), W * 3) != 0;
    std::printf("screenshot %s: %s\n", ok ? "saved" : "FAILED", s.shotPath.c_str());
    s.notify(ok ? "Saved " + s.shotPath : "Screenshot failed");
    s.shotPending = false;
    if (s.opt.shot.empty()) s.shotPath.clear();
}

// ---------------------------------------------------------------- frame

static void stepSimulation(AppState& s, double dt) {
    auto t0 = std::chrono::steady_clock::now();
    int ticks = s.clock.advance(dt);
    double screenMs = 0;
    for (int k = 0; k < ticks; ++k) {
        s.prevPos = s.sim.pop.pos;
        s.sim.step(s.clock.simDtPerTick(), k == ticks - 1 ? s.sim.cfg.budgetMs : 0.0);
        screenMs += s.sim.screener.lastRunMs;
        if (s.selected >= 0 && s.sim.pop.alive[s.selected]) {
            s.trail.push_back(s.sim.pop.pos[s.selected]);
            while (int(s.trail.size()) > s.sim.cfg.trailLength) s.trail.pop_front();
        }
    }
    if (ticks == 0) {  // keep filling the look-ahead window between ticks / while paused
        s.sim.screen(s.sim.cfg.budgetMs);
        screenMs += s.sim.screener.lastRunMs;
    }
    s.screenMs = 0.9 * s.screenMs + 0.1 * screenMs;
    s.simMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    const auto& pos = s.sim.pop.pos;
    if (s.prevPos.size() != pos.size()) s.prevPos = pos;
    s.renderPos.resize(pos.size());
    double a = s.clock.paused ? 1.0 : std::clamp(s.clock.alpha(), 0.0, 1.0);
    for (size_t i = 0; i < pos.size(); ++i) s.renderPos[i] = lerp(s.prevPos[i], pos[i], a);
    if (s.selected >= 0 && !s.sim.pop.alive[s.selected]) s.selected = -1;
    if (s.camera.mode == CamMode::Follow && s.selected < 0) {  // followed object destroyed
        s.camera.mode = CamMode::Arcball;
        s.camera.target = {0, 0, 0};
        s.camera.dist = std::max(s.camera.dist, 3.0);
    }
    if (s.camera.mode == CamMode::Follow && s.selected >= 0) s.camera.target = s.renderPos[s.selected] / RE;
}

static void drawQuadLabels(Canvas& cv, int W, int H) {
    RGB c{0.6, 0.85, 1.0};
    cv.text(8, H - 16, "PERSPECTIVE", c);
    cv.text(W / 2 + 8, H - 16, "TOP (ORTHO, +Z)", c);
    cv.text(8, H / 2 - 16, "SIDE (ORTHO, +X)", c);
    cv.line(W / 2, 0, W / 2, H, c, 0.6f);
    cv.line(0, H / 2, W, H / 2, c, 0.6f);
}

static void renderFrame(AppState& s) {
    auto t0 = std::chrono::steady_clock::now();
    int W = s.winW, H = s.winH;
    RGB bg = renderer.clearColor(s);
    ViewRect mainVp = s.quadView ? ViewRect{0, H / 2, W / 2, H - H / 2} : ViewRect{0, 0, W, H};
    Mat4 view = s.camera.view(), proj = s.camera.proj(double(mainVp.w) / std::max(1, mainVp.h));

    if (s.pickPending) {
        s.pickPending = false;
        int gy = H - 1 - s.pickY;
        if (s.pickX >= mainVp.x && s.pickX < mainVp.x + mainVp.w && gy >= mainVp.y && gy < mainVp.y + mainVp.h) {
            int id = pickObject(renderer, s, view, proj, mainVp, s.pickX, s.pickY);
            if (id >= 0) {
                actSelect(s, id);
                s.focusA = s.focusB = -1;
            }
        }
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, W, H);
    glClearColor(float(bg.r), float(bg.g), float(bg.b), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(mainVp.x, mainVp.y, mainVp.w, mainVp.h);
    renderer.drawScene(s, view, proj);
    if (s.quadView) {  // CAD-style: same scene through three projections + the ground track
        double hh = std::clamp(s.camera.dist * 0.45, 1.3, 8.0), asp = double(W / 2) / std::max(1, H / 2);
        Mat4 op = ortho(-hh * asp, hh * asp, -hh, hh, 0.1, 40.0);
        glViewport(W / 2, H / 2, W - W / 2, H - H / 2);
        renderer.drawScene(s, lookAt({0, 0, 20}, {0, 0, 0}, {0, 1, 0}), op);
        glViewport(0, 0, W / 2, H / 2);
        renderer.drawScene(s, lookAt({20, 0, 0}, {0, 0, 0}, {0, 0, 1}), op);
    }

    Canvas cv;
    cv.aa = s.lab.aa;
    cv.mode = s.lab.color;
    cv.begin(W, H);
    if (s.quadView) {
        drawQuadLabels(cv, W, H);
        drawGroundTrackMap(cv, s, renderer.dayTexture(s.lab.color), W / 2 + 20, 40, W - W / 2 - 40, H / 2 - 80);
        drawWarningBanner(cv, s, W, H);
        drawStatusBar(cv, s, W);
    } else if (s.showHud) {
        int mw = s.sim.cfg.mapW, mh = s.sim.cfg.mapH;
        s.mapRect[0] = W - mw - 14; s.mapRect[1] = H - mh - 26; s.mapRect[2] = mw; s.mapRect[3] = mh;
        drawGroundTrackMap(cv, s, renderer.dayTexture(s.lab.color), s.mapRect[0], s.mapRect[1], mw, mh);
        int R = s.sim.cfg.radarPx / 2;
        drawRadar(cv, s, W - R - 26, 22 + R + 34, R);
        drawAlertFeed(cv, s, 10, 30, 480);
        drawWarningBanner(cv, s, W, H);
        drawStatusBar(cv, s, W);
    }
    cv.end();
    s.renderMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

static void runPickSelfTest(AppState& s) {
    s.renderPos = s.sim.pop.pos;
    ViewRect vp{0, 0, s.winW, s.winH};
    Mat4 view = s.camera.view(), proj = s.camera.proj(double(vp.w) / vp.h);
    auto r = runPickTest(renderer, s, view, proj, vp);
    double pct = r.tested ? 100.0 * r.hits / r.tested : 0.0;
    std::printf("PICK TEST: %d / %d isolated visible objects picked correctly (%.2f%%)\n", r.hits, r.tested, pct);
    s.exitCode = (r.tested > 0 && r.hits == r.tested) ? 0 : 1;
    s.quit = true;
}

static void finishBench(AppState& s) {
    int n = std::max(1, bench.frames);
    char buf[640];
    std::snprintf(buf, sizeof buf,
                  "BENCH %s\n  objects %d, window %dx%d, warp %.0fx, %d frames\n"
                  "  avg frame %.2f ms (%.1f FPS)\n  sim per frame %.2f ms (propagation %.2f ms/tick)\n"
                  "  screening per frame %.2f ms, per slice %.2f ms (cell %.0f km, %d candidate pairs last slice)\n"
                  "  render+HUD %.2f ms\n",
                  s.scenario.name.c_str(), s.sim.pop.aliveCount(), s.winW, s.winH, s.clock.warp, n, bench.frame / n,
                  1000.0 / (bench.frame / n), bench.sim / n, bench.prop / n, bench.screen / n, bench.slice / n,
                  s.sim.screener.cellKm(s.sim.cfg), s.sim.screener.candidatePairs, bench.render / n);
    std::printf("%s", buf);
    if (!s.opt.bench.empty()) {
        if (FILE* f = std::fopen(s.opt.bench.c_str(), "a")) { std::fputs(buf, f); std::fclose(f); }
    }
    s.quit = true;
}

static void applyStartupKeys(AppState& s) {
    keysApplied = true;
    if (!s.opt.select.empty()) {
        int i = s.sim.pop.findByName(s.opt.select);
        if (i >= 0) actSelect(s, i);
        else std::fprintf(stderr, "--select: no object named %s\n", s.opt.select.c_str());
    }
    if (s.opt.warp > 0) s.clock.warp = s.opt.warp;
    for (auto& t : {"painter", "flat", "noaa", "wire", "hsv", "yiq", "cmy"})
        if (s.opt.lab.find(t) != std::string::npos) {
            std::string k = t;
            if (k == "painter") s.lab.painter = true;
            else if (k == "flat") s.lab.flat = true;
            else if (k == "noaa") s.lab.aa = false;
            else if (k == "wire") s.lab.wireframe = true;
            else s.lab.color = k == "hsv" ? ColorMode::HSVHeat : k == "yiq" ? ColorMode::YIQNight : ColorMode::CMYPrint;
        }
    if (s.opt.lab.find("panel") != std::string::npos) s.showLab = true;
    if (!s.opt.keys.empty()) s.sim.screen(-1);  // scripted keys see the complete look-ahead window
    for (char k : s.opt.keys) onKey(s, k == '!' ? '\r' : (unsigned char)k);
    if (s.opt.camDist > 0) s.camera.dist = s.opt.camDist;
    if (s.opt.camYaw < 1e8) s.camera.yaw = s.opt.camYaw;
    if (s.opt.camPitch < 1e8) s.camera.pitch = s.opt.camPitch;
}

static void display() {
    AppState& s = g;
    double t = nowSeconds();
    double dt = lastT < 0 ? 1.0 / 60 : t - lastT;
    lastT = t;
    s.realTime = t;
    s.frameMs = 0.9 * s.frameMs + 0.1 * dt * 1000.0;
    s.fps = s.frameMs > 0 ? 1000.0 / s.frameMs : 0;
    ++s.frame;
    if (s.frame == 3 && !keysApplied) applyStartupKeys(s);

    if (s.opt.pickTest && s.frame >= s.opt.frames) {
        s.clock.paused = true;
        runPickSelfTest(s);
    }

    updateTour(s, dt);
    stepSimulation(s, dt);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGLUT_NewFrame();
    ImGui::NewFrame();
    renderFrame(s);
    if (s.showUi && !s.quadView) drawPanels(s);
    ImGui::Render();
    glViewport(0, 0, s.winW, s.winH);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    if (!s.opt.shot.empty() && s.frame == s.opt.frames) actScreenshot(s);
    if (s.shotPending) {
        saveScreenshot(s);
        if (!s.opt.shot.empty()) s.quit = true;
    }
    glutSwapBuffers();

    if (s.opt.benchFrames > 0 && s.frame > 60) {  // skip warm-up (catch-up screening, texture upload)
        bench.frames++;
        bench.frame += dt * 1000.0;
        bench.sim += s.simMs;
        bench.prop += s.sim.propagateMs;
        bench.screen += s.screenMs;
        bench.slice += s.sim.screener.avgSliceMs;
        bench.render += s.renderMs;
        if (bench.frames >= s.opt.benchFrames) finishBench(s);
    }
    if (s.quit) glutLeaveMainLoop();
}

static bool imguiMouse() { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse; }
static bool imguiKeys() { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard; }

static void cbReshape(int w, int h) {
    g.winW = std::max(1, w);
    g.winH = std::max(1, h);
    ImGui_ImplGLUT_ReshapeFunc(w, h);
}
static void cbKeyboard(unsigned char c, int x, int y) {
    ImGui_ImplGLUT_KeyboardFunc(c, x, y);
    if (!imguiKeys()) onKey(g, c);
}
static void cbKeyboardUp(unsigned char c, int x, int y) { ImGui_ImplGLUT_KeyboardUpFunc(c, x, y); }
static void cbSpecial(int k, int x, int y) {
    ImGui_ImplGLUT_SpecialFunc(k, x, y);
    if (!imguiKeys()) onSpecial(g, k);
}
static void cbSpecialUp(int k, int x, int y) { ImGui_ImplGLUT_SpecialUpFunc(k, x, y); }
static void cbMouse(int b, int st, int x, int y) {
    ImGui_ImplGLUT_MouseFunc(b, st, x, y);
    if (st == GLUT_UP || !imguiMouse()) onMouseButton(g, b, st, x, y);
}
static void cbMotion(int x, int y) {
    ImGui_ImplGLUT_MotionFunc(x, y);
    if (!imguiMouse()) onMouseDrag(g, x, y);
}
static void cbWheel(int b, int dir, int x, int y) {
    ImGui_ImplGLUT_MouseWheelFunc(b, dir, x, y);
    if (!imguiMouse()) onWheel(g, dir, x, y);
}

static void disableVsync() {
#ifdef _WIN32
    using SwapFn = BOOL(WINAPI*)(int);
    if (auto f = (SwapFn)wglGetProcAddress("wglSwapIntervalEXT")) f(0);
#endif
}

int runApp(const Options& opt, int argc, char** argv) {
    g.opt = opt;
    g.shotPath = opt.shot;
    if (!loadConfig(opt.config, baseCfg)) std::fprintf(stderr, "config %s not found, using built-in defaults\n", opt.config.c_str());
    g.winW = baseCfg.winW;
    g.winH = baseCfg.winH;

    int glutArgc = 1;  // our own flags are not GLUT's business
    glutInit(&glutArgc, argv);
    (void)argc;
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutSetOption(GLUT_MULTISAMPLE, 4);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | (baseCfg.msaa ? GLUT_MULTISAMPLE : 0));
    glutInitWindowSize(g.winW, g.winH);
    glutCreateWindow("ORBITGUARD - Space Situational Awareness Console");
    std::printf("OpenGL %s | %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // deterministic layout every launch
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.86f;
    ImGui_ImplGLUT_Init();
    ImGui_ImplOpenGL2_Init();

    glutDisplayFunc(display);
    glutIdleFunc([] { glutPostRedisplay(); });
    glutReshapeFunc(cbReshape);
    glutKeyboardFunc(cbKeyboard);
    glutKeyboardUpFunc(cbKeyboardUp);
    glutSpecialFunc(cbSpecial);
    glutSpecialUpFunc(cbSpecialUp);
    glutMouseFunc(cbMouse);
    glutMotionFunc(cbMotion);
    glutPassiveMotionFunc([](int x, int y) { ImGui_ImplGLUT_MotionFunc(x, y); });
    glutMouseWheelFunc(cbWheel);
    if (opt.benchFrames > 0) disableVsync();

    renderer.init(baseCfg);
    actLoadScenario(g, opt.scenario.empty() ? "" : resolvePath(opt.scenario));
    if (opt.pickTest) g.showUi = false;

    glutMainLoop();

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGLUT_Shutdown();
    ImGui::DestroyContext();
    return g.exitCode;
}

}  // namespace og
