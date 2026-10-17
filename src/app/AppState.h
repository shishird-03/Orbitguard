// Everything the app layer (render, HUD, UI, input) shares. The simulation itself is GL-free.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "app/SimClock.h"
#include "io/Scenario.h"
#include "lab/AlgorithmLab.h"
#include "render/Camera.h"
#include "ssa/AvoidancePlanner.h"

namespace og {

struct Options {
    std::string config = "config.json", scenario, shot, keys, select, bench, lab;
    int frames = 90, benchFrames = 0;
    bool pickTest = false;
    double camDist = 0, camYaw = 1e9, camPitch = 1e9, warp = 0;
};

struct MapView { double centerLon = 0, centerLat = 0, zoom = 1; };

struct AppState {
    Options opt;
    Simulation sim;
    ScenarioInfo scenario;
    std::string scenarioPath;  // "" = default catalog view
    SimClock clock;
    Camera camera;
    LabSettings lab;
    MapView map;

    int selected = -1;
    int focusA = -1, focusB = -1;  // conjunction being inspected/planned, by object pair (alert indices shift)
    double focusTca = 0;
    BurnPlan plan;
    bool planShown = false;

    std::vector<Vec3> prevPos, renderPos;  // render positions = lerp(prev tick, current tick)
    std::deque<Vec3> trail;                // selected object's recent ECI positions

    bool quadView = false, showOrbits = true, showStations = true, showHud = true, showUi = true;
    bool showLab = false, showThreatLines = true;
    int alertFilter = 3;  // minimum tier shown: 1 yellow, 2 amber, 3 red (full catalog has thousands of ambers)

    // cinematic tour: keyframes are offsets from an anchor object (-1 = Earth centre), so the path
    // follows moving satellites; eye and look-at each run along a uniform cubic B-spline
    struct TourKey { int anchor; Vec3 eyeOff, centerOff; };
    bool touring = false, tourPlanned = false;
    double tourT = 0, tourWarpBefore = 0;
    std::vector<TourKey> tour;

    // input plumbing
    bool pickPending = false, shotPending = false, quit = false;
    int pickX = 0, pickY = 0, exitCode = 0;
    int mapRect[4] = {0, 0, 0, 0};  // x, y (GL, bottom-left), w, h of the minimap
    std::string shotPath;

    // window / perf
    int winW = 1600, winH = 900;
    double fps = 0, frameMs = 0, simMs = 0, screenMs = 0, renderMs = 0;
    long frame = 0;
    std::string toast;
    double toastUntil = 0, realTime = 0;

    void notify(const std::string& s, double seconds = 4.0) { toast = s; toastUntil = realTime + seconds; }
    int focusIndex() const {
        for (int k = 0; k < int(sim.alerts.size()); ++k) {
            const auto& a = sim.alerts[k];
            if (((a.a == focusA && a.b == focusB) || (a.a == focusB && a.b == focusA)) &&
                std::fabs(a.tcaJD - focusTca) * SEC_PER_DAY < 300.0)
                return k;
        }
        return -1;
    }
};

}  // namespace og
