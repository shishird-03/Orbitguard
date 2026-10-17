// ORBITGUARD: real-time space situational awareness console.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "app/App.h"

#ifdef _WIN32
// Ask hybrid-graphics laptops (NVIDIA Optimus / AMD PowerXpress) to run us on the discrete GPU.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static void usage() {
    std::puts(
        "orbitguard [options]\n"
        "  --scenario FILE     load a scenario (scenarios/*.json); default: full catalog\n"
        "  --config FILE       config file (default config.json)\n"
        "  --shot PNG          render --frames frames, save a screenshot, exit\n"
        "  --frames N          frames before --shot / --pick-test (default 90)\n"
        "  --keys STR          keystrokes applied at startup ('!' = Enter), e.g. \"q\" or \"2W\"\n"
        "  --select NAME       select an object by name at startup\n"
        "  --lab LIST          Algorithm Lab presets: painter,flat,noaa,wire,hsv,yiq,cmy,panel\n"
        "  --cam D,YAW,PITCH   arcball distance (Earth radii) and angles (rad)\n"
        "  --warp X            time warp\n"
        "  --pick-test         color-ID picking self-check (exit code 0 = 100%)\n"
        "  --bench N [FILE]    measure N frames (vsync off), print + append to FILE");
}

int main(int argc, char** argv) {
    og::Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--scenario") o.scenario = next();
        else if (a == "--config") o.config = next();
        else if (a == "--shot") o.shot = next();
        else if (a == "--frames") o.frames = std::atoi(next().c_str());
        else if (a == "--keys") o.keys = next();
        else if (a == "--select") o.select = next();
        else if (a == "--lab") o.lab = next();
        else if (a == "--warp") o.warp = std::atof(next().c_str());
        else if (a == "--pick-test") o.pickTest = true;
        else if (a == "--cam") std::sscanf(next().c_str(), "%lf,%lf,%lf", &o.camDist, &o.camYaw, &o.camPitch);
        else if (a == "--bench") {
            o.benchFrames = std::atoi(next().c_str());
            if (i + 1 < argc && argv[i + 1][0] != '-') o.bench = argv[++i];
        } else { usage(); return a == "--help" || a == "-h" ? 0 : 2; }
    }
    return og::runApp(o, argc, argv);
}
