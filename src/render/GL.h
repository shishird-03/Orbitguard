// Single include point for OpenGL/GLUT + the few post-1.1 enums Windows' gl.h lacks.
#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/freeglut.h>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "color/ColorModels.h"
#include "math/Mat4.h"

namespace og {
inline void glColor(const RGB& c, float a = 1.0f) { glColor4f(float(c.r), float(c.g), float(c.b), a); }
inline void glVertex(const Vec3& v) { glVertex3d(v.x, v.y, v.z); }
inline RGB rgbOf(const double* c) { return {c[0], c[1], c[2]}; }
}  // namespace og
