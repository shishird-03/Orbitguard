// Object population (struct-of-arrays) + Kepler/J2 propagation.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "orbit/OrbitalElements.h"

namespace og {

enum class Kind : uint8_t { Payload, RocketBody, Debris, Fragment };
const char* kindName(Kind k);

struct Population {
    std::vector<std::string> name;
    std::vector<int> norad;
    std::vector<Kind> kind;
    std::vector<uint8_t> alive;
    std::vector<int> eventId;            // Kessler event that created it, -1 otherwise
    std::vector<Elements> el;            // elements at their own epoch
    std::vector<double> n, raanDot, argpDot;  // mean motion + J2 secular rates (rad/s)
    std::vector<Vec3> pos, vel;          // current ECI state (km, km/s)

    size_t size() const { return name.size(); }
    int add(const std::string& nm, int id, Kind k, const Elements& e, int event = -1);
    void setElements(int i, const Elements& e);
    int aliveCount() const;
    int findByName(const std::string& nm) const;
    void clear();
};

Elements elementsAt(const Population& p, int i, double jd);  // with J2 drift applied
void propagateOne(const Population& p, int i, double jd, Vec3& r, Vec3& v);
void propagateElements(const Elements& e0, double n, double raanDot, double argpDot,
                       double jd, Vec3& r, Vec3& v);
void j2Rates(const Elements& e, double& n, double& raanDot, double& argpDot);
// Propagates every live object to jd into pos/vel (parallel when OpenMP is on).
void propagateAll(const Population& p, double jd, std::vector<Vec3>& pos, std::vector<Vec3>& vel);

}  // namespace og
