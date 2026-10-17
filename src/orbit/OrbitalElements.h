// Classical orbital elements, Kepler's equation, state-vector conversions, time & frames.
#pragma once
#include <string>
#include "math/Mat4.h"

namespace og {

constexpr double MU = 398600.4418;      // km^3/s^2
constexpr double RE = 6378.137;         // km
constexpr double J2 = 1.08263e-3;
constexpr double WE = 7.2921159e-5;     // rad/s
constexpr double SEC_PER_DAY = 86400.0;
constexpr double TWO_PI = 2.0 * PI;

// Angles in radians, a in km. M is the mean anomaly at epochJD.
struct Elements {
    double a = 0, e = 0, inc = 0, raan = 0, argp = 0, M = 0;
    double epochJD = 0;
};

double wrapTwoPi(double x);
double meanMotion(double a);  // rad/s
double periodS(double a);
double solveKepler(double M, double e, int* iterations = nullptr);  // Newton-Raphson
void coe2rv(const Elements& el, Vec3& r, Vec3& v);                  // state at el.M
Elements rv2coe(const Vec3& r, const Vec3& v, double epochJD);

// Time
double jdFromCalendar(int y, int mo, int d, int h, int mi, double s);
std::string jdToUTC(double jd);                        // "YYYY-MM-DD HH:MM:SS"
bool parseUTC(const std::string& iso, double& jdOut);  // "YYYY-MM-DDTHH:MM:SSZ"
double gmst(double jd);                                // rad

// Frames (spherical Earth: good enough for display, honest about it)
Vec3 eciToEcef(const Vec3& r, double jd);
Vec3 ecefToEci(const Vec3& r, double jd);
void ecefToLatLon(const Vec3& p, double& latDeg, double& lonDeg, double& altKm);
Vec3 latLonToEcef(double latDeg, double lonDeg, double altKm);
double elevationDeg(const Vec3& stationEcef, const Vec3& satEcef);
Vec3 sunDirectionECI(double jd);  // unit vector, low-precision almanac formula

}  // namespace og
