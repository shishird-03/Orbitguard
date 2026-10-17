// Application shell: GLUT window + callbacks, frame loop, and the user actions shared by keyboard and UI.
#pragma once
#include "app/AppState.h"

namespace og {

AppState& app();
int runApp(const Options& opt, int argc, char** argv);

// actions (keyboard shortcuts and ImGui buttons route through these)
void actLoadScenario(AppState& s, const std::string& path);  // "" = full catalog
void actSelect(AppState& s, int i);
void actFocusAlert(AppState& s, int alertIndex);
void actPlan(AppState& s);
void actCommit(AppState& s);
void actDiscardPlan(AppState& s);
void actKessler(AppState& s);
void actExport(AppState& s);
void actTour(AppState& s);
void actScreenshot(AppState& s);
void actJump(AppState& s, double jd);
void actQuit(AppState& s);
std::vector<std::string> listScenarios();

}  // namespace og
