// 3D scene renderer (fixed-function OpenGL). Draws Earth, objects, orbits, maneuvers, stations.
// Visible-surface determination is either the Z-buffer or a painter's list (Algorithm Lab).
#pragma once
#include <vector>
#include "app/AppState.h"
#include "geom/Mesh.h"
#include "render/Starfield.h"

namespace og {

class Renderer {
public:
    bool init(const Config& c);
    void drawScene(const AppState& s, const Mat4& view, const Mat4& proj);
    // GPU color-ID picking buffer: Earth as black occluder + every object as a fat point of color id+1.
    void drawIdBuffer(const AppState& s, const Mat4& view, const Mat4& proj);
    unsigned dayTexture(ColorMode m) const { return dayTex_[int(m)]; }
    RGB clearColor(const AppState& s) const;

private:
    void applyLabState(const AppState& s);
    void drawEarth(const AppState& s);
    void drawPoints(const AppState& s, const std::vector<int>& subset, bool useSubset);
    void drawSatMesh(const AppState& s, int i, const Vec3& eye);
    void drawLodMeshes(const AppState& s, const Vec3& eye, std::vector<int>* listOut);
    void drawOverlays(const AppState& s);
    void drawStations(const AppState& s, bool cones);
    void drawPainter(const AppState& s, const Mat4& view, const Vec3& eye);
    void drawOrbit(const Elements& e, const RGB& c, float alpha, bool stipple) const;
    void computeObjectColors(const AppState& s);

    Mesh earth_, earthWire_, dish_;
    unsigned dayTex_[4] = {}, nightTex_[4] = {};
    unsigned satList_[4] = {}, rockList_[4] = {}, dishList_ = 0;
    Starfield stars_;
    Mat4 earthModel_;
    Vec3 sunEci_;
    std::vector<float> nightRGBA_, ptXYZ_, ptRGB_;
    std::vector<RGB> objColor_;
    std::vector<float> objRisk_;  // min miss distance of shown alerts (km), <0 if none
    const Config* cfg_ = nullptr;
};

}  // namespace og
