// Conjunction report export (CSV), mirroring an operator's screening output.
#pragma once
#include <string>
#include "ssa/Simulation.h"

namespace og {

// Writes reports/conjunctions_<sim-timestamp>.csv; returns the path, or "" on failure.
std::string exportConjunctionReport(const Simulation& sim, const std::string& dir = "reports");

}  // namespace og
