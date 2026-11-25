#include "SatelliteDB.hpp"

namespace DrHD {

const SatelliteDB& SatelliteDB::instance() {
    static SatelliteDB db;
    return db;
}

SatelliteDB::SatelliteDB() {
    initData();
}

void SatelliteDB::initData() {
    // LNB Converters
    converters_ = {
        {"UNIVERSAL Ku (9750/10600 MHz)", 9750, 10600},
        {"STANDARD Ku (10000 MHz)", 10000, 10000},
        {"C-BAND (5150 MHz)", 5150, 5150},
        {"WIDEBAND (10410 MHz)", 10410, 10410}
    };

    // 1. Hotbird 13°E
    Satellite hotbird;
    hotbird.name = "Hotbird 13.0°E";
    hotbird.position = "13.0E";
    hotbird.transponders = {
        {10992, Polarization::Vertical, 27500, DvbStandard::DvbS, "2/3"},
        {11034, Polarization::Vertical, 27500, DvbStandard::DvbS, "3/4"},
        {11054, Polarization::Horizontal, 27500, DvbStandard::DvbS2, "5/6"},
        {11096, Polarization::Horizontal, 29900, DvbStandard::DvbS2, "3/4"},
        {11411, Polarization::Horizontal, 27500, DvbStandard::DvbS2, "3/4"},
        {11449, Polarization::Horizontal, 27500, DvbStandard::DvbS2, "3/4"}
    };
    satellites_.push_back(hotbird);

    // 2. Astra 19.2°E
    Satellite astra19;
    astra19.name = "Astra 19.2°E";
    astra19.position = "19.2E";
    astra19.transponders = {
        {11836, Polarization::Horizontal, 27500, DvbStandard::DvbS, "3/4"},
        {11954, Polarization::Horizontal, 27500, DvbStandard::DvbS, "3/4"},
        {12188, Polarization::Horizontal, 27500, DvbStandard::DvbS, "3/4"},
        {12226, Polarization::Horizontal, 27500, DvbStandard::DvbS, "3/4"}
    };
    satellites_.push_back(astra19);

    // 3. Turksat 42°E
    Satellite turksat;
    turksat.name = "Türksat 42.0°E";
    turksat.position = "42.0E";
    turksat.transponders = {
        {12015, Polarization::Horizontal, 27500, DvbStandard::DvbS2, "5/6"},
        {12054, Polarization::Horizontal, 27500, DvbStandard::DvbS, "5/6"},
        {12245, Polarization::Horizontal, 27500, DvbStandard::DvbS2, "5/6"},
        {12380, Polarization::Vertical, 30000, DvbStandard::DvbS, "3/4"}
    };
    satellites_.push_back(turksat);

    // 4. Nilesat 7°W
    Satellite nilesat;
    nilesat.name = "Nilesat 7.0°W";
    nilesat.position = "7.0W";
    nilesat.transponders = {
        {11747, Polarization::Vertical, 27500, DvbStandard::DvbS, "5/6"},
        {11766, Polarization::Horizontal, 27500, DvbStandard::DvbS, "5/6"},
        {11938, Polarization::Vertical, 27500, DvbStandard::DvbS, "3/4"}
    };
    satellites_.push_back(nilesat);
}

} // namespace DrHD
