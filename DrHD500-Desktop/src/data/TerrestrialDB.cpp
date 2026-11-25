#include "TerrestrialDB.hpp"
#include <cmath>

namespace DrHD {

const TerrestrialDB& TerrestrialDB::instance() {
    static TerrestrialDB db;
    return db;
}

TerrestrialDB::TerrestrialDB() {
    initChannels();
}

void TerrestrialDB::initChannels() {
    // European UHF Channels 21 - 70 (8 MHz spacing, 474 MHz to 866 MHz)
    for (int ch = 21; ch <= 70; ++ch) {
        float f = 474.0f + static_cast<float>(ch - 21) * 8.0f;
        ChannelInfo info;
        info.name = "CH " + std::to_string(ch);
        info.number = ch;
        info.frequencyMHz = f;
        info.bandwidthMHz = 8;
        info.isT2 = true;
        info.modulation = "DVB-T2";
        info.netInfo = "CELL:0000 NET:3085 PLPN:1";

        if (ch == 21) {
            // Live services from photo_6231029323888530924_y.jpg
            info.services.push_back({"Sharjah TV", "SBA", "HDTV", "044C", false});
            info.services.push_back({"Sharqiya from Kalba", "SBA", "HDTV", "04B0", false});
            info.services.push_back({"Sharjah Sport", "SBA", "HDTV", "0514", false});
            info.services.push_back({"Sharjah Radio", "SBA", "Radio", "0578", false});
            info.services.push_back({"Sharjah Quran Radio", "SBA", "Radio", "05DC", false});
            info.services.push_back({"WOUSTAA TV", "SBA", "HDTV", "0640", false});
            info.services.push_back({"Pulse 95 Radio", "SBA", "Radio", "06A4", false});
        } else if (ch == 38) {
            // Live services from photo_6231029323888530926_y.jpg
            info.services.push_back({"Bahrain International", "SBA", "SDTV", "044C", false});
            info.services.push_back({"Bahrain Radio Services", "SBA", "SDTV", "04B0", false});
            info.services.push_back({"Bahrain TV", "SBA", "SDTV", "0514", false});
            info.services.push_back({"Holy Quran", "SBA", "Radio", "0578", false});
            info.services.push_back({"Bahrain Sport 1", "SBA", "SDTV", "05DC", false});
            info.services.push_back({"Bahrain Sport 2", "SBA", "SDTV", "0640", false});
        } else {
            // Default multiplex bouquet
            info.services.push_back({"DTT HD Program 1", "Broadcaster", "HDTV", "0101", false});
            info.services.push_back({"DTT HD Program 2", "Broadcaster", "HDTV", "0102", false});
            info.services.push_back({"DTT News", "Broadcaster", "SDTV", "0103", false});
            info.services.push_back({"DTT Radio 1", "Broadcaster", "Radio", "0201", false});
        }

        info.standard = "DVB-T2";
        info.isAnalog = false;
        uhfChannels_.push_back(info);
    }

    // European VHF Band III Channels 5 - 12 (7 MHz spacing, 174 MHz to 230 MHz)
    // Supports both DVB-T / DAB+ digital multiplexes and legacy Analog PAL B/G vision carriers
    struct VhfChDef {
        const char* name;
        int num;
        float centerFreq;
        float visionFreq;
        float soundFreq;
        bool isAnalog;
    };
    static const VhfChDef vhfDefs[] = {
        { "CH 05 (DVB-T / DAB+)",  5, 177.5f, 175.25f, 180.75f, false },
        { "CH 06 (DVB-T / DAB+)",  6, 184.5f, 182.25f, 187.75f, false },
        { "CH 07 (DVB-T / DAB+)",  7, 191.5f, 189.25f, 194.75f, false },
        { "CH 08 (DVB-T / DAB+)",  8, 198.5f, 196.25f, 201.75f, false },
        { "CH 09 (DVB-T / DAB+)",  9, 205.5f, 203.25f, 208.75f, false },
        { "CH 10 (DVB-T / DAB+)", 10, 212.5f, 210.25f, 215.75f, false },
        { "CH 11 (DVB-T / DAB+)", 11, 219.5f, 217.25f, 222.75f, false },
        { "CH 12 (DVB-T / DAB+)", 12, 226.5f, 224.25f, 229.75f, false },
        { "CH E5 (Analog PAL B/G)", 5, 177.5f, 175.25f, 180.75f, true },
        { "CH E6 (Analog PAL B/G)", 6, 184.5f, 182.25f, 187.75f, true },
        { "CH E7 (Analog PAL B/G)", 7, 191.5f, 189.25f, 194.75f, true },
        { "CH E8 (Analog PAL B/G)", 8, 198.5f, 196.25f, 201.75f, true }
    };
    for (const auto& vd : vhfDefs) {
        ChannelInfo info;
        info.name = vd.name;
        info.number = vd.num;
        info.frequencyMHz = vd.centerFreq;
        info.bandwidthMHz = 7;
        info.isT2 = false;
        info.isAnalog = vd.isAnalog;
        info.standard = vd.isAnalog ? "Analog PAL B/G" : "DVB-T";
        info.modulation = vd.isAnalog ? "AM-VSB + FM" : "64QAM";
        info.videoCarrierMHz = vd.visionFreq;
        info.audioCarrierMHz = vd.soundFreq;
        vhfChannels_.push_back(info);
    }

    // VHF Low / VLF & FM Broadcast Band (47 MHz to 108 MHz)
    // Covers Band I TV channels E2-E4 (Vision carriers 48.25-62.25 MHz) and FM Radio
    static const VhfChDef vhfLowDefs[] = {
        { "CH E2 (Analog PAL B/G)", 2, 51.5f,  48.25f, 53.75f, true },
        { "CH E3 (Analog PAL B/G)", 3, 58.5f,  55.25f, 60.75f, true },
        { "CH E4 (Analog PAL B/G)", 4, 65.5f,  62.25f, 67.75f, true },
        { "CH R1 (OIRT Band I)",    1, 53.0f,  49.75f, 56.25f, true },
        { "CH R2 (OIRT Band I)",    2, 62.5f,  59.25f, 65.75f, true },
        { "FM 88.0 MHz (Radio)",   88, 88.0f,   0.0f,  88.0f,  true },
        { "FM 92.0 MHz (Radio)",   92, 92.0f,   0.0f,  92.0f,  true },
        { "FM 96.0 MHz (Radio)",   96, 96.0f,   0.0f,  96.0f,  true },
        { "FM 100.0 MHz (Radio)", 100, 100.0f,  0.0f, 100.0f,  true },
        { "FM 104.0 MHz (Radio)", 104, 104.0f,  0.0f, 104.0f,  true },
        { "FM 108.0 MHz (Radio)", 108, 108.0f,  0.0f, 108.0f,  true }
    };
    for (const auto& vld : vhfLowDefs) {
        ChannelInfo info;
        info.name = vld.name;
        info.number = vld.num;
        info.frequencyMHz = vld.centerFreq;
        info.bandwidthMHz = (vld.num >= 88) ? 1 : 7;
        info.isT2 = false;
        info.isAnalog = true;
        info.standard = (vld.num >= 88) ? "FM Radio" : "Analog PAL B/G";
        info.modulation = (vld.num >= 88) ? "Wideband FM" : "AM-VSB + FM";
        info.videoCarrierMHz = vld.visionFreq;
        info.audioCarrierMHz = vld.soundFreq;
        vhfLowChannels_.push_back(info);
    }

    // European Cable TV Channels S01 - S41 (106 MHz to 466 MHz, DVB-C & Analog CATV)
    for (int i = 1; i <= 41; ++i) {
        ChannelInfo info;
        info.name = "S" + (i < 10 ? std::string("0") : std::string("")) + std::to_string(i);
        info.number = i;
        info.frequencyMHz = 106.0f + static_cast<float>(i) * 8.0f;
        info.bandwidthMHz = 8;
        info.isT2 = false;
        info.isAnalog = false;
        info.standard = "DVB-C";
        info.modulation = "64QAM";
        info.videoCarrierMHz = info.frequencyMHz - 1.25f;
        info.audioCarrierMHz = info.videoCarrierMHz + 5.5f;
        cableChannels_.push_back(info);
    }
}

const ChannelInfo* TerrestrialDB::findNearestUHF(float freqMHz, float maxToleranceMHz) const {
    const ChannelInfo* best = nullptr;
    float minDiff = maxToleranceMHz;

    for (const auto& ch : uhfChannels_) {
        float diff = std::abs(ch.frequencyMHz - freqMHz);
        if (diff < minDiff) {
            minDiff = diff;
            best = &ch;
        }
    }
    return best;
}

} // namespace DrHD
