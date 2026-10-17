// Arcball / follow / free-fly camera with perspective <-> orthographic projection.
// 1 GL unit = 1 Earth radius. View and projection come from our own Mat4 code.
#pragma once
#include "math/Mat4.h"

namespace og {

enum class CamMode { Arcball, Follow, FreeFly };
const char* camModeName(CamMode m);

struct Camera {
    CamMode mode = CamMode::Arcball;
    bool ortho = false;
    double fovy = 45.0 * DEG;
    // arcball / follow: spherical coordinates around target
    double yaw = 0.9, pitch = 0.35, dist = 4.2;
    Vec3 target{0, 0, 0};
    // free-fly
    Vec3 flyPos{4.5, 0, 0.8};
    double flyYaw = PI, flyPitch = -0.15;
    // cinematic tour override
    bool overrideOn = false;
    Vec3 ovEye, ovCenter;

    Vec3 eye() const;
    Vec3 center() const;
    Mat4 view() const;
    Mat4 proj(double aspect) const;
    void rotate(double dxPx, double dyPx);
    void zoom(double steps);
    void fly(double forward, double right, double up);
};

}  // namespace og
