// Illumination: ambient + diffuse + specular sun (GL_LIGHT0, directional), flat vs Gouraud shading,
// and the per-vertex night-side weight used to blend city lights in past the terminator.
#pragma once
#include <vector>
#include "geom/Mesh.h"

namespace og {

// Must be called with the view matrix loaded, so the light direction is in world (ECI) space.
void setupSunLight(const Vec3& sunDirWorld, bool flatShading);
void setMaterial(float ambient, float diffuse, float specular, float shininess);
// alpha_i = clamp((0.08 - N_i . L) * 4, 0, 1): 0 on the day side, ramping to 1 just past the terminator.
void nightWeights(const Mesh& sphere, const Vec3& sunDirModel, std::vector<float>& rgbaOut);

}  // namespace og
