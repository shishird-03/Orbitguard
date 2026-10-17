// Fixed-timestep simulation clock, decoupled from the render rate.
#pragma once

namespace og {

struct SimClock {
    double tickDt = 1.0 / 50.0;  // real seconds per sim tick
    double warp = 60.0;          // sim seconds per real second
    double warpMax = 10000.0;
    int maxTicksPerFrame = 5;
    bool paused = false;
    int pendingSteps = 0;        // single-steps requested while paused

    // Accumulate real time; returns how many fixed ticks to run this frame.
    int advance(double realDt);
    double simDtPerTick() const { return warp * tickDt; }
    double alpha() const { return acc_ / tickDt; }  // interpolation factor between the last two ticks
    void warpUp();
    void warpDown();

private:
    double acc_ = 0.0;
};

}  // namespace og
