#include "ssa/Conjunction.h"
#include <algorithm>
#include <chrono>

namespace og {

const char* tierName(Tier t) {
    switch (t) {
        case Tier::Red: return "RED";
        case Tier::Amber: return "AMBER";
        case Tier::Yellow: return "YELLOW";
        default: return "GREEN";
    }
}

Tier classifyMiss(double d, const Config& c) {
    if (d < c.redKm) return Tier::Red;
    if (d < c.amberKm) return Tier::Amber;
    if (d < c.yellowKm) return Tier::Yellow;
    return Tier::Green;
}

double approxPc(double d, double R, double s) {
    return std::min(1.0, (R * R) / (2 * s * s) * std::exp(-(d * d) / (2 * s * s)));
}

OrbitRef orbitOf(const Population& p, int i) {
    OrbitRef o;
    o.el = p.el[i]; o.n = p.n[i]; o.raanDot = p.raanDot[i]; o.argpDot = p.argpDot[i];
    return o;
}

OrbitRef orbitFrom(const Elements& e) {
    OrbitRef o;
    o.el = e;
    j2Rates(e, o.n, o.raanDot, o.argpDot);
    return o;
}

double goldenMin(const std::function<double(double)>& f, double a, double b, double tol, double& fmin) {
    const double g = 0.6180339887498949;
    double c = b - g * (b - a), d = a + g * (b - a);
    double fc = f(c), fd = f(d);
    while (b - a > tol) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - g * (b - a); fc = f(c); }
        else         { a = c; c = d; fc = fd; d = a + g * (b - a); fd = f(d); }
    }
    double x = 0.5 * (a + b);
    fmin = f(x);
    return x;
}

static double dist(const OrbitRef& A, const OrbitRef& B, double jd) {
    Vec3 ra, va, rb, vb;
    A.state(jd, ra, va); B.state(jd, rb, vb);
    return length(ra - rb);
}

double findTCA(const OrbitRef& A, const OrbitRef& B, double t0, double t1, double stepS,
               double& missKm, double& relSpeed) {
    double step = stepS / SEC_PER_DAY;
    double best = t0, bestD = 1e30;
    for (double t = t0; t <= t1 + 1e-12; t += step) {
        double d = dist(A, B, t);
        if (d < bestD) { bestD = d; best = t; }
    }
    double lo = std::max(t0, best - step), hi = std::min(t1, best + step);
    // golden section in seconds-from-lo to keep precision (JD values are large)
    double fmin;
    double xs = goldenMin([&](double s) { return dist(A, B, lo + s / SEC_PER_DAY); }, 0.0,
                          (hi - lo) * SEC_PER_DAY, 1e-3, fmin);
    double tca = lo + xs / SEC_PER_DAY;
    missKm = fmin;
    Vec3 ra, va, rb, vb;
    A.state(tca, ra, va); B.state(tca, rb, vb);
    relSpeed = length(va - vb);
    return tca;
}

uint64_t pairKey(int a, int b) {
    if (a > b) std::swap(a, b);
    return (uint64_t(uint32_t(a)) << 32) | uint32_t(b);
}

void mergeAlert(std::vector<Conjunction>& alerts, const Conjunction& c, AlertIndex& index) {
    auto& slot = index[pairKey(c.a, c.b)];
    for (int k : slot) {
        auto& a = alerts[k];
        if (std::fabs(a.tcaJD - c.tcaJD) * SEC_PER_DAY < 300.0) {
            if (c.missKm < a.missKm) {
                double det = a.detectedJD;
                bool passed = a.passed;
                a = c;
                a.detectedJD = det;
                a.passed = passed;
            }
            return;
        }
    }
    slot.push_back(int(alerts.size()));
    alerts.push_back(c);
}

double Screener::cellKm(const Config& c) const {
    // A pair that comes within the alert distance anywhere in the slice is, at the slice midpoint,
    // at most vrel_max*slice/2 + yellow apart, so it must share or neighbor a cell of this size.
    // ponytail: vrel_max is a global bound (15 km/s); per-shell bounds would shrink cells in GEO.
    return std::max(c.cellKm, c.vrelMaxKms * c.sliceS * 0.5 + c.yellowKm + c.marginKm);
}

void Screener::run(const Population& p, double nowJD, const Config& c, double budgetMs,
                   std::vector<Conjunction>& alerts, unsigned alertsGen) {
    auto t0 = std::chrono::steady_clock::now();
    slicesLastRun = 0;
    // Falling behind sim time means those slices will never be screened: count it as lost coverage.
    if (horizonJD_ < nowJD) {
        skippedS += (nowJD - horizonJD_) * SEC_PER_DAY;
        horizonJD_ = nowJD;
    }
    double target = nowJD + c.lookaheadS / SEC_PER_DAY;
    double slice = c.sliceS / SEC_PER_DAY;
    if (horizonJD_ >= target) { lastRunMs = 0; return; }
    if (alertsGen != indexGen_) {  // caller compacted the list: indices moved
        index_.clear();
        for (int k = 0; k < int(alerts.size()); ++k) index_[pairKey(alerts[k].a, alerts[k].b)].push_back(k);
        indexGen_ = alertsGen;
    }
    while (horizonJD_ < target) {
        screenSlice(p, horizonJD_, horizonJD_ + slice, c, nowJD, alerts, index_);
        horizonJD_ += slice;
        ++slicesLastRun;
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (ms > budgetMs) break;
    }
    lastRunMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (slicesLastRun) avgSliceMs = 0.9 * avgSliceMs + 0.1 * (lastRunMs / slicesLastRun);
}

