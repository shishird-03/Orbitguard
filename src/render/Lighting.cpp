#include "render/Lighting.h"
#include <algorithm>
#include "render/GL.h"

namespace og {

void setupSunLight(const Vec3& s, bool flat) {
    glShadeModel(flat ? GL_FLAT : GL_SMOOTH);
    GLfloat pos[4] = {float(s.x), float(s.y), float(s.z), 0.0f};  // w=0: directional (sun at infinity)
    GLfloat amb[4] = {0.06f, 0.06f, 0.08f, 1}, dif[4] = {1.0f, 0.97f, 0.92f, 1}, spe[4] = {0.6f, 0.6f, 0.55f, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spe);
    glEnable(GL_LIGHT0);
    GLfloat global[4] = {0.03f, 0.03f, 0.04f, 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
}

void setMaterial(float a, float d, float s, float shin) {
    GLfloat am[4] = {a, a, a, 1}, di[4] = {d, d, d, 1}, sp[4] = {s, s, s, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, am);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, di);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sp);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shin);
}

void nightWeights(const Mesh& m, const Vec3& L, std::vector<float>& out) {
    out.resize(m.nrm.size() * 4);
    for (size_t i = 0; i < m.nrm.size(); ++i) {
        float a = float(std::clamp((0.08 - dot(m.nrm[i], L)) * 4.0, 0.0, 1.0));
        out[i * 4 + 0] = out[i * 4 + 1] = out[i * 4 + 2] = 1.0f;
        out[i * 4 + 3] = a;
    }
}

}  // namespace og
