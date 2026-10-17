#include "lab/AlgorithmLab.h"
#include <imgui.h>

namespace og {

std::string labSummary(const LabSettings& s) {
    std::string r = s.painter ? "PAINTER" : "Z-BUF";
    r += s.flat ? " FLAT" : " GOURAUD";
    r += s.aa ? " AA" : " ALIASED";
    if (s.wireframe) r += " WIRE+HLR";
    r += std::string(" ") + colorModeName(s.color);
    return r;
}

static void explain(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void drawAlgorithmLabPanel(LabSettings& s, bool* open) {
    ImGui::SetNextWindowPos(ImVec2(340, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Algorithm Lab  [L]", open)) { ImGui::End(); return; }

    ImGui::SeparatorText("Visible-surface determination");
    int vsd = s.painter ? 1 : 0;
    ImGui::RadioButton("Z-buffer", &vsd, 0); ImGui::SameLine();
    ImGui::RadioButton("Painter's (list priority)", &vsd, 1);
    s.painter = vsd == 1;
    explain(s.painter ? "Depth test OFF. Objects sorted back-to-front by eye-space depth of their centre and drawn "
                        "in that order; later draws overwrite earlier ones. Fails on interpenetrating/large objects: "
                        "watch satellites at the limb pop in front of the Earth."
                      : "Per-pixel depth comparison: a fragment is written only if nearer than the stored depth. "
                        "Order-independent and exact at pixel resolution.");

    ImGui::SeparatorText("Shading");
    int sh = s.flat ? 0 : 1;
    ImGui::RadioButton("Flat", &sh, 0); ImGui::SameLine();
    ImGui::RadioButton("Gouraud", &sh, 1);
    s.flat = sh == 0;
    explain(s.flat ? "One lighting evaluation per polygon (glShadeModel(GL_FLAT)): facets visible across the terminator."
                   : "Lighting evaluated at vertices (ambient + diffuse + specular), colours interpolated across "
                     "each polygon (glShadeModel(GL_SMOOTH)).");

    ImGui::SeparatorText("Antialiasing");
    ImGui::Checkbox("Antialiasing", &s.aa);
    explain(s.aa ? "MSAA + GL_LINE_SMOOTH in 3D; HUD lines use Xiaolin Wu (two pixels per step, coverage-weighted)."
                 : "Aliased: HUD lines use integer midpoint scan conversion, 3D lines/points unsmoothed: stair-steps.");

    ImGui::SeparatorText("Color model  [1-4]");
    int cm = int(s.color);
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        ImGui::RadioButton(colorModeName(ColorMode(i)), &cm, i);
    }
    s.color = ColorMode(cm);
    explain(colorModeExplain(s.color));

    ImGui::SeparatorText("Hidden-line removal  [W]");
    ImGui::Checkbox("Wireframe + hidden-line removal", &s.wireframe);
    explain(s.wireframe ? "Pass 1 fills polygons in the background colour with glPolygonOffset pushing them back in "
                          "depth; pass 2 draws edges. Edges behind front faces fail the depth test: hidden lines removed."
                        : "Solid shaded polygons.");
    ImGui::End();
}

}  // namespace og
