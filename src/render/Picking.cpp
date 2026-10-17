#include "render/Picking.h"
#include <unordered_map>
#include "render/GL.h"

namespace og {

static int decode(const unsigned char* px) {
    int id = px[0] | (px[1] << 8) | (px[2] << 16);
    return id - 1;  // 0 (background) -> -1
}

int pickObject(Renderer& r, const AppState& s, const Mat4& view, const Mat4& proj, ViewRect vp, int x, int yTop) {
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp.x, vp.y, vp.w, vp.h);
    r.drawIdBuffer(s, view, proj);
    glDisable(GL_SCISSOR_TEST);
    const int R = 4;  // search a 9x9 neighborhood, nearest hit to the cursor wins
    int gy = s.winH - 1 - yTop;
    unsigned char buf[(2 * R + 1) * (2 * R + 1) * 3];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x - R, gy - R, 2 * R + 1, 2 * R + 1, GL_RGB, GL_UNSIGNED_BYTE, buf);
    int best = -1, bestD = 1 << 30;
    for (int dy = -R; dy <= R; ++dy)
        for (int dx = -R; dx <= R; ++dx) {
            int id = decode(buf + ((dy + R) * (2 * R + 1) + (dx + R)) * 3);
            if (id < 0 || id >= int(s.sim.pop.size())) continue;
            int d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = id; }
        }
    return best;
}

PickTestResult runPickTest(Renderer& r, const AppState& s, const Mat4& view, const Mat4& proj, ViewRect vp) {
    PickTestResult res;
    const auto& p = s.sim.pop;
    Mat4 pv = proj * view;
    Vec3 eye = inverse(view).transformPoint({0, 0, 0});
    struct Proj { int i; double x, y; };
    std::vector<Proj> vis;
    for (int i = 0; i < int(p.size()); ++i) {
        if (!p.alive[i]) continue;
        Vec3 w = s.renderPos[i] / RE;
        double in[4] = {w.x, w.y, w.z, 1.0}, c[4];
        pv.transform4(in, c);
        if (c[3] <= 0) continue;
        double nx = c[0] / c[3], ny = c[1] / c[3], nz = c[2] / c[3];
        if (nx < -0.98 || nx > 0.98 || ny < -0.98 || ny > 0.98 || nz < -1 || nz > 1) continue;
        // occluded by the Earth? (ray eye->object hits the unit sphere first)
        Vec3 d = w - eye;
        double a = dot(d, d), b = 2 * dot(eye, d), cc = dot(eye, eye) - 1.0, disc = b * b - 4 * a * cc;
        if (disc > 0) {
            double t = (-b - std::sqrt(disc)) / (2 * a);
            if (t > 0 && t < 1) continue;
        }
        vis.push_back({i, vp.x + (nx + 1) * 0.5 * vp.w, vp.y + (ny + 1) * 0.5 * vp.h});
    }
    // keep only objects with no other visible object within 9 px (otherwise "correct" is ambiguous)
    std::unordered_map<long long, int> bucket;
    auto key = [](int bx, int by) { return (long long)bx * 100000 + by; };
    for (auto& v : vis) bucket[key(int(v.x / 9), int(v.y / 9))]++;
    std::vector<Proj> iso;
    for (auto& v : vis) {
        int bx = int(v.x / 9), by = int(v.y / 9), n = 0;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                auto it = bucket.find(key(bx + dx, by + dy));
                if (it != bucket.end()) n += it->second;
            }
        if (n == 1) iso.push_back(v);
    }

    glViewport(vp.x, vp.y, vp.w, vp.h);
    r.drawIdBuffer(s, view, proj);
    std::vector<unsigned char> fb(size_t(vp.w) * vp.h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(vp.x, vp.y, vp.w, vp.h, GL_RGB, GL_UNSIGNED_BYTE, fb.data());
    for (auto& v : iso) {
        int px = int(v.x) - vp.x, py = int(v.y) - vp.y;
        if (px < 0 || py < 0 || px >= vp.w || py >= vp.h) continue;
        ++res.tested;
        if (decode(&fb[(size_t(py) * vp.w + px) * 3]) == v.i) ++res.hits;
    }
    return res;
}

}  // namespace og
