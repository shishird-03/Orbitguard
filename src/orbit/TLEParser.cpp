#include "orbit/TLEParser.h"
#include <fstream>
#include <sstream>

namespace og {

double tleEpochToJD(int yy, double doy) {
    int year = yy < 57 ? 2000 + yy : 1900 + yy;
    return jdFromCalendar(year, 1, 1, 0, 0, 0) + (doy - 1.0);
}

Kind classifyName(const std::string& n) {
    if (n.find(" DEB") != std::string::npos) return Kind::Debris;
    if (n.find("R/B") != std::string::npos) return Kind::RocketBody;
    return Kind::Payload;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static double field(const std::string& line, int col1, int len) {  // 1-based column
    return std::stod(line.substr(col1 - 1, len));
}

std::vector<TLERecord> parseTLEText(const std::string& text) {
    std::vector<TLERecord> out;
    std::istringstream in(text);
    std::string line, name, l1;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line.size() >= 69 && line[0] == '1' && line[1] == ' ') { l1 = line; continue; }
        if (line.size() >= 69 && line[0] == '2' && line[1] == ' ' && !l1.empty()) {
            try {
                TLERecord r;
                r.norad = std::stoi(l1.substr(2, 5));
                r.name = name.empty() ? ("OBJ " + std::to_string(r.norad)) : name;
                r.kind = classifyName(r.name);
                int yy = std::stoi(l1.substr(18, 2));
                double doy = field(l1, 21, 12);
                r.el.epochJD = tleEpochToJD(yy, doy);
                r.el.inc = field(line, 9, 8) * DEG;
                r.el.raan = field(line, 18, 8) * DEG;
                r.el.e = std::stod("0." + line.substr(26, 7));
                r.el.argp = field(line, 35, 8) * DEG;
                r.el.M = field(line, 44, 8) * DEG;
                double revPerDay = field(line, 53, 11);
                double nRad = revPerDay * TWO_PI / SEC_PER_DAY;
                r.el.a = std::cbrt(MU / (nRad * nRad));
                if (r.el.e < 1.0 && r.el.a > RE) out.push_back(r);
            } catch (...) { /* malformed record: skip it */ }
            name.clear(); l1.clear();
            continue;
        }
        name = line;
    }
    return out;
}

std::vector<TLERecord> loadTLEFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return parseTLEText(ss.str());
}

}  // namespace og
