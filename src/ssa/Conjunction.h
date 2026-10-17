// Conjunction screening: time-sliced broad phase (spatial grid) + TCA narrow phase.
//
// The screener keeps a "horizon" that runs ahead of sim time. Each pass screens one slice
// [h, h+slice] of the future: propagate everything to the slice midpoint, hash into a grid whose
// cell is wide enough that any pair which gets within the alert distance during the slice must share
// or neighbor a cell, then linearize relative motion per candidate pair and refine survivors with a
// golden-section search on the true propagated distance. In steady state each pass screens the slice
// entering the look-ahead window, so threats are announced ~lookahead seconds before TCA.
#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include "io/Config.h"
#include "orbit/Propagator.h"
#include "ssa/SpatialGrid.h"

namespace og {

enum class Tier { Green = 0, Yellow, Amber, Red };
const char* tierName(Tier t);
Tier classifyMiss(double missKm, const Config& c);
// Approx. Pc: small hard-body approximation of a 2D isotropic Gaussian in the encounter plane.
double approxPc(double missKm, double hardBodyKm, double sigmaKm);

struct Conjunction {
    int a = -1, b = -1;
    double tcaJD = 0, missKm = 0, relSpeedKms = 0, pc = 0;
    Tier tier = Tier::Green;
    double detectedJD = 0;
    bool passed = false;  // TCA is in the past and has been evaluated for collision
};

// One orbit, propagatable independently of a Population (planner trial orbits use this).
struct OrbitRef {
    Elements el;
    double n = 0, raanDot = 0, argpDot = 0;
    void state(double jd, Vec3& r, Vec3& v) const { propagateElements(el, n, raanDot, argpDot, jd, r, v); }
};
OrbitRef orbitOf(const Population& p, int i);
OrbitRef orbitFrom(const Elements& e);

// Golden-section minimization of f on [a,b]; returns argmin, writes the minimum.
double goldenMin(const std::function<double(double)>& f, double a, double b, double tol, double& fmin);
// Time of closest approach between two orbits inside [t0,t1] (JD): coarse sampling, then golden section.
double findTCA(const OrbitRef& A, const OrbitRef& B, double t0, double t1, double stepS,
               double& missKm, double& relSpeedKms);

// Pair key -> indices into the alert list, so merging a new detection is O(1) instead of O(alerts).
using AlertIndex = std::unordered_map<uint64_t, std::vector<int>>;
uint64_t pairKey(int a, int b);
void mergeAlert(std::vector<Conjunction>& alerts, const Conjunction& c, AlertIndex& index);

class Screener {
public:
    void reset(double nowJD) { horizonJD_ = nowJD; }
    // Screens slices until the horizon reaches now+lookahead or the time budget is used up.
    // `alertsGen` must change whenever the caller removes/reorders alerts (the pair index is then rebuilt).
    void run(const Population& p, double nowJD, const Config& c, double budgetMs, std::vector<Conjunction>& alerts,
             unsigned alertsGen);
    double horizonJD() const { return horizonJD_; }
    double cellKm(const Config& c) const;

    // stats
    int slicesLastRun = 0, candidatePairs = 0, narrowChecks = 0, gridMoved = 0;
    double lastRunMs = 0, avgSliceMs = 0, skippedS = 0;
    double msPropagate = 0, msGrid = 0, msBroad = 0, msNarrow = 0;  // last slice, per phase
    SpatialGrid grid;

private:
    void screenSlice(const Population& p, double t0, double t1, const Config& c, double nowJD,
                     std::vector<Conjunction>& alerts, AlertIndex& index);
    double horizonJD_ = 0;
    std::vector<Vec3> pos_, vel_;
    AlertIndex index_;
    unsigned indexGen_ = ~0u;
};

}  // namespace og
