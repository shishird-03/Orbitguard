// Collision-avoidance planner: minimum along-track delta-v that restores a safe miss distance
// without creating a new conjunction with anything else in the catalog.
#pragma once
#include <string>
#include "ssa/Conjunction.h"

namespace og {

struct BurnPlan {
    bool valid = false;
    std::string note;
    int obj = -1, threat = -1;
    double burnJD = 0, dvMps = 0;          // signed: + prograde, - retrograde
    double oldMissKm = 0, newMissKm = 0, newTcaJD = 0, fuelKg = 0;
    double nearestOtherKm = 1e9;           // closest approach to any third object after the burn
    int nearestOther = -1, candidatesTried = 0;
    Elements before, after;
};

// checkAll=false skips the full-catalog re-screen (fast estimate, used for CSV export).
BurnPlan planAvoidance(const Population& p, const Conjunction& cj, double nowJD, const Config& c, bool checkAll = true);
void commitBurn(Population& p, const BurnPlan& plan);
double propellantKg(double dvMps, double ispS, double massKg);  // Tsiolkovsky

}  // namespace og