void Screener::screenSlice(const Population& p, double t0, double t1, const Config& c, double nowJD,
                           std::vector<Conjunction>& alerts, AlertIndex& index) {
    using clk = std::chrono::steady_clock;
    auto since = [](clk::time_point a) { return std::chrono::duration<double, std::milli>(clk::now() - a).count(); };
    double tm = 0.5 * (t0 + t1);
    double half = 0.5 * c.sliceS;
    auto ts = clk::now();
    propagateAll(p, tm, pos_, vel_);
    msPropagate = since(ts);
    ts = clk::now();
    grid.setCellSize(cellKm(c));
    gridMoved = grid.update(pos_, p.alive);
    msGrid = since(ts);
    ts = clk::now();

    const double gate = c.yellowKm + c.marginKm;
    struct Cand { int i, j; double tLin; };  // tLin: linearized TCA offset from the slice midpoint (s)
    std::vector<Cand> narrow;
    int pairs = 0;
    const auto& keys = grid.cellKeys();
#pragma omp parallel
    {
    std::vector<Cand> local;
    int localPairs = 0;
#pragma omp for schedule(dynamic, 64) nowait
    for (int kc = 0; kc < int(keys.size()); ++kc)
    grid.pairsForCell(keys[kc], [&](int i, int j) {
        ++localPairs;
        // Fragments are only screened against intact objects: cm-scale fragment pairs have negligible
        // cross-section, and screening them would swamp the alert list right after every breakup.
        if (p.kind[i] == Kind::Fragment && p.kind[j] == Kind::Fragment) return;
        // perigee/apogee filter: radial shells that never overlap can't meet
        const Elements &ei = p.el[i], &ej = p.el[j];
        double qi = ei.a * (1 - ei.e), Qi = ei.a * (1 + ei.e), qj = ej.a * (1 - ej.e), Qj = ej.a * (1 + ej.e);
        if (std::max(qi, qj) - std::min(Qi, Qj) > gate) return;
        // linear relative motion about the slice midpoint
        Vec3 r = pos_[j] - pos_[i], v = vel_[j] - vel_[i];
        double vv = dot(v, v);
        double t = vv > 0 ? std::clamp(-dot(r, v) / vv, -half, half) : 0.0;
        if (length(r + v * t) < gate) local.push_back({i, j, t});
    });
#pragma omp critical
    {
        narrow.insert(narrow.end(), local.begin(), local.end());
        pairs += localPairs;
    }
    }
    // thread interleaving changes the order; sort so results (and Kessler RNG use) are deterministic
    std::sort(narrow.begin(), narrow.end(), [](const Cand& x, const Cand& y) { return x.i != y.i ? x.i < y.i : x.j < y.j; });
    candidatePairs = pairs;
    narrowChecks = int(narrow.size());
    msBroad = since(ts);
    ts = clk::now();

    // Narrow phase (parallel): relative motion is near-linear over a slice, so the true minimum lies
    // close to the linear estimate; golden-section search on the propagated distance in a bracket around it.
    struct Hit { double tca, miss, vrel; };
    std::vector<Hit> hits(narrow.size());
    const double bracket = std::max(10.0, 0.5 * c.sliceS);
#pragma omp parallel for schedule(dynamic, 16)
    for (int k = 0; k < int(narrow.size()); ++k) {
        OrbitRef A = orbitOf(p, narrow[k].i), B = orbitOf(p, narrow[k].j);
        double lo = tm + (narrow[k].tLin - bracket) / SEC_PER_DAY, fmin;
        double xs = goldenMin([&](double s) { return dist(A, B, lo + s / SEC_PER_DAY); }, 0.0, 2 * bracket, 1e-3, fmin);
        double tca = lo + xs / SEC_PER_DAY;
        Vec3 ra, va, rb, vb;
        A.state(tca, ra, va);
        B.state(tca, rb, vb);
        hits[k] = {tca, fmin, length(va - vb)};
    }
    msNarrow = since(ts);

    for (size_t k = 0; k < narrow.size(); ++k) {
        int i = narrow[k].i, j = narrow[k].j;
        double tca = hits[k].tca, miss = hits[k].miss, vrel = hits[k].vrel;
        if (miss >= c.yellowKm || tca < nowJD) continue;
        if (vrel < c.formationKms) continue;  // docked / formation-flying: co-located by design, not a conjunction
        Conjunction cj;
        cj.a = i; cj.b = j; cj.tcaJD = tca; cj.missKm = miss; cj.relSpeedKms = vrel;
        cj.pc = approxPc(miss, c.hardBodyKm, c.sigmaKm);
        cj.tier = classifyMiss(miss, c);
        cj.detectedJD = nowJD;
        mergeAlert(alerts, cj, index);
    }
}

}  // namespace og
