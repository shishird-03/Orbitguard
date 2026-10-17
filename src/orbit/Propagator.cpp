#include "orbit/Propagator.h"

namespace og {

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Payload: return "Payload";
        case Kind::RocketBody: return "Rocket body";
        case Kind::Debris: return "Debris";
        case Kind::Fragment: return "Fragment";
    }
    return "?";
}

void j2Rates(const Elements& e, double& n, double& raanDot, double& argpDot) {
    n = meanMotion(e.a);
    double p = e.a * (1.0 - e.e * e.e);
    double k = n * J2 * (RE / p) * (RE / p);
    double ci = std::cos(e.inc);
    raanDot = -1.5 * k * ci;
    argpDot = 0.75 * k * (5.0 * ci * ci - 1.0);
}

int Population::add(const std::string& nm, int id, Kind k, const Elements& e, int event) {
    name.push_back(nm); norad.push_back(id); kind.push_back(k);
    alive.push_back(1); eventId.push_back(event);
    el.push_back(e); n.push_back(0); raanDot.push_back(0); argpDot.push_back(0);
    pos.emplace_back(); vel.emplace_back();
    int i = int(size()) - 1;
    setElements(i, e);
    return i;
}

void Population::setElements(int i, const Elements& e) {
    el[i] = e;
    j2Rates(e, n[i], raanDot[i], argpDot[i]);
}

int Population::aliveCount() const {
    int c = 0;
    for (auto a : alive) c += a;
    return c;
}

int Population::findByName(const std::string& nm) const {
    for (size_t i = 0; i < size(); ++i) if (name[i] == nm) return int(i);
    return -1;
}

void Population::clear() { *this = Population(); }

Elements elementsAt(const Population& p, int i, double jd) {
    Elements e = p.el[i];
    double dt = (jd - e.epochJD) * SEC_PER_DAY;
    e.M = wrapTwoPi(e.M + p.n[i] * dt);
    e.raan = wrapTwoPi(e.raan + p.raanDot[i] * dt);
    e.argp = wrapTwoPi(e.argp + p.argpDot[i] * dt);
    e.epochJD = jd;
    return e;
}

void propagateElements(const Elements& e0, double n, double raanDot, double argpDot,
                       double jd, Vec3& r, Vec3& v) {
    Elements e = e0;
    double dt = (jd - e.epochJD) * SEC_PER_DAY;
    e.M = e.M + n * dt;
    e.raan = e.raan + raanDot * dt;
    e.argp = e.argp + argpDot * dt;
    coe2rv(e, r, v);
}

void propagateOne(const Population& p, int i, double jd, Vec3& r, Vec3& v) {
    propagateElements(p.el[i], p.n[i], p.raanDot[i], p.argpDot[i], jd, r, v);
}

void propagateAll(const Population& p, double jd, std::vector<Vec3>& pos, std::vector<Vec3>& vel) {
    int N = int(p.size());
    pos.resize(N); vel.resize(N);
#pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i)
        if (p.alive[i]) propagateOne(p, i, jd, pos[i], vel[i]);
}

}  // namespace og
