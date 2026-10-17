// Kepler solver, TLE parsing, element <-> state round trips, ISS period, full catalog load.
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cstdio>
#include "io/Config.h"
#include "orbit/TLEParser.h"

using namespace og;

int main() {
    // Kepler residual |E - e sinE - M| < 1e-10 across the domain
    double worst = 0;
    int maxIt = 0;
    for (double e = 0; e < 0.95; e += 0.05)
        for (double M = 0; M < TWO_PI; M += 0.01) {
            int it;
            double E = solveKepler(M, e, &it);
            worst = std::max(worst, std::fabs(E - e * std::sin(E) - wrapTwoPi(M)));
            maxIt = std::max(maxIt, it);
        }
    std::printf("kepler worst residual %.3e, max iterations %d\n", worst, maxIt);
    assert(worst < 1e-10 && maxIt <= 10);

    const char* iss =
        "ISS (ZARYA)\n"
        "1 25544U 98067A   26288.82192346  .00005580  00000+0  10980-3 0  9999\n"
        "2 25544  51.6317  63.1136 0006755 265.8514  94.1703 15.48879905590452\n";
    auto recs = parseTLEText(iss);
    assert(recs.size() == 1);
    const auto& r = recs[0];
    assert(r.norad == 25544 && r.name == "ISS (ZARYA)" && r.kind == Kind::Payload);
    assert(std::fabs(r.el.inc / DEG - 51.6317) < 1e-9 && std::fabs(r.el.e - 0.0006755) < 1e-12);
    double periodMin = periodS(r.el.a) / 60.0;
    std::printf("ISS a = %.1f km, period = %.2f min, epoch %s\n", r.el.a, periodMin, jdToUTC(r.el.epochJD).c_str());
    assert(periodMin > 92.0 && periodMin < 93.0);
    assert(jdToUTC(r.el.epochJD).rfind("2026-10-15 19:43", 0) == 0);

    // coe -> rv -> coe round trip (eccentric, inclined)
    Elements e0{7500, 0.12, 0.9, 1.3, 2.1, 0.7, 2461000.5};
    Vec3 rv, vv;
    coe2rv(e0, rv, vv);
    Elements e1 = rv2coe(rv, vv, e0.epochJD);
    assert(std::fabs(e1.a - e0.a) < 1e-6 && std::fabs(e1.e - e0.e) < 1e-9 && std::fabs(e1.inc - e0.inc) < 1e-9);
    assert(std::fabs(e1.raan - e0.raan) < 1e-9 && std::fabs(e1.argp - e0.argp) < 1e-9 && std::fabs(e1.M - e0.M) < 1e-9);
    assert(std::fabs(dot(vv, vv) - MU * (2 / length(rv) - 1 / e0.a)) < 1e-9);  // vis-viva

    // propagation: after one period the radius returns (J2 secular terms only rotate the orbit)
    Population p;
    p.add(r.name, r.norad, r.kind, r.el);
    Vec3 a0, va0, a1, va1;
    propagateOne(p, 0, r.el.epochJD, a0, va0);
    propagateOne(p, 0, r.el.epochJD + periodS(r.el.a) / SEC_PER_DAY, a1, va1);
    assert(std::fabs(length(a0) - length(a1)) < 0.01);
    assert(p.raanDot[0] < 0);  // prograde orbit regresses westward

    assert(std::fabs(gmst(2451545.0) / DEG - 280.46061837) < 1e-6);  // GMST at J2000

    auto cat = loadTLEFile(resolvePath("data/catalog.tle"));
    std::printf("catalog: %zu objects\n", cat.size());
    assert(cat.size() > 15000);
    std::printf("test_kepler OK\n");
    return 0;
}
