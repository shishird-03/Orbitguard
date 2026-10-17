#include "io/Config.h"
#include <filesystem>
#include <fstream>

namespace og {

std::string resolvePath(const std::string& rel) {
    namespace fs = std::filesystem;
    if (rel.empty() || fs::exists(rel)) return rel;
#ifdef ORBITGUARD_ROOT
    fs::path p = fs::path(ORBITGUARD_ROOT) / rel;
    if (fs::exists(p)) return p.string();
#endif
    return rel;
}

template <class T>
static void get(const nlohmann::json& j, const char* k, T& v) {
    if (j.contains(k)) v = j[k].get<T>();
}

static void getColor(const nlohmann::json& j, const char* k, double* c) {
    if (j.contains(k) && j[k].is_array() && j[k].size() == 3)
        for (int i = 0; i < 3; ++i) c[i] = j[k][i].get<double>();
}

void applyConfigJson(const nlohmann::json& root, Config& c) {
    if (root.contains("window")) {
        auto& w = root["window"]; get(w, "width", c.winW); get(w, "height", c.winH); get(w, "msaa", c.msaa);
    }
    if (root.contains("sim")) {
        auto& s = root["sim"];
        get(s, "tick_hz", c.tickHz); get(s, "warp_init", c.warpInit); get(s, "warp_max", c.warpMax);
        get(s, "max_ticks_per_frame", c.maxTicksPerFrame); get(s, "max_objects", c.maxObjects);
        get(s, "catalog", c.catalog);
    }
    if (root.contains("render")) {
        auto& r = root["render"];
        get(r, "earth_stacks", c.earthStacks); get(r, "earth_slices", c.earthSlices); get(r, "star_count", c.starCount);
        get(r, "point_size", c.pointSize); get(r, "mesh_lod_distance", c.meshLodDist); get(r, "mesh_scale", c.meshScale);
        get(r, "mesh_lod_max", c.meshLodMax); get(r, "trail_length", c.trailLength); get(r, "orbit_samples", c.orbitSamples);
        get(r, "station_cone_alt_km", c.stationConeAltKm);
    }
    if (root.contains("screening")) {
        auto& s = root["screening"];
        get(s, "slice_s", c.sliceS); get(s, "lookahead_s", c.lookaheadS); get(s, "cell_km", c.cellKm);
        get(s, "vrel_max_kms", c.vrelMaxKms); get(s, "margin_km", c.marginKm); get(s, "budget_ms", c.budgetMs);
        get(s, "red_km", c.redKm); get(s, "amber_km", c.amberKm); get(s, "yellow_km", c.yellowKm);
        get(s, "sigma_km", c.sigmaKm); get(s, "hard_body_km", c.hardBodyKm); get(s, "alert_expire_s", c.alertExpireS);
        get(s, "formation_kms", c.formationKms);
    }
    if (root.contains("planner")) {
        auto& p = root["planner"];
        get(p, "dv_min_mps", c.dvMinMps); get(p, "dv_max_mps", c.dvMaxMps); get(p, "dv_steps", c.dvSteps);
        get(p, "safe_km", c.safeKm); get(p, "sample_s", c.plannerSampleS); get(p, "isp_s", c.ispS); get(p, "mass_kg", c.massKg);
    }
    if (root.contains("kessler")) {
        auto& k = root["kessler"];
        get(k, "enabled", c.kesslerEnabled); get(k, "collision_km", c.collisionKm); get(k, "frag_k", c.fragK);
        get(k, "frag_min", c.fragMin); get(k, "frag_max", c.fragMax); get(k, "dispersion_kms", c.dispersionKms);
        get(k, "asat_vrel_kms", c.asatVrelKms); get(k, "reentry_alt_km", c.reentryAltKm); get(k, "seed", c.seed);
    }
    if (root.contains("hud")) {
        auto& h = root["hud"];
        get(h, "radar_range_km", c.radarRangeKm); get(h, "radar_px", c.radarPx); get(h, "map_w", c.mapW); get(h, "map_h", c.mapH);
    }
    if (root.contains("tour")) get(root["tour"], "duration_s", c.tourDurationS);
    if (root.contains("colors")) {
        auto& k = root["colors"];
        getColor(k, "background", c.colBackground); getColor(k, "payload", c.colPayload); getColor(k, "debris", c.colDebris);
        getColor(k, "rocket_body", c.colRocket); getColor(k, "fragment", c.colFragment); getColor(k, "selected", c.colSelected);
    }
    if (root.contains("stations")) {
        c.stations.clear();
        for (auto& s : root["stations"])
            c.stations.push_back({s.value("name", "GS"), s.value("lat_deg", 0.0), s.value("lon_deg", 0.0),
                                  s.value("alt_km", 0.0), s.value("mask_deg", 10.0)});
    }
}

bool loadConfig(const std::string& path, Config& cfg) {
    std::ifstream f(resolvePath(path));
    if (!f) return false;
    try {
        applyConfigJson(nlohmann::json::parse(f, nullptr, true, true), cfg);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace og
