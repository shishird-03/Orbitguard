#include "app/Input.h"
#include <algorithm>
#include <cctype>
#include "app/App.h"
#include "hud/Hud.h"
#include "render/GL.h"

namespace og {

namespace {
struct Drag {
    bool active = false, moved = false, inMap = false;
    int startX = 0, startY = 0, lastX = 0, lastY = 0;
} drag;

bool inMap(const AppState& s, int x, int yTop) {
    int gy = s.winH - 1 - yTop;
    return !s.quadView && s.showHud && x >= s.mapRect[0] && x < s.mapRect[0] + s.mapRect[2] && gy >= s.mapRect[1] &&
           gy < s.mapRect[1] + s.mapRect[3];
}
}  // namespace

void onKey(AppState& s, unsigned char key) {
    switch (std::tolower(key)) {
        case ' ': s.clock.paused = !s.clock.paused; break;
        case '+': case '=': s.clock.warpUp(); s.notify("warp " + std::to_string(int(s.clock.warp)) + "x", 1.5); break;
        case '-': case '_': s.clock.warpDown(); s.notify("warp " + std::to_string(int(s.clock.warp)) + "x", 1.5); break;
        case '.': s.clock.paused = true; s.clock.pendingSteps++; break;
        case 'f':
            if (s.camera.mode == CamMode::Follow) {
                s.camera.mode = CamMode::Arcball;
                s.camera.target = {0, 0, 0};
                s.camera.dist = std::max(s.camera.dist, 2.5);
            } else if (s.selected >= 0) {
                s.camera.mode = CamMode::Follow;
                if (s.camera.dist > 1.0) s.camera.dist = 0.12;
            } else {
                s.notify("Select an object first");
            }
            break;
        case 'c':
            s.camera.mode = s.camera.mode == CamMode::FreeFly ? CamMode::Arcball : CamMode::FreeFly;
            if (s.camera.mode == CamMode::FreeFly) {  // start flying from where the arcball was
                s.camera.flyPos = s.camera.eye();
                Vec3 d = normalize(s.camera.center() - s.camera.flyPos);
                s.camera.flyYaw = std::atan2(d.y, d.x);
                s.camera.flyPitch = std::asin(d.z);
            } else {
                s.camera.target = {0, 0, 0};
            }
            break;
        case 'p': s.camera.ortho = !s.camera.ortho; break;
        case 'q': s.quadView = !s.quadView; break;
        case 'a': actPlan(s); break;
        case '\r': case '\n': actCommit(s); break;
        case 'k': actKessler(s); break;
        case 'l': s.showLab = !s.showLab; break;
        case 't': actTour(s); break;
        case 'e': actExport(s); break;
        case 'w': s.lab.wireframe = !s.lab.wireframe; break;
        case 'h': s.showHud = !s.showHud; s.showUi = s.showHud; break;
        case '1': case '2': case '3': case '4':
            s.lab.color = ColorMode(key - '1');
            s.notify(std::string("Color model: ") + colorModeName(s.lab.color), 2.0);
            break;
        case 27:
            if (s.touring) { s.touring = false; s.camera.overrideOn = false; s.clock.warp = s.tourWarpBefore; }
            else actQuit(s);
            break;
    }
}

void onSpecial(AppState& s, int key) {
    if (key == GLUT_KEY_F12) { actScreenshot(s); return; }
    if (s.camera.mode != CamMode::FreeFly) return;
    switch (key) {
        case GLUT_KEY_UP: s.camera.fly(0.1, 0, 0); break;
        case GLUT_KEY_DOWN: s.camera.fly(-0.1, 0, 0); break;
        case GLUT_KEY_LEFT: s.camera.fly(0, -0.1, 0); break;
        case GLUT_KEY_RIGHT: s.camera.fly(0, 0.1, 0); break;
        case GLUT_KEY_PAGE_UP: s.camera.fly(0, 0, 0.1); break;
        case GLUT_KEY_PAGE_DOWN: s.camera.fly(0, 0, -0.1); break;
    }
}

void onMouseButton(AppState& s, int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON) return;
    if (state == GLUT_DOWN) {
        drag = {true, false, inMap(s, x, y), x, y, x, y};
        return;
    }
    if (drag.active && !drag.moved && !drag.inMap) {
        s.pickPending = true;
        s.pickX = x;
        s.pickY = y;
    }
    drag.active = false;
}

void onMouseDrag(AppState& s, int x, int y) {
    if (!drag.active) return;
    int dx = x - drag.lastX, dy = y - drag.lastY;
    drag.lastX = x;
    drag.lastY = y;
    if (std::abs(x - drag.startX) + std::abs(y - drag.startY) > 3) drag.moved = true;
    if (drag.inMap) {
        double lon0, lat0, lon1, lat1;
        mapWindow(s.map, lon0, lat0, lon1, lat1);
        s.map.centerLon = (lon0 + lon1) / 2 - dx * (lon1 - lon0) / s.mapRect[2];
        s.map.centerLat = (lat0 + lat1) / 2 + dy * (lat1 - lat0) / s.mapRect[3];
    } else {
        s.camera.rotate(dx, dy);
    }
}

void onWheel(AppState& s, int dir, int x, int y) {
    if (inMap(s, x, y)) {
        double lon0, lat0, lon1, lat1;
        mapWindow(s.map, lon0, lat0, lon1, lat1);
        int gy = s.winH - 1 - y;
        double cl = lon0 + (x - s.mapRect[0]) * (lon1 - lon0) / s.mapRect[2];
        double ct = lat0 + (gy - s.mapRect[1]) * (lat1 - lat0) / s.mapRect[3];
        s.map.zoom = std::clamp(s.map.zoom * (dir > 0 ? 1.25 : 0.8), 1.0, 12.0);
        s.map.centerLon += (cl - s.map.centerLon) * 0.3;  // drift toward the cursor
        s.map.centerLat += (ct - s.map.centerLat) * 0.3;
        return;
    }
    s.camera.zoom(dir);
}

}  // namespace og
