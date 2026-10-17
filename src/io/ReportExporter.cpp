#include "io/ReportExporter.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "ssa/AvoidancePlanner.h"

namespace og {

std::string exportConjunctionReport(const Simulation& sim, const std::string& dir) {
    std::filesystem::create_directories(dir);
    std::string ts = jdToUTC(sim.nowJD);
    for (char& ch : ts) if (ch == ' ' || ch == ':') ch = (ch == ' ') ? '_' : '-';
    std::string path = dir + "/conjunctions_" + ts + ".csv";
    std::ofstream f(path);
    if (!f) return "";
    f << "object_1,norad_1,object_2,norad_2,tca_utc,miss_km,rel_speed_kms,approx_pc,tier,recommended_dv_mps,maneuvering_object\n";
    std::vector<const Conjunction*> rows;
    for (const auto& a : sim.alerts)
        if (!a.passed) rows.push_back(&a);
    std::sort(rows.begin(), rows.end(), [](const Conjunction* x, const Conjunction* y) {
        return x->tier != y->tier ? x->tier > y->tier : x->tcaJD < y->tcaJD;
    });
    for (const Conjunction* ap : rows) {
        const Conjunction& a = *ap;
        std::string dv = "", who = "";
        if (a.tier >= Tier::Amber) {  // quick estimate vs the threat only; full re-screen is interactive
            BurnPlan bp = planAvoidance(sim.pop, a, sim.nowJD, sim.cfg, false);
            if (bp.valid) { char b[32]; std::snprintf(b, sizeof b, "%.3f", bp.dvMps); dv = b; who = sim.pop.name[bp.obj]; }
            else dv = "n/a";
        }
        char line[512];
        std::snprintf(line, sizeof line, "\"%s\",%d,\"%s\",%d,%s,%.4f,%.3f,%.3e,%s,%s,\"%s\"\n",
                      sim.pop.name[a.a].c_str(), sim.pop.norad[a.a], sim.pop.name[a.b].c_str(), sim.pop.norad[a.b],
                      jdToUTC(a.tcaJD).c_str(), a.missKm, a.relSpeedKms, a.pc, tierName(a.tier), dv.c_str(), who.c_str());
        f << line;
    }
    return path;
}

}  // namespace og
