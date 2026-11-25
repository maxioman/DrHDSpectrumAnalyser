#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

namespace DrHD {

constexpr uint8_t PACKET_HEADER_CMD = 0x3D; // ASCII '=' (61)
constexpr uint8_t PACKET_HEADER_RX  = 0xFE; // 254
constexpr size_t  PACKET_RX_SIZE    = 104;

// Packet command modes
enum class CmdMode : uint8_t {
    SatSNR          = 0,
    TerSNR          = 1,
    CabSNR          = 2,
    Service         = 3,
    BlindScan       = 4,
    Diseqc          = 5,
    SpectrumRDA     = 6,
    SpectrumRDACal  = 7,
    SpectrumMXL     = 8,
    SpectrumMXLCal  = 9
};

enum class WorkingBand : uint8_t {
    Satellite = 0,
    TerrestrialUHF = 1,
    TerrestrialVHF = 2,       // VHF Band III (174 - 230 MHz)
    TerrestrialVHFLow = 3,    // VHF Low / VLF & FM (47 - 108 MHz)
    CableTV = 4               // Cable S-Channels (106 - 466 MHz)
};

enum class ModulationType : uint8_t {
    QPSK = 0,
    PSK8 = 1,
    QAM16 = 2,
    QAM64 = 3,
    QAM256 = 4
};

struct DvbService {
    std::string serviceName;      // e.g. "Sharjah TV"
    std::string serviceProvider;  // e.g. "SBA"
    std::string serviceType;      // e.g. "HDTV", "SDTV", "Radio"
    std::string sid;              // e.g. "044C"
    bool isPaid = false;
};

// Signal measurement telemetry matching official Dr.HD Finder APK
struct SignalMetrics {
    float snrDb = 0.0f;           // C/N in dB (e.g. 14.2)
    float pwrDbuV = 0.0f;         // RSSI in dBµV (e.g. 40 or 54)
    float merDb = 0.0f;           // MER in dB (e.g. 12.2)
    float ber = 0.0f;             // Pre-BER / CBER
    float vber = 4.0e-7f;         // VBER (e.g. 4.0e-7)
    float lmCnDb = 0.0f;          // Link Margin C/N (e.g. 2.2 dB)
    float lmMerDb = 0.0f;         // Link Margin MER
    bool hasLmCn = false;
    bool hasLmMer = false;
    uint32_t packetErrors = 0;
    bool carrierLock = false;
    bool demodLock = false;
    
    // Multiplex parameter flags from hardware stream
    std::string modulationName = "64QAM"; // "QPSK", "16QAM", "64QAM", "256-QAM"
    std::string codeRate = "2/3";         // "1/2", "3/5", "2/3", "3/4", "4/5", "5/6"
    std::string guardInterval = "1/8";    // "1/32", "1/16", "1/8", "1/4", "1/128", "19/128", "19/256"
    std::string fftSize = "32K";          // "2K", "8K", "16K", "32K"
    int cellId = 0x0000;
    int netId = 0x3085;
    int plp = 1;
    std::string networkName = "UAE DTT Network";
    std::vector<DvbService> services;

    std::chrono::steady_clock::time_point lastUpdate;
};

// Hardware voltages and battery status
struct HardwareTelemetry {
    int batteryMilliVolts = 7400;
    int batteryPercent = 75;
    bool isCharging = false;
    float lnbVoltage = 13.8f;
    int lnbCurrentMilliAmps = 145;
    float psuVoltage = 12.0f;
    bool lnbPowerEnabled = true;
    int hwVersion = 134;
    int swVersion = 49;
    std::string hardwareID = "01:AC:78:00:04:55";
    std::chrono::steady_clock::time_point lastUpdate;
};

// Constellation I/Q symbol point
struct ConstellationPoint {
    float i = 0.0f; // -1.0 to 1.0
    float q = 0.0f; // -1.0 to 1.0
    float age = 0.0f; // 0.0 (fresh) to 1.0 (expired)
    std::chrono::steady_clock::time_point timestamp;
};

// RF Spectrum Sweep
struct SpectrumData {
    std::vector<float> levels;       // 300 points in dBµV
    std::vector<float> maxHold;      // Max hold envelope
    std::vector<float> avgTrace;     // Moving average trace
    float centerFreqMHz = 1242.0f;
    int spanMHz = 400;
    int refLevelDbuV = 85;
    int markerIndex = -1;
    bool isTrackingPeak = true;
    int peakIndex = 0;
    float peakFreqMHz = 0.0f;
    float peakDbuV = 0.0f;
    std::chrono::steady_clock::time_point lastSweepTime;
};

class Protocol {
public:
    static uint8_t computeCRC8(const uint8_t* data, size_t length);
    static bool validateChecksum(const uint8_t* packet104);

    // Command packet builders
    static std::vector<uint8_t> makeMeasuresSatPacket(
        int mode,
        int intermediateFreqMHz,
        int symbolRateKSps,
        bool isS2 = false,
        int powerMode = 2 // 2 = 13V, 4 = 18V
    );

    static std::vector<uint8_t> makeMeasuresTerPacket(
        int mode,
        int frequencyMHz,
        bool isT2 = true,
        int bandwidthCode = 0, // 0 = 8MHz, 1 = 7MHz, 2 = 6MHz
        int powerMode = 0
    );

    static std::vector<uint8_t> makeSpectrumPacket(
        int mode,
        int spanBeginFreq,
        int span,
        int refLevel,
        int powerMode = 0,
        bool isRdaMode = true
    );

    static std::vector<uint8_t> makeServicePacket(int serviceMode = 0);
    static std::vector<uint8_t> makeDiseqcPacket(const std::vector<uint8_t>& rawCmd);

    // Packet parsers
    static void parseTelemetryPacket(
        const uint8_t* packet,
        SignalMetrics& metrics,
        HardwareTelemetry& telemetry,
        std::vector<ConstellationPoint>& constPoints,
        SpectrumData& spectrum
    );

private:
    static uint8_t calculateSpanCode(int span, bool isRda);
};

} // namespace DrHD
