#include "io/Scenario.h"
#include <algorithm>
#include <fstream>
#include "orbit/TLEParser.h"

namespace og {

double loadCatalog(Population& p, const std::string& path, int limit) {
    auto recs = loadTLEFile(resolvePath(path));
    if (recs.empty()) return 0;
    std::vector<double> epochs;
    double stride = (limit > 0 && size_t(limit) < recs.size()) ? double(recs.size()) / limit : 1.0;
    for (double k = 0; size_t(k) < recs.size(); k += stride) {
        const auto& r = recs[size_t(k)];
        p.add(r.name, r.norad, r.kind, r.el);
        epochs.push_back(r.el.epochJD);
    }
    std::nth_element(epochs.begin(), epochs.begin() + epochs.size() / 2, epochs.end());
    return epochs[epochs.size() / 2];
}

static Kind parseKind(const std::string& s) {
    if (s == "debris") return Kind::Debris;
    if (s == "rocket_body") return Kind::RocketBody;
    if (s == "fragment") return Kind::Fragment;
    return Kind::Payload;
}

// Builds a circular orbit that passes `missKm` radially above `target` exactly tcaS after epoch,
// in a plane of the requested inclination. Elements are back-propagated to the epoch so the
// J2 drift during the lead time is accounted for.
bool interceptElements(const Vec3& rT, double jdT, double epoch, double missKm, double incDeg, Elements& out,
                       std::string& err) {
    double tcaS = (jdT - epoch) * SEC_PER_DAY;
    Vec3 u = normalize(rT);
    double inc = incDeg * DEG;
    double rho = std::sqrt(u.x * u.x + u.y * u.y), phi = std::atan2(u.y, u.x);
    double s = -u.z * std::cos(inc) / (std::sin(inc) * rho);
    if (std::fabs(s) > 1.0) { err = "intercept inclination too low to reach the target latitude"; return false; }
    double raan = phi + std::asin(s);
    Vec3 h{std::sin(inc) * std::sin(raan), -std::sin(inc) * std::cos(raan), std::cos(inc)};
    Vec3 node{std::cos(raan), std::sin(raan), 0};
    double argLat = std::atan2(dot(cross(node, u), h), dot(node, u));
    Elements e;
    e.a = length(rT) + missKm; e.e = 0; e.inc = inc;
    double n, rd, ad;
    j2Rates(e, n, rd, ad);
    e.raan = wrapTwoPi(raan - rd * tcaS);
    e.argp = wrapTwoPi(-ad * tcaS);
    e.M = wrapTwoPi(argLat - n * tcaS);
    e.epochJD = epoch;
    out = e;
    return true;
}

bool loadScenario(const std::string& path, Simulation& sim, ScenarioInfo& info, std::string& err, bool includeCatalog) {
    std::ifstream f(resolvePath(path));
    if (!f) { err = "cannot open " + path; return false; }
    nlohmann::json j;
    try { j = nlohmann::json::parse(f, nullptr, true, true); }
    catch (const std::exception& e) { err = e.what(); return false; }

    if (j.contains("config")) applyConfigJson(j["config"], sim.cfg);
    sim.pop.clear();
    sim.alerts.clear();
    sim.events.clear();
    sim.kessler = KesslerStats();
    info = ScenarioInfo();
    info.name = j.value("name", "scenario");
    info.description = j.value("description", "");
    info.follow = j.value("follow", "");
    info.warp = j.value("warp", 0.0);
    if (j.contains("expect")) info.expect = j["expect"];

    double catalogEpoch = 0;
    if (includeCatalog && j.contains("catalog"))
        catalogEpoch = loadCatalog(sim.pop, j["catalog"].value("path", sim.cfg.catalog), j["catalog"].value("limit", 0));
    double epoch = catalogEpoch;
    if (j.contains("epoch_utc") && !parseUTC(j["epoch_utc"].get<std::string>(), epoch)) { err = "bad epoch_utc"; return false; }
    if (epoch == 0) { err = "scenario needs epoch_utc when no catalog is loaded"; return false; }
    info.epochJD = epoch;

    int autoId = 800000;
    for (auto& o : j.value("objects", nlohmann::json::array())) {
        Elements e;
        e.a = o.contains("alt_km") ? RE + o["alt_km"].get<double>() : o.value("a_km", RE + 500);
        e.e = o.value("e", 0.0);
        e.inc = o.value("i_deg", 0.0) * DEG;
        e.raan = o.value("raan_deg", 0.0) * DEG;
        e.argp = o.value("argp_deg", 0.0) * DEG;
        e.M = o.value("M_deg", 0.0) * DEG;
        e.epochJD = epoch;
        sim.pop.add(o.value("name", "OBJ"), o.value("norad", autoId++), parseKind(o.value("kind", "payload")), e);
    }
    for (auto& w : j.value("walker", nlohmann::json::array())) {
        int P = w.value("planes", 10), S = w.value("per_plane", 10), F = w.value("phasing", 1);
        int id = w.value("norad_base", autoId);
        std::string base = w.value("name", "SHELL");
        Kind k = parseKind(w.value("kind", "payload"));
        for (int pl = 0; pl < P; ++pl)
            for (int s = 0; s < S; ++s) {
                Elements e;
                e.a = RE + w.value("alt_km", 550.0);
                e.inc = w.value("i_deg", 53.0) * DEG;
                e.raan = wrapTwoPi(w.value("raan0_deg", 0.0) * DEG + TWO_PI * pl / P);
                e.M = wrapTwoPi(TWO_PI * s / S + TWO_PI * F * pl / (P * S));
                e.epochJD = epoch;
                sim.pop.add(base + "-" + std::to_string(pl) + "-" + std::to_string(s), id++, k, e);
            }
        autoId = id;
    }
    for (auto& x : j.value("intercepts", nlohmann::json::array())) {
        int t = sim.pop.findByName(x.value("target", ""));
        if (t < 0) { err = "intercept target not found: " + x.value("target", ""); return false; }
        double jdT = epoch + x.value("tca_s", 1200.0) / SEC_PER_DAY;
        Vec3 rT, vT;
        propagateOne(sim.pop, t, jdT, rT, vT);
        Elements e;
        if (!interceptElements(rT, jdT, epoch, x.value("miss_km", 0.3), x.value("i_deg", 70.0), e, err)) return false;
        sim.pop.add(x.value("name", "INTERCEPT"), x.value("norad", autoId++), parseKind(x.value("kind", "debris")), e);
    }
    if (sim.pop.size() == 0) { err = "scenario has no objects"; return false; }
    sim.start(epoch);
    return true;
}

}  // namespace og
