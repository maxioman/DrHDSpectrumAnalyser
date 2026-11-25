#pragma once

#include "core/DrHDProtocol.hpp"
#include <string>
#include <vector>

namespace DrHD {

struct ChannelInfo {
    std::string name;          // e.g. "CH 21", "CH 5", "CH E2", "FM 92.0"
    int number;                // 21
    float frequencyMHz;        // 474.0 (Center or Vision carrier)
    int bandwidthMHz;          // 8 (or 7 for VHF, 0.3 for FM)
    bool isT2 = false;         // true for DVB-T2
    bool isAnalog = false;     // true for Analog TV / FM radio
    std::string standard;      // "DVB-T2", "DVB-T", "DVB-C", "Analog PAL B/G", "FM Radio"
    float videoCarrierMHz = 0.0f; // Vision carrier for analog TV
    float audioCarrierMHz = 0.0f; // Sound carrier for analog TV
    std::string modulation = "DVB-T2"; // "64QAM", "256QAM", "AM-VSB + FM", "W-FM"
    std::string netInfo = "CELL:0000 NET:3085 PLPN:1";
    std::vector<DvbService> services;
};

struct BandGroup {
    std::string bandName; // "UHF", "VHF Band III", "VHF Low / FM", "Cable TV"
    std::vector<ChannelInfo> channels;
};

class TerrestrialDB {
public:
    static const TerrestrialDB& instance();

    const std::vector<ChannelInfo>& getUHFChannels() const { return uhfChannels_; }
    const std::vector<ChannelInfo>& getVHFChannels() const { return vhfChannels_; }
    const std::vector<ChannelInfo>& getVHFLowChannels() const { return vhfLowChannels_; }
    const std::vector<ChannelInfo>& getCableChannels() const { return cableChannels_; }

    const ChannelInfo* findNearestUHF(float freqMHz, float maxToleranceMHz = 4.0f) const;

private:
    TerrestrialDB();
    void initChannels();

    std::vector<ChannelInfo> uhfChannels_;
    std::vector<ChannelInfo> vhfChannels_;
    std::vector<ChannelInfo> vhfLowChannels_;
    std::vector<ChannelInfo> cableChannels_;
};

} // namespace DrHD
