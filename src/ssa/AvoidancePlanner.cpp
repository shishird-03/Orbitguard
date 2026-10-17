#include "ssa/AvoidancePlanner.h"
#include <algorithm>
#include <cstdio>

namespace og {

double propellantKg(double dvMps, double isp, double m0) {
    return m0 * (1.0 - std::exp(-std::fabs(dvMps) / (isp * 9.80665)));
}

BurnPlan planAvoidance(const Population& p, const Conjunction& cj, double nowJD, const Config& c, bool checkAll) {
    BurnPlan plan;
    plan.oldMissKm = cj.missKm;
    plan.burnJD = nowJD;
    if (cj.a < 0 || !p.alive[cj.a] || !p.alive[cj.b]) { plan.note = "Conjunction objects no longer exist"; return plan; }
    if (cj.tcaJD <= nowJD) { plan.note = "TCA already passed"; return plan; }
    if (p.kind[cj.a] == Kind::Payload) { plan.obj = cj.a; plan.threat = cj.b; }
    else if (p.kind[cj.b] == Kind::Payload) { plan.obj = cj.b; plan.threat = cj.a; }
    else { plan.note = "Neither object is a maneuverable payload"; return plan; }

    const int obj = plan.obj;
    OrbitRef threat = orbitOf(p, plan.threat);
    Vec3 r, v;
    propagateOne(p, obj, nowJD, r, v);
    plan.before = elementsAt(p, obj, nowJD);
    Vec3 vhat = normalize(v);

    // candidate magnitudes, log-spaced, each tried prograde then retrograde
    std::vector<double> cands;
    for (int k = 0; k < c.dvSteps; ++k) {
        double f = c.dvSteps > 1 ? double(k) / (c.dvSteps - 1) : 0.0;
        double mag = c.dvMinMps * std::pow(c.dvMaxMps / c.dvMinMps, f);
        cands.push_back(mag);
        cands.push_back(-mag);
    }

    double win = 900.0 / SEC_PER_DAY;
    double tEnd = std::max(nowJD + c.lookaheadS / SEC_PER_DAY, cj.tcaJD + win);
    std::vector<Vec3> allPos, allVel;
    int blockedBy = -1;  // last third object that vetoed a burn which did clear the threat
    double gate = c.vrelMaxKms * c.plannerSampleS * 0.5 + c.safeKm;

    for (double dv : cands) {
        ++plan.candidatesTried;
        Vec3 v2 = v + vhat * (dv / 1000.0);
        OrbitRef trial = orbitFrom(rv2coe(r, v2, nowJD));
        double miss, vrel;
        double tca = findTCA(trial, threat, std::max(nowJD, cj.tcaJD - win), cj.tcaJD + win, 5.0, miss, vrel);
        if (miss < c.safeKm) continue;

        double nearest = 1e9; int nearestId = -1;
        bool ok = true;
        if (checkAll) {
            // Re-screen the trial orbit against everything, sampling the window. Usually only the
            // first candidate that clears the threat gets here, so propagating on the fly is cheaper
            // than caching N x samples states.
            double half = 0.5 * c.plannerSampleS;
            for (double tk = nowJD; tk <= tEnd && ok; tk += c.plannerSampleS / SEC_PER_DAY) {
                propagateAll(p, tk, allPos, allVel);
                Vec3 rk, vk;
                trial.state(tk, rk, vk);
                for (int j = 0; j < int(p.size()); ++j) {
                    if (j == obj || j == plan.threat || !p.alive[j]) continue;
                    Vec3 rr = allPos[j] - rk;
                    if (std::fabs(rr.x) > gate || std::fabs(rr.y) > gate || std::fabs(rr.z) > gate) continue;
                    Vec3 vv = allVel[j] - vk;
                    double s2 = dot(vv, vv);
                    double t = s2 > 0 ? std::clamp(-dot(rr, vv) / s2, -half, half) : 0.0;
                    if (length(rr + vv * t) > c.safeKm + 2.0) continue;
                    double m2, vr2;
                    findTCA(trial, orbitOf(p, j), tk - half / SEC_PER_DAY, tk + half / SEC_PER_DAY, 1.0, m2, vr2);
                    if (m2 < nearest) { nearest = m2; nearestId = j; }
                    if (m2 < c.safeKm) { ok = false; blockedBy = j; break; }
                }
            }
        }
        if (!ok) continue;
        plan.valid = true;
        plan.dvMps = dv;
        plan.newMissKm = miss;
        plan.newTcaJD = tca;
        plan.nearestOtherKm = nearest;
        plan.nearestOther = nearestId;
        plan.after = trial.el;
        plan.fuelKg = propellantKg(dv, c.ispS, c.massKg);
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s burn %.3f m/s -> miss %.2f km (was %.3f km)",
                      dv > 0 ? "Prograde" : "Retrograde", std::fabs(dv), miss, cj.missKm);
        plan.note = buf;
        return plan;
    }
    if (blockedBy >= 0) {
        plan.note = "Burns that clear the threat would create a new conjunction with " + p.name[blockedBy];
        return plan;
    }
    plan.note = "No burn within +/-" + std::to_string(c.dvMaxMps).substr(0, 4) +
                " m/s reaches the safe distance; TCA too close for along-track only";
    return plan;
}

void commitBurn(Population& p, const BurnPlan& plan) {
    if (!plan.valid) return;
    p.setElements(plan.obj, plan.after);
}

}  // namespace og
