// Algorithm Lab: live switches between textbook rendering algorithms, each with a one-line explanation.
#pragma once
#include <string>
#include "color/ColorModels.h"

namespace og {

struct LabSettings {
    bool painter = false;    // list-priority (painter's) instead of Z-buffer
    bool flat = false;       // flat instead of Gouraud shading
    bool aa = true;          // GL line/point smoothing + MSAA, and Wu (vs midpoint) lines in the HUD
    bool wireframe = false;  // wireframe with hidden-line removal
    ColorMode color = ColorMode::RGB;
};

std::string labSummary(const LabSettings& s);  // compact status-bar text
void drawAlgorithmLabPanel(LabSettings& s, bool* open);

}  // namespace og
