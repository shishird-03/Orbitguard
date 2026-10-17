// Spatial grid superset + coherence, near-miss scenario RED at expected TCA, avoidance burn,
// and Kessler cascade determinism.
#undef NDEBUG
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include "io/Scenario.h"
#include "ssa/AvoidancePlanner.h"

using namespace og;

static void testGrid() {
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> U(-1000, 1000);
    std::vector<Vec3> pos(2000);
    for (auto& p : pos) p = {U(rng), U(rng), U(rng)};
    std::vector<uint8_t> alive(pos.size(), 1);
    SpatialGrid g;
    g.setCellSize(50);
    assert(g.update(pos, alive) == int(pos.size()));
    assert(g.update(pos, alive) == 0);  // coherence: nothing moved, nothing rehashed
    std::set<std::pair<int, int>> cand;
    g.forEachCandidatePair([&](int i, int j) {
        auto k = std::make_pair(std::min(i, j), std::max(i, j));
        assert(!cand.count(k));  // each pair exactly once
        cand.insert(k);
    });
    int brute = 0;
    for (int i = 0; i < int(pos.size()); ++i)
        for (int j = i + 1; j < int(pos.size()); ++j)
            if (length(pos[i] - pos[j]) < 50) { ++brute; assert(cand.count({i, j})); }
    std::printf("grid: %d brute-force close pairs, all among %zu candidates (vs %zu all-pairs)\n", brute, cand.size(),
                pos.size() * (pos.size() - 1) / 2);
}

static void testNearMissAndPlanner() {
    Simulation sim;
    loadConfig("config.json", sim.cfg);
    ScenarioInfo info;
    std::string err;
    bool ok = loadScenario("scenarios/leo_near_miss.json", sim, info, err, true);
    if (!ok) std::printf("scenario error: %s\n", err.c_str());
    assert(ok);
    auto t0 = std::chrono::steady_clock::now();
    sim.step(0.0, -1);  // screen the whole look-ahead window
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("screened %zu objects x %.0f s window in %.0f ms (%d slices, avg %.2f ms/slice), %zu alerts\n",
                sim.pop.size(), sim.cfg.lookaheadS, ms, sim.screener.slicesLastRun, sim.screener.avgSliceMs, sim.alerts.size());

    int a = sim.pop.findByName("ORBITGUARD-1"), b = sim.pop.findByName("COSMOS 2251 DEB (SIM)");
    const Conjunction* hit = nullptr;
    for (auto& c : sim.alerts)
        if ((c.a == a && c.b == b) || (c.a == b && c.b == a)) hit = &c;
    assert(hit);
    double dtca = (hit->tcaJD - info.epochJD) * SEC_PER_DAY - info.expect["tca_s"].get<double>();
    std::printf("RED check: tier %s, miss %.3f km, TCA error %.3f s, approx Pc %.2e\n", tierName(hit->tier),
                hit->missKm, dtca, hit->pc);
    assert(hit->tier == Tier::Red);
    assert(std::fabs(dtca) < info.expect["tca_tol_s"].get<double>());
    assert(std::fabs(hit->missKm - info.expect["miss_km"].get<double>()) < 0.05);

    t0 = std::chrono::steady_clock::now();
    BurnPlan plan = planAvoidance(sim.pop, *hit, sim.nowJD, sim.cfg, true);
    ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("planner (%.0f ms, %d candidates): %s; nearest third object %s; fuel %.3f kg\n", ms,
                plan.candidatesTried, plan.note.c_str(),
                plan.nearestOther < 0 ? "none within gate" : (std::to_string(plan.nearestOtherKm) + " km").c_str(),
                plan.fuelKg);
    assert(plan.valid && plan.obj == a);
    assert(plan.newMissKm >= sim.cfg.safeKm && plan.nearestOtherKm >= sim.cfg.safeKm);

    // Rejection path: put a blocker 0.5 km off the *planned* post-burn orbit 20 min ahead. The same burn
    // would now create a new conjunction, so the planner must pick a different one (or none).
    {
        Population p2 = sim.pop;
        double jdB = sim.nowJD + 1200.0 / SEC_PER_DAY;
        Vec3 rB, vB;
        orbitFrom(plan.after).state(jdB, rB, vB);
        Elements eb;
        std::string e2;
        assert(interceptElements(rB, jdB, sim.nowJD, 0.5, 90.0, eb, e2));
        p2.add("BLOCKER", 99999, Kind::Debris, eb);
        BurnPlan plan2 = planAvoidance(p2, *hit, sim.nowJD, sim.cfg, true);
        std::printf("with blocker on the planned orbit: %s\n", plan2.note.c_str());
        // either a different burn that also clears the blocker, or an explicit veto naming it
        assert(plan2.valid || plan2.note.find("BLOCKER") != std::string::npos);
        assert(!plan2.valid || plan2.dvMps != plan.dvMps);
        assert(!plan2.valid || plan2.nearestOtherKm >= sim.cfg.safeKm);
        assert(plan2.candidatesTried > plan.candidatesTried);
    }

    // commit and re-screen: no alert with the maneuvering object below the safe distance
    commitBurn(sim.pop, plan);
    sim.afterBurn(plan.obj);
    sim.step(0.0, -1);
    for (auto& c : sim.alerts)
        if (c.a == plan.obj || c.b == plan.obj) {
            std::printf("post-burn alert %s x %s %.2f km\n", sim.pop.name[c.a].c_str(), sim.pop.name[c.b].c_str(), c.missKm);
            assert(c.missKm >= sim.cfg.safeKm);
        }
    std::printf("post-burn re-screen clean\n");
}

static std::pair<int, int> runKessler(int& alive) {
    Simulation sim;
    loadConfig("config.json", sim.cfg);
    ScenarioInfo info;
    std::string err;
    bool ok = loadScenario("scenarios/kessler_event.json", sim, info, err, true);
    if (!ok) std::printf("scenario error: %s\n", err.c_str());
    assert(ok);
    double runS = info.expect["run_s"].get<double>();
    auto w0 = std::chrono::steady_clock::now();
    for (double t = 0; t < runS; t += 10.0) {
        sim.step(10.0, -1);
        if (std::fmod(t, 3600.0) == 0.0)
            std::printf("  t=%5.0fs alive %d alerts %zu collisions %d pairs %d narrow %d wall %.1fs\n", t, sim.pop.aliveCount(),
                        sim.alerts.size(), sim.kessler.collisions, sim.screener.candidatePairs, sim.screener.narrowChecks,
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count());
    }
    alive = sim.pop.aliveCount();
    for (auto& e : sim.events)
        if (e.text.rfind("COLLISION", 0) == 0) std::printf("  [%s] %s\n", jdToUTC(e.jd).c_str(), e.text.c_str());
    return {sim.kessler.collisions, sim.kessler.fragments};
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testGrid();
    testNearMissAndPlanner();
    int alive1, alive2;
    auto r1 = runKessler(alive1);
    auto r2 = runKessler(alive2);
    std::printf("kessler: run1 %d collisions / %d fragments / %d alive, run2 %d / %d / %d\n", r1.first, r1.second,
                alive1, r2.first, r2.second, alive2);
    assert(r1 == r2 && alive1 == alive2);  // seeded => reproducible
    assert(r1.first >= 2);                 // a cascade, not just the initial event
    std::printf("test_ssa OK\n");
    return 0;
}
