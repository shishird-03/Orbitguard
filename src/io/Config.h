// All tunables, loaded from config.json (scenarios may override any subset).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace og {

struct Station { std::string name; double latDeg = 0, lonDeg = 0, altKm = 0, maskDeg = 10; };

struct Config {
    // window
    int winW = 1600, winH = 900;
    bool msaa = true;
    // sim
    double tickHz = 50, warpInit = 60, warpMax = 10000;
    int maxTicksPerFrame = 5, maxObjects = 30000;
    std::string catalog = "data/catalog.tle";
    // render
    int earthStacks = 48, earthSlices = 96, starCount = 4000;
    double pointSize = 2.5, meshLodDist = 0.6, meshScale = 0.018, stationConeAltKm = 550;
    int meshLodMax = 200, trailLength = 400, orbitSamples = 256;
    // screening
    double sliceS = 10, lookaheadS = 1800, cellKm = 50, vrelMaxKms = 15, marginKm = 10, budgetMs = 6;
    double redKm = 1, amberKm = 5, yellowKm = 25, sigmaKm = 0.2, hardBodyKm = 0.02, alertExpireS = 600, formationKms = 0.05;
    // planner
    double dvMinMps = 0.01, dvMaxMps = 2.0, safeKm = 5, plannerSampleS = 20, ispS = 220, massKg = 500;
    int dvSteps = 24;
    // kessler
    bool kesslerEnabled = false;
    double collisionKm = 0.1, fragK = 8, dispersionKms = 0.1, asatVrelKms = 10, reentryAltKm = 120;
    int fragMin = 200, fragMax = 1000;
    uint32_t seed = 42;
    // hud
    double radarRangeKm = 500;
    int radarPx = 220, mapW = 400, mapH = 200;
    // tour
    double tourDurationS = 40;
    // colors (RGB 0..1)
    double colBackground[3] = {0.0, 0.0, 0.02}, colPayload[3] = {0.55, 0.85, 1.0}, colDebris[3] = {1.0, 0.55, 0.2},
           colRocket[3] = {1.0, 0.9, 0.3}, colFragment[3] = {1.0, 0.25, 0.25}, colSelected[3] = {0.2, 1.0, 0.4};
    std::vector<Station> stations;
};

std::string resolvePath(const std::string& rel);  // cwd first, then the source tree root
bool loadConfig(const std::string& path, Config& cfg);
void applyConfigJson(const nlohmann::json& j, Config& cfg);

}  // namespace og
