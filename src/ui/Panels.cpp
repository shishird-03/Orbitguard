#include "ui/Panels.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <imgui.h>
#include "app/App.h"

namespace og {

static ImVec4 tierColor(Tier t) {
    switch (t) {
        case Tier::Red: return {1.0f, 0.3f, 0.25f, 1};
        case Tier::Amber: return {1.0f, 0.65f, 0.15f, 1};
        case Tier::Yellow: return {1.0f, 0.95f, 0.35f, 1};
        default: return {0.4f, 1.0f, 0.5f, 1};
    }
}

static std::string tminus(double jd, double now) {
    long s = long((jd - now) * SEC_PER_DAY);
    char b[32];
    std::snprintf(b, sizeof b, "T%c%02ld:%02ld", s < 0 ? '+' : '-', std::labs(s) / 60, std::labs(s) % 60);
    return b;
}

static void controlPanel(AppState& s) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("ORBITGUARD Mission Control");
    ImGui::TextColored({0.6f, 0.85f, 1, 1}, "%s", s.scenarioPath.empty() ? "Full catalog snapshot" : s.scenario.name.c_str());
    if (!s.scenario.description.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s.scenario.description.c_str());

    static int picked = -1;  // combo choice not yet loaded; -1 = show what is loaded
    auto list = listScenarios();
    std::vector<std::string> labels = {"(full catalog)"};
    int current = 0;
    for (size_t k = 0; k < list.size(); ++k) {
        labels.push_back(std::filesystem::path(list[k]).stem().string());
        if (!s.scenarioPath.empty() && std::filesystem::path(list[k]).stem() == std::filesystem::path(s.scenarioPath).stem())
            current = int(k) + 1;
    }
    std::vector<const char*> cl;
    for (auto& l : labels) cl.push_back(l.c_str());
    int shown = picked >= 0 ? picked : current;
    ImGui::SetNextItemWidth(180);
    if (ImGui::Combo("##scen", &shown, cl.data(), int(cl.size()))) picked = shown;
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        actLoadScenario(s, shown == 0 ? "" : list[shown - 1]);
        picked = -1;
    }

    ImGui::SeparatorText("Time");
    ImGui::Text("%s UTC", jdToUTC(s.sim.nowJD).c_str());
    if (ImGui::Button(s.clock.paused ? "Play  [Space]" : "Pause [Space]")) s.clock.paused = !s.clock.paused;
    ImGui::SameLine();
    if (ImGui::Button("Step")) { s.clock.paused = true; s.clock.pendingSteps++; }
    float lw = float(std::log10(s.clock.warp));
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("warp", &lw, 0.0f, float(std::log10(s.clock.warpMax)), "")) s.clock.warp = std::round(std::pow(10.0, lw));
    ImGui::SameLine();
    ImGui::Text("%.0fx", s.clock.warp);
    static float scrubH = 0;
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("scrub (h)", &scrubH, -24.0f, 24.0f, "%+.1f h");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        actJump(s, s.sim.nowJD + scrubH / 24.0);
        scrubH = 0;
    }

    ImGui::SeparatorText("View");
    int cm = int(s.camera.mode);
    ImGui::RadioButton("Arcball", &cm, 0); ImGui::SameLine();
    ImGui::RadioButton("Follow [F]", &cm, 1); ImGui::SameLine();
    ImGui::RadioButton("Fly [C]", &cm, 2);
    s.camera.mode = CamMode(cm);
    ImGui::Checkbox("Ortho [P]", &s.camera.ortho); ImGui::SameLine();
    ImGui::Checkbox("Quad view [Q]", &s.quadView);
    ImGui::Checkbox("Orbits", &s.showOrbits); ImGui::SameLine();
    ImGui::Checkbox("Threat lines", &s.showThreatLines); ImGui::SameLine();
    ImGui::Checkbox("Stations", &s.showStations);
    ImGui::Checkbox("Algorithm Lab [L]", &s.showLab); ImGui::SameLine();
    ImGui::Checkbox("HUD [H]", &s.showHud);

    ImGui::SeparatorText("Actions");
    if (ImGui::Button("Tour [T]")) actTour(s);
    ImGui::SameLine();
    if (ImGui::Button("Export CSV [E]")) actExport(s);
    ImGui::SameLine();
    if (ImGui::Button("Screenshot [F12]")) actScreenshot(s);

    ImGui::SetNextItemOpen(s.sim.cfg.kesslerEnabled, ImGuiCond_Appearing);
    if (ImGui::CollapsingHeader("Kessler cascade")) {
        ImGui::Checkbox("Kessler mode (collisions spawn debris)", &s.sim.cfg.kesslerEnabled);
        if (ImGui::Button("ASAT test on selected [K]")) actKessler(s);
        const auto& k = s.sim.kessler;
        ImGui::Text("Collisions %d   fragments %d   reentered %d", k.collisions, k.fragments, k.reentered);
        if (!k.history.empty()) {
            auto [mn, mx] = std::minmax_element(k.history.begin(), k.history.end());
            char ov[48];
            std::snprintf(ov, sizeof ov, "%d objects", int(k.history.back()));
            ImGui::PlotLines("##pop", k.history.data(), int(k.history.size()), 0, ov, *mn - 1, *mx + 1, ImVec2(300, 60));
        }
    }
    ImGui::End();
}

