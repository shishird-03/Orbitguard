// Headless simulation core: population + clock + screening + collisions. No OpenGL; the app and
// the tests both drive it through step().
#pragma once
#include <string>
#include <vector>
#include "ssa/Conjunction.h"
#include "ssa/Kessler.h"

namespace og {

struct SimEvent { double jd; Tier tier; std::string text; };

class Simulation {
public:
    Config cfg;
    Population pop;
    double nowJD = 0;
    Screener screener;
    std::vector<Conjunction> alerts;
    KesslerStats kessler;
    std::vector<SimEvent> events;
    double propagateMs = 0;
    unsigned alertsGen = 0;  // bumped whenever alerts are removed (screener re-indexes)

    void start(double jd);
    // Advance by dtS sim seconds; screening may use up to budgetMs of wall time (<0 = unlimited).
    void step(double dtS, double budgetMs);
    // Screening only (no time advance): lets the look-ahead fill in while paused.
    void screen(double budgetMs);
    // Jump the clock (scrub): drops alerts and restarts screening from the new time.
    void jumpTo(double jd);
    // A burn changed obj's orbit: forget its alerts and re-screen the window.
    void afterBurn(int obj);
    // User-triggered anti-satellite test on `target`.
    int asat(int target);
    void log(Tier t, const std::string& text);
    int activeAlerts(Tier minTier = Tier::Yellow) const;
};

}  // namespace og
