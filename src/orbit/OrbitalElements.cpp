#include "orbit/OrbitalElements.h"
#include "math/Transform3D.h"
#include <cstdio>

namespace og {

double wrapTwoPi(double x) {
    x = std::fmod(x, TWO_PI);
    return x < 0 ? x + TWO_PI : x;
}

double meanMotion(double a) { return std::sqrt(MU / (a * a * a)); }
double periodS(double a) { return TWO_PI / meanMotion(a); }

double solveKepler(double M, double e, int* iterations) {
    M = wrapTwoPi(M);
    double E = (e < 0.8) ? M : PI;
    int it = 0;
    for (; it < 10; ++it) {
        double f = E - e * std::sin(E) - M;
        double d = f / (1.0 - e * std::cos(E));
        E -= d;
        if (std::fabs(d) < 1e-12) { ++it; break; }
    }
    if (iterations) *iterations = it;
    return E;
}

void coe2rv(const Elements& el, Vec3& r, Vec3& v) {
    double E = solveKepler(el.M, el.e);
    double cosE = std::cos(E), sinE = std::sin(E);
    double rad = el.a * (1.0 - el.e * cosE);
    double fac = std::sqrt(1.0 - el.e * el.e);
    double cosNu = (cosE - el.e) / (1.0 - el.e * cosE);
    double sinNu = fac * sinE / (1.0 - el.e * cosE);
    double p = el.a * (1.0 - el.e * el.e);
    double vs = std::sqrt(MU / p);
    Mat4 R = perifocalToECI(el.raan, el.inc, el.argp);
    r = R.transformDir({rad * cosNu, rad * sinNu, 0.0});
    v = R.transformDir({-vs * sinNu, vs * (el.e + cosNu), 0.0});
}

// Signed angle from a to b, measured about axis n (all in the orbit plane).
static double planeAngle(const Vec3& a, const Vec3& b, const Vec3& n) {
    return std::atan2(dot(cross(a, b), n), dot(a, b));
}

Elements rv2coe(const Vec3& r, const Vec3& v, double epochJD) {
    Elements el;
    el.epochJD = epochJD;
    double rm = length(r), v2 = dot(v, v);
    Vec3 h = cross(r, v);
    Vec3 hHat = normalize(h);
    Vec3 node = {-h.y, h.x, 0.0};
    double nm = length(node);
    Vec3 ev = (r * (v2 - MU / rm) - v * dot(r, v)) / MU;
    el.e = length(ev);
    el.a = -MU / (2.0 * (v2 / 2.0 - MU / rm));
    el.inc = std::acos(std::fmax(-1.0, std::fmin(1.0, hHat.z)));
    Vec3 nHat = nm > 1e-9 ? node / nm : Vec3{1, 0, 0};  // equatorial: measure from x axis
    el.raan = nm > 1e-9 ? wrapTwoPi(std::atan2(node.y, node.x)) : 0.0;
    double nu;
    if (el.e > 1e-9) {
        Vec3 eHat = ev / el.e;
        el.argp = wrapTwoPi(planeAngle(nHat, eHat, hHat));
        nu = planeAngle(eHat, normalize(r), hHat);
    } else {  // circular: periapsis undefined, use argument of latitude
        el.e = 0.0;
        el.argp = 0.0;
        nu = planeAngle(nHat, normalize(r), hHat);
    }
    double E = 2.0 * std::atan2(std::sqrt(1.0 - el.e) * std::sin(nu / 2), std::sqrt(1.0 + el.e) * std::cos(nu / 2));
    el.M = wrapTwoPi(E - el.e * std::sin(E));
    return el;
}

double jdFromCalendar(int y, int mo, int d, int h, int mi, double s) {
    if (mo <= 2) { y -= 1; mo += 12; }
    int A = y / 100, B = 2 - A + A / 4;
    double jd = std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (mo + 1)) + d + B - 1524.5;
    return jd + (h + mi / 60.0 + s / 3600.0) / 24.0;
}

std::string jdToUTC(double jd) {
    double z = std::floor(jd + 0.5), f = jd + 0.5 - z;
    double alpha = std::floor((z - 1867216.25) / 36524.25);
    double A = z + 1 + alpha - std::floor(alpha / 4);
    double B = A + 1524, C = std::floor((B - 122.1) / 365.25);
    double D = std::floor(365.25 * C), E = std::floor((B - D) / 30.6001);
    int day = int(B - D - std::floor(30.6001 * E));
    int mo = int(E < 14 ? E - 1 : E - 13);
    int yr = int(mo > 2 ? C - 4716 : C - 4715);
    long secs = long(std::floor(f * SEC_PER_DAY + 0.5));
    if (secs >= 86400) secs = 86399;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02ld:%02ld:%02ld", yr, mo, day, secs / 3600, (secs / 60) % 60, secs % 60);
    return buf;
}

bool parseUTC(const std::string& s, double& jd) {
    int y, mo, d, h = 0, mi = 0; double sec = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &sec) < 3) return false;
    jd = jdFromCalendar(y, mo, d, h, mi, sec);
    return true;
}

double gmst(double jd) {
    double deg = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
    return wrapTwoPi(deg * DEG);
}

Vec3 eciToEcef(const Vec3& r, double jd) { return rotateZ(-gmst(jd)).transformDir(r); }
Vec3 ecefToEci(const Vec3& r, double jd) { return rotateZ(gmst(jd)).transformDir(r); }

void ecefToLatLon(const Vec3& p, double& lat, double& lon, double& alt) {
    double rm = length(p);
    lat = std::asin(p.z / rm) / DEG;
    lon = std::atan2(p.y, p.x) / DEG;
    alt = rm - RE;
}

Vec3 latLonToEcef(double lat, double lon, double alt) {
    double r = RE + alt, cl = std::cos(lat * DEG);
    return {r * cl * std::cos(lon * DEG), r * cl * std::sin(lon * DEG), r * std::sin(lat * DEG)};
}

double elevationDeg(const Vec3& st, const Vec3& sat) {
    Vec3 rho = normalize(sat - st);
    return std::asin(dot(rho, normalize(st))) / DEG;
}

Vec3 sunDirectionECI(double jd) {
    double n = jd - 2451545.0;
    double L = (280.460 + 0.9856474 * n) * DEG;
    double g = (357.528 + 0.9856003 * n) * DEG;
    double lam = L + (1.915 * std::sin(g) + 0.020 * std::sin(2 * g)) * DEG;
    double eps = (23.439 - 0.0000004 * n) * DEG;
    return normalize({std::cos(lam), std::cos(eps) * std::sin(lam), std::sin(eps) * std::sin(lam)});
}

}  // namespace og
