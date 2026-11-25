#pragma once

#include <string>
#include <vector>

namespace DrHD {

enum class Polarization {
    Horizontal,
    Vertical,
    CircularLeft,
    CircularRight
};

enum class DvbStandard {
    DvbS,
    DvbS2
};

struct Transponder {
    int frequencyMHz;        // e.g. 10992
    Polarization pol;        // Horizontal / Vertical
    int symbolRateKSps;      // e.g. 27500
    DvbStandard standard;    // DVB-S / DVB-S2
    std::string fec;         // "3/4", "8/9"

    std::string getDisplay() const {
        return std::to_string(frequencyMHz) + " " +
               (pol == Polarization::Horizontal ? "H" : "V") + " " +
               std::to_string(symbolRateKSps / 1000) + "k " +
               (standard == DvbStandard::DvbS2 ? "S2" : "S");
    }
};

struct Satellite {
    std::string name;        // e.g. "Hotbird 13.0°E"
    std::string position;    // "13.0E"
    std::vector<Transponder> transponders;
};

struct LnbConverter {
    std::string name;        // "UNIVERSAL (9750/10600)"
    int lowFreqMHz;          // 9750
    int highFreqMHz;         // 10600
};

class SatelliteDB {
public:
    static const SatelliteDB& instance();

    const std::vector<Satellite>& getSatellites() const { return satellites_; }
    const std::vector<LnbConverter>& getConverters() const { return converters_; }

private:
    SatelliteDB();
    void initData();

    std::vector<Satellite> satellites_;
    std::vector<LnbConverter> converters_;
};

} // namespace DrHD
