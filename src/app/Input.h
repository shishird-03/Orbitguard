// Keyboard and mouse handling (GLUT callbacks forward here once ImGui has declined the event).
#pragma once
#include "app/AppState.h"

namespace og {

void onKey(AppState& s, unsigned char key);
void onSpecial(AppState& s, int key);
void onMouseButton(AppState& s, int button, int state, int x, int y);  // y from the top
void onMouseDrag(AppState& s, int x, int y);
void onWheel(AppState& s, int dir, int x, int y);

}  // namespace og
