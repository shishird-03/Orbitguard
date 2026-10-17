#include "render/Camera.h"
#include <algorithm>

namespace og {

const char* camModeName(CamMode m) {
    switch (m) {
        case CamMode::Arcball: return "ARCBALL";
        case CamMode::Follow: return "FOLLOW";
        case CamMode::FreeFly: return "FREE-FLY";
    }
    return "?";
}

static Vec3 flyDir(double yaw, double pitch) {
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}

Vec3 Camera::eye() const {
    if (overrideOn) return ovEye;
    if (mode == CamMode::FreeFly) return flyPos;
    return target + Vec3{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)} * dist;
}

Vec3 Camera::center() const {
    if (overrideOn) return ovCenter;
    if (mode == CamMode::FreeFly) return flyPos + flyDir(flyYaw, flyPitch);
    return target;
}

Mat4 Camera::view() const { return lookAt(eye(), center(), {0, 0, 1}); }

Mat4 Camera::proj(double aspect) const {
    double d = length(eye() - center());
    if (mode == CamMode::FreeFly && !overrideOn) d = std::max(1.0, length(flyPos) - 1.0);
    if (ortho) {
        double h = d * std::tan(fovy / 2);
        return og::ortho(-h * aspect, h * aspect, -h, h, -60.0, 60.0 + d);
    }
    double n = std::clamp(d * 0.02, 0.0005, 0.05);
    return perspective(fovy, aspect, n, d + 60.0);
}

void Camera::rotate(double dx, double dy) {
    if (mode == CamMode::FreeFly) {
        flyYaw -= dx * 0.004;
        flyPitch = std::clamp(flyPitch - dy * 0.004, -1.5, 1.5);
        return;
    }
    yaw -= dx * 0.006;
    pitch = std::clamp(pitch + dy * 0.006, -1.55, 1.55);
}

void Camera::zoom(double steps) {
    if (mode == CamMode::FreeFly) { fly(steps * 0.1, 0, 0); return; }
    double minD = mode == CamMode::Follow ? 0.003 : 1.05;
    dist = std::clamp(dist * std::pow(0.88, steps), minD, 40.0);
}

void Camera::fly(double f, double r, double u) {
    Vec3 fw = flyDir(flyYaw, flyPitch);
    Vec3 rt = normalize(cross(fw, {0, 0, 1}));
    double speed = std::max(0.01, (length(flyPos) - 1.0) * 0.5);
    flyPos += (fw * f + rt * r + Vec3{0, 0, 1} * u) * speed;
}

}  // namespace og
