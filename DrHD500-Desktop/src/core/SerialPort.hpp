#pragma once

#include "DrHDProtocol.hpp"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace DrHD {

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Simulation
};

struct TunedInfo {
    WorkingBand band = WorkingBand::TerrestrialUHF;
    std::string channelOrSatName = "CH 21 (474.0 MHz)";
    std::string channelNameOnly = "CH21";
    float frequencyMHz = 474.0f;
    int bandwidthMHz = 8;
    std::string standard = "DVB-T2";
    std::string polarization = "";
    int symbolRateKSps = 0;
    int ifFreqMHz = 474;
    bool isAnalog = false;
    float videoCarrierMHz = 0.0f;
    float audioCarrierMHz = 0.0f;

    std::string getBanner(const SignalMetrics& metrics) const {
        if (band == WorkingBand::Satellite) {
            return std::to_string(static_cast<int>(frequencyMHz)) + "M " + polarization + " " + std::to_string(symbolRateKSps) + "k " + standard;
        }
        if (isAnalog) {
            std::string s = channelNameOnly + " (" + standard + ") ";
            char fbuf[32];
            snprintf(fbuf, sizeof(fbuf), "%.2fM", frequencyMHz);
            s += fbuf;
            if (videoCarrierMHz > 0.0f) {
                char vbuf[64];
                snprintf(vbuf, sizeof(vbuf), " [Vis:%.2fM Aud:%.2fM]", videoCarrierMHz, audioCarrierMHz);
                s += vbuf;
            }
            return s;
        }
        std::string s = std::to_string(static_cast<int>(frequencyMHz)) + "M ";
        if (!channelNameOnly.empty()) s += channelNameOnly + " ";
        else s += "CH" + std::to_string(static_cast<int>((frequencyMHz - 474.0f) / 8.0f + 21)) + " ";
        if (!metrics.guardInterval.empty()) s += "GI:" + metrics.guardInterval + " ";
        if (!metrics.fftSize.empty()) s += metrics.fftSize + " ";
        if (!metrics.codeRate.empty()) s += metrics.codeRate + " ";
        if (!metrics.modulationName.empty()) s += metrics.modulationName + " ";
        s += std::to_string(bandwidthMHz) + "M ";
        s += (standard.find("T2") != std::string::npos ? "T2" : (standard.find("DVB-C") != std::string::npos ? "C" : "T"));
        return s;
    }
};

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    bool openPort(const std::string& portName, int baudRate = 115200);
    void closePort();
    bool isConnected() const { return status_ == ConnectionStatus::Connected; }
    bool isSimulating() const { return status_ == ConnectionStatus::Simulation; }
    ConnectionStatus getStatus() const { return status_; }
    std::string getPortName() const { return portName_; }

    void startSimulation();
    void stopSimulation();

    // Command configuration
    void setWorkingBand(WorkingBand band);
    WorkingBand getWorkingBand() const { return band_; }

    void setSatelliteTuning(int ifFreqMHz, int symbolRateKSps, bool isS2, int powerMode = 2);
    void setSatelliteTuningDetailed(const std::string& satName, float rfFreqMHz, int ifFreqMHz, int symbolRateKSps, bool isS2, const std::string& pol, int powerMode = 2);
    void setTerrestrialTuning(int freqMHz, bool isT2, int bwCode = 0, int powerMode = 0);
    void setTerrestrialTuningDetailed(WorkingBand band, const std::string& chName, float freqMHz, int bwMHz, bool isT2, bool isAnalog, const std::string& standard, float vidMHz = 0.0f, float audMHz = 0.0f);
    void setSpectrumTuning(int spanBegin, int span, int refLevel);

    void setMode(CmdMode mode) { currentMode_ = mode; }
    CmdMode getMode() const { return currentMode_; }

    TunedInfo getTunedInfo() const { return tunedInfo_; }

    // Direct access to parsed states (thread-safe copy)
    void getTelemetry(SignalMetrics& metrics, HardwareTelemetry& hw, std::vector<ConstellationPoint>& constPts, SpectrumData& spec);
    void clearConstellation();

    // Available serial ports on system
    static std::vector<std::string> listAvailablePorts();

private:
    void workerThreadLoop();
    void readerThreadLoop();
    void simulationTick();
    void sendCommandPacket();
    void handleIncomingBytes(const uint8_t* data, size_t length);

    int fd_ = -1;
    bool isUsingNativeBT_ = false;
    std::string portName_ = "/dev/cu.DrHD500ComboBT";
    int baudRate_ = 115200;
    std::atomic<ConnectionStatus> status_{ConnectionStatus::Disconnected};
    std::atomic<bool> shouldStop_{false};
    std::thread workerThread_;
    std::thread readerThread_;

    // Current Tuning Parameters
    WorkingBand band_ = WorkingBand::TerrestrialUHF;
    CmdMode currentMode_ = CmdMode::TerSNR;
    
    // Sat
    int satIfFreq_ = 1242; // 10992 - 9750
    int satSymbolRate_ = 27500;
    bool satIsS2_ = false;
    int satPowerMode_ = 2; // 13V

    // Ter
    int terFreqMHz_ = 474;
    bool terIsT2_ = true;
    int terBwCode_ = 0; // 8MHz
    int terPowerMode_ = 0;

    // Spectrum (Default: Full European UHF Band 250 - 950 MHz, Span 700 MHz, Ref Level 50 dBuV)
    int specSpanBegin_ = 250;
    int specSpan_ = 700;
    int specRefLevel_ = 50;

    // State data protected by mutex
    std::mutex dataMutex_;
    TunedInfo tunedInfo_;
    SignalMetrics metrics_;
    HardwareTelemetry telemetry_;
    std::vector<ConstellationPoint> constPoints_;
    SpectrumData spectrum_;

    // Framer buffer
    std::vector<uint8_t> rxBuffer_;
    uint32_t tickCounter_ = 0;
};

} // namespace DrHD
