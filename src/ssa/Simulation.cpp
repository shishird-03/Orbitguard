#include "ssa/Simulation.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace og {

void Simulation::log(Tier t, const std::string& text) {
    events.push_back({nowJD, t, text});
    if (events.size() > 200) events.erase(events.begin());
}

int Simulation::activeAlerts(Tier minTier) const {
    int n = 0;
    for (auto& a : alerts) n += (!a.passed && int(a.tier) >= int(minTier));
    return n;
}

void Simulation::start(double jd) {
    nowJD = jd;
    propagateAll(pop, nowJD, pop.pos, pop.vel);
    screener.reset(nowJD);
    screener.grid.clear();
    ++alertsGen;
    kessler.lastSampleJD = nowJD;
    kessler.history.assign(1, float(pop.aliveCount()));
}

void Simulation::step(double dtS, double budgetMs) {
    nowJD += dtS / SEC_PER_DAY;
    auto t0 = std::chrono::steady_clock::now();
    propagateAll(pop, nowJD, pop.pos, pop.vel);
    propagateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    screen(budgetMs);

    // TCA passed: evaluate for collision
    bool collided = false;
    for (auto& a : alerts) {
        if (a.passed || a.tcaJD > nowJD) continue;
        a.passed = true;
        if (!cfg.kesslerEnabled || a.missKm >= cfg.collisionKm || !pop.alive[a.a] || !pop.alive[a.b]) continue;
        std::string na = pop.name[a.a], nb = pop.name[a.b];
        int n = spawnCollision(pop, a.a, a.b, a.tcaJD, a.relSpeedKms, cfg, kessler);
        char buf[200];
        std::snprintf(buf, sizeof buf, "COLLISION #%d: %s x %s at %.1f km/s -> %d fragments", kessler.collisions,
                      na.c_str(), nb.c_str(), a.relSpeedKms, n);
        log(Tier::Red, buf);
        collided = true;
    }
    if (collided) {
        propagateAll(pop, nowJD, pop.pos, pop.vel);
        screener.reset(nowJD);  // fragments need screening from now, not from the old horizon
    }

    // Expire old alerts in batches: compaction invalidates the screener's pair index, so only do it
    // when a meaningful fraction is stale (consumers already skip passed alerts).
    double expire = cfg.alertExpireS / SEC_PER_DAY;
    auto stale = [&](const Conjunction& a) {
        return a.tcaJD < nowJD - expire || (a.passed && (!pop.alive[a.a] || !pop.alive[a.b]));
    };
    size_t nStale = 0;
    for (auto& a : alerts) nStale += a.passed && stale(a);
    if (collided || nStale * 8 > alerts.size()) {
        alerts.erase(std::remove_if(alerts.begin(), alerts.end(), stale), alerts.end());
        ++alertsGen;
    }

    if (nowJD - kessler.lastSampleJD >= 60.0 / SEC_PER_DAY) {
        kessler.lastSampleJD = nowJD;
        kessler.history.push_back(float(pop.aliveCount()));
        if (kessler.history.size() > 600) kessler.history.erase(kessler.history.begin());
    }
}

void Simulation::screen(double budgetMs) {
    size_t before = alerts.size();
    screener.run(pop, nowJD, cfg, budgetMs < 0 ? 1e30 : budgetMs, alerts, alertsGen);
    for (size_t k = before; k < alerts.size(); ++k) {
        const auto& a = alerts[k];
        if (a.tier < Tier::Red) continue;  // the feed is for RED; the table shows the rest
        char buf[200];
        std::snprintf(buf, sizeof buf, "%s: %s x %s, miss %.2f km at %s", tierName(a.tier), pop.name[a.a].c_str(),
                      pop.name[a.b].c_str(), a.missKm, jdToUTC(a.tcaJD).substr(11).c_str());
        log(a.tier, buf);
    }
}

void Simulation::jumpTo(double jd) {
    nowJD = jd;
    alerts.clear();
    ++alertsGen;
    propagateAll(pop, nowJD, pop.pos, pop.vel);
    screener.reset(nowJD);
    kessler.lastSampleJD = nowJD;
}

void Simulation::afterBurn(int obj) {
    alerts.erase(std::remove_if(alerts.begin(), alerts.end(),
                                [&](const Conjunction& a) { return !a.passed && (a.a == obj || a.b == obj); }),
                 alerts.end());
    ++alertsGen;
    propagateAll(pop, nowJD, pop.pos, pop.vel);
    screener.reset(nowJD);
}

int Simulation::asat(int target) {
    if (target < 0 || !pop.alive[target]) return 0;
    std::string nm = pop.name[target];
    int n = spawnCollision(pop, target, -1, nowJD, cfg.asatVrelKms, cfg, kessler);
    log(Tier::Red, "ASAT TEST: " + nm + " destroyed -> " + std::to_string(n) + " fragments");
    propagateAll(pop, nowJD, pop.pos, pop.vel);
    screener.reset(nowJD);
    return n;
}

}  // namespace og
