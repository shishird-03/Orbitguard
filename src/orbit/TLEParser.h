// Two-Line Element parser (fixed-column NORAD format).
#pragma once
#include <string>
#include <vector>
#include "orbit/Propagator.h"

namespace og {

struct TLERecord {
    std::string name;
    int norad = 0;
    Kind kind = Kind::Payload;
    Elements el;
};

double tleEpochToJD(int yy, double dayOfYear);
Kind classifyName(const std::string& name);
std::vector<TLERecord> parseTLEText(const std::string& text);
std::vector<TLERecord> loadTLEFile(const std::string& path);

}  // namespace og