static void inspector(AppState& s) {
    ImGui::SetNextWindowPos(ImVec2(10, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");
    const auto& p = s.sim.pop;
    int i = s.selected;
    if (i < 0 || i >= int(p.size()) || !p.alive[i]) {
        ImGui::TextDisabled("Click an object to inspect it.");
        ImGui::End();
        return;
    }
    Elements e = elementsAt(p, i, s.sim.nowJD);
    double alt = length(p.pos[i]) - RE, spd = length(p.vel[i]);
    ImGui::TextColored({0.4f, 1.0f, 0.5f, 1}, "%s", p.name[i].c_str());
    ImGui::Text("NORAD %d   %s", p.norad[i], kindName(p.kind[i]));
    ImGui::Separator();
    ImGui::Text("Altitude   %9.1f km", alt);
    ImGui::Text("Speed      %9.3f km/s", spd);
    ImGui::Text("Period     %9.2f min", periodS(e.a) / 60.0);
    ImGui::Text("a %.1f km  e %.5f  i %.3f deg", e.a, e.e, e.inc / DEG);
    ImGui::Text("RAAN %.3f  argp %.3f  M %.3f deg", e.raan / DEG, e.argp / DEG, e.M / DEG);
    ImGui::Text("Element epoch %s", jdToUTC(p.el[i].epochJD).c_str());
    const Conjunction* worst = nullptr;
    int worstIdx = -1;
    for (int k = 0; k < int(s.sim.alerts.size()); ++k) {
        const auto& a = s.sim.alerts[k];
        if (!a.passed && (a.a == i || a.b == i) && (!worst || a.missKm < worst->missKm)) { worst = &a; worstIdx = k; }
    }
    ImGui::Separator();
    if (worst) {
        int o = worst->a == i ? worst->b : worst->a;
        ImGui::TextColored(tierColor(worst->tier), "Nearest threat: %s", p.name[o].c_str());
        ImGui::Text("miss %.3f km at %s, %.2f km/s", worst->missKm, tminus(worst->tcaJD, s.sim.nowJD).c_str(), worst->relSpeedKms);
        if (ImGui::Button("Plan avoidance [A]")) { actFocusAlert(s, worstIdx); actPlan(s); }
    } else {
        ImGui::TextDisabled("No conjunctions in the look-ahead window.");
    }
    if (ImGui::Button(s.camera.mode == CamMode::Follow ? "Stop following" : "Follow [F]"))
        s.camera.mode = s.camera.mode == CamMode::Follow ? CamMode::Arcball : CamMode::Follow;
    ImGui::End();
}

static void conjunctionTable(AppState& s) {
    ImGui::SetNextWindowPos(ImVec2(float(s.winW - 440), 245), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Conjunctions");
    const char* filters[] = {">= YELLOW", ">= AMBER", "RED only"};
    int f = s.alertFilter - 1;  // filter is a Tier value 1..3
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("show", &f, filters, 3);
    s.alertFilter = f + 1;
    ImGui::SameLine();
    ImGui::TextDisabled("approx. Pc, assumed sigma");
    std::vector<int> rows;
    int minTier = s.alertFilter;
    for (int k = 0; k < int(s.sim.alerts.size()); ++k)
        if (!s.sim.alerts[k].passed && int(s.sim.alerts[k].tier) >= minTier) rows.push_back(k);
    std::sort(rows.begin(), rows.end(), [&](int a, int b) {
        const auto &x = s.sim.alerts[a], &y = s.sim.alerts[b];
        return x.tier != y.tier ? x.tier > y.tier : x.tcaJD < y.tcaJD;
    });
    ImGui::Text("%zu shown / %zu tracked", rows.size(), s.sim.alerts.size());
    if (ImGui::BeginTable("cj", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const char* h : {"Tier", "Objects", "TCA", "Miss km", "km/s", "Pc"}) ImGui::TableSetupColumn(h);
        ImGui::TableHeadersRow();
        int fi = s.focusIndex();
        ImGuiListClipper clip;
        clip.Begin(int(rows.size()));
        while (clip.Step())
            for (int r = clip.DisplayStart; r < clip.DisplayEnd; ++r) {
                int k = rows[r];
                const auto& a = s.sim.alerts[k];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(k);
                if (ImGui::Selectable(tierName(a.tier), k == fi, ImGuiSelectableFlags_SpanAllColumns)) actFocusAlert(s, k);
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextColored(tierColor(a.tier), "%.14s x %.14s", s.sim.pop.name[a.a].c_str(), s.sim.pop.name[a.b].c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", tminus(a.tcaJD, s.sim.nowJD).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", a.missKm);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", a.relSpeedKms);
                ImGui::TableNextColumn();
                ImGui::Text("%.1e", a.pc);
            }
        ImGui::EndTable();
    }
    ImGui::End();
}

static void plannerPanel(AppState& s) {
    if (!s.planShown) return;
    ImGui::SetNextWindowPos(ImVec2(340, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Avoidance Planner", &s.planShown);
    const auto& b = s.plan;
    const auto& p = s.sim.pop;
    if (b.obj >= 0) ImGui::Text("Maneuvering: %s", p.name[b.obj].c_str());
    if (b.threat >= 0) ImGui::Text("Threat:      %s", p.name[b.threat].c_str());
    ImGui::Separator();
    if (!b.valid) {
        ImGui::TextColored({1, 0.4f, 0.3f, 1}, "%s", b.note.c_str());
    } else {
        ImGui::TextColored({0.3f, 0.9f, 1, 1}, "%s burn  %.3f m/s", b.dvMps > 0 ? "PROGRADE" : "RETROGRADE", std::fabs(b.dvMps));
        ImGui::Text("Miss distance  %.3f km -> %.2f km", b.oldMissKm, b.newMissKm);
        ImGui::Text("New TCA        %s", tminus(b.newTcaJD, s.sim.nowJD).c_str());
        if (b.nearestOther >= 0)
            ImGui::Text("Closest 3rd    %.1f km (%s)", b.nearestOtherKm, p.name[b.nearestOther].c_str());
        else
            ImGui::Text("Closest 3rd    none within screening gate");
        ImGui::Text("Propellant     %.3f kg (Isp %.0f s, %.0f kg)", b.fuelKg, s.sim.cfg.ispS, s.sim.cfg.massKg);
        ImGui::Text("Semi-major axis %.3f -> %.3f km", b.before.a, b.after.a);
        ImGui::Text("Candidates tried: %d", b.candidatesTried);
        ImGui::TextDisabled("dashed = current orbit, solid = post-burn, magenta = transfer arc");
        if (ImGui::Button("Commit Burn [Enter]")) actCommit(s);
        ImGui::SameLine();
    }
    if (ImGui::Button("Discard")) actDiscardPlan(s);
    ImGui::End();
}

void drawPanels(AppState& s) {
    controlPanel(s);
    inspector(s);
    conjunctionTable(s);
    plannerPanel(s);
    if (s.showLab) drawAlgorithmLabPanel(s.lab, &s.showLab);
    if (!s.toast.empty() && s.realTime < s.toastUntil) {
        ImGui::SetNextWindowPos(ImVec2(s.winW * 0.5f, s.winH - 200.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("##toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(s.toast.c_str());
        ImGui::End();
    }
}

}  // namespace og
