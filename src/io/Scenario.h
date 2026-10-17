// JSON scenarios for reproducible demos: catalog subset + explicit objects + generators
// (Walker shells, engineered intercepts) + config overrides.
#pragma once
#include <string>
#include "ssa/Simulation.h"

namespace og {

struct ScenarioInfo {
    std::string name, description, follow;
    double epochJD = 0, warp = 0;
    nlohmann::json expect;
};

// Loads catalog objects (every k-th record when limit < file size). Returns the median epoch:
// a few TLEs carry odd epochs, and the median keeps propagation spans short for most objects.
double loadCatalog(Population& p, const std::string& path, int limit);

// Circular orbit passing `missKm` radially above point rT at time jdT, in a plane of inclination incDeg;
// elements are expressed at `epoch` (J2 drift over the lead time accounted for).
bool interceptElements(const Vec3& rT, double jdT, double epoch, double missKm, double incDeg, Elements& out,
                       std::string& err);

// Fills sim.cfg (overrides applied on top of what is already there) and sim.pop, then sim.start().
// includeCatalog=false ignores the scenario's catalog block (fast headless tests).
bool loadScenario(const std::string& path, Simulation& sim, ScenarioInfo& info, std::string& err,
                  bool includeCatalog = true);

}  // namespace og
