// Custom-drawn mission-control HUD components.
#pragma once
#include "app/AppState.h"
#include "hud/Viewport.h"

namespace og {

void drawRadar(Canvas& cv, const AppState& s, int cx, int cy, int R);
// x, y = bottom-left corner in GL pixels
void drawGroundTrackMap(Canvas& cv, const AppState& s, unsigned mapTexture, int x, int y, int w, int h);
void drawAlertFeed(Canvas& cv, const AppState& s, int x, int y, int w);
void drawWarningBanner(Canvas& cv, const AppState& s, int W, int H);
void drawStatusBar(Canvas& cv, const AppState& s, int W);

// lat/lon window of the minimap for the current pan/zoom
void mapWindow(const MapView& m, double& lon0, double& lat0, double& lon1, double& lat1);

}  // namespace og
