#include "DashboardView.hpp"
#include "data/SatelliteDB.hpp"
#include "data/TerrestrialDB.hpp"
#include "audio/ToneGenerator.hpp"
#include <imgui.h>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace DrHD {

static int selectedSatIdx = 0;
static int selectedTpIdx = 0;
static int selectedConvIdx = 0;
static int selectedUHFIdx = 0;
static int selectedVHFIdx = 0;
static int selectedVHFLowIdx = 0;
static int selectedCableIdx = 0;
static int selectedLnbVoltMode = 2; // 2 = 13V, 4 = 18V

void DashboardView::render(SerialPort& serial) {
    // Ensure serial driver is in SNR measures mode when viewing Dashboard
    if (serial.getWorkingBand() == WorkingBand::Satellite) {
        serial.setMode(CmdMode::SatSNR);
    } else {
        serial.setMode(CmdMode::TerSNR);
    }

    SignalMetrics metrics;
    HardwareTelemetry hw;
    std::vector<ConstellationPoint> constPts;
    SpectrumData spec;
    serial.getTelemetry(metrics, hw, constPts, spec);

    // Update audio tone generator with live SNR
    ToneGenerator::instance().updateSignal(metrics.snrDb, metrics.carrierLock);

    // 1. Hardware Status Bar
    renderHardwareStatusBar(hw, serial.getStatus());

    ImGui::Separator();
    ImGui::Spacing();

    // 2. Main 2-column layout: Left = Tuning, Right = Live Signal Meters
    ImGui::Columns(2, "DashboardColumns", true);
    ImGui::SetColumnWidth(0, 310.0f);

    // LEFT: Tuning & Band Controls
    renderTuningControls(serial);

    ImGui::NextColumn();

    // RIGHT: Signal Gauges & Telemetry
    renderSignalGauges(metrics, hw, serial.getTunedInfo(), constPts, serial);

    ImGui::Columns(1);
}

void DashboardView::renderHardwareStatusBar(const HardwareTelemetry& hw, ConnectionStatus status) {
    ImGui::BeginGroup();

    // Connection badge
    if (status == ConnectionStatus::Connected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 1.0f, 0.3f, 1.0f));
        ImGui::Text("● CONNECTED: Dr.HD 500 (%s)", hw.hardwareID.c_str());
        ImGui::PopStyleColor();
    } else if (status == ConnectionStatus::Simulation) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.1f, 1.0f));
        ImGui::Text("● SIMULATION MODE (Hardware Emulated)");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::Text("○ DISCONNECTED");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 30.0f);

    // Battery Indicator
    ImVec4 battColor = hw.batteryPercent > 50 ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f) :
                       hw.batteryPercent > 20 ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    ImGui::TextColored(battColor, "[BAT: %d%% (%d mV)]", hw.batteryPercent, hw.batteryMilliVolts);

    ImGui::SameLine(0.0f, 20.0f);

    // LNB Current Draw & Overload Warning
    bool lnbOverload = hw.lnbCurrentMilliAmps > 400;
    if (lnbOverload) {
        ImGui::TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "⚠️ LNB OVERLOAD: %d mA (%.1f V)", hw.lnbCurrentMilliAmps, hw.lnbVoltage);
    } else {
        ImGui::Text("⚡ LNB: %d mA (%.1f V)", hw.lnbCurrentMilliAmps, hw.lnbVoltage);
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("PSU: %.1f V", hw.psuVoltage);

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "FW: v%d | HW: %d", hw.swVersion, hw.hwVersion);

    ImGui::EndGroup();
}

void DashboardView::renderTuningControls(SerialPort& serial) {
    ImGui::BeginChild("TuningPanel", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "SIGNAL BAND SELECTION");
    ImGui::Separator();

    WorkingBand currentBand = serial.getWorkingBand();
    const char* bands[] = { 
        "Satellite (DVB-S/S2)", 
        "Terrestrial UHF (DVB-T2 / 470-862 MHz)", 
        "VHF Band III (174-230 MHz)", 
        "VHF Low / VLF & FM (47-108 MHz)", 
        "Cable TV (DVB-C / 106-466 MHz)" 
    };
    int bandIdx = static_cast<int>(currentBand);
    if (bandIdx > 4) bandIdx = 0;

    if (ImGui::Combo("Band", &bandIdx, bands, IM_ARRAYSIZE(bands))) {
        serial.setWorkingBand(static_cast<WorkingBand>(bandIdx));
    }

    ImGui::Spacing();
    ImGui::Separator();

    static float lastTunedFreq = -1.0f;
    float curTunedFreq = serial.getTunedInfo().frequencyMHz;

    if (currentBand == WorkingBand::Satellite) {
        // Satellite & Transponder Tuning
        const auto& sats = SatelliteDB::instance().getSatellites();
        const auto& convs = SatelliteDB::instance().getConverters();

        std::vector<const char*> satNames;
        for (const auto& s : sats) satNames.push_back(s.name.c_str());

        if (ImGui::Combo("Satellite", &selectedSatIdx, satNames.data(), satNames.size())) {
            selectedTpIdx = 0; // reset tp selection
        }

        const auto& currentSat = sats[selectedSatIdx];
        std::vector<std::string> tpDisplayStrings;
        std::vector<const char*> tpNames;
        for (const auto& tp : currentSat.transponders) {
            tpDisplayStrings.push_back(tp.getDisplay());
        }
        for (const auto& str : tpDisplayStrings) {
            tpNames.push_back(str.c_str());
        }

        if (selectedTpIdx >= static_cast<int>(tpNames.size())) selectedTpIdx = 0;
        ImGui::Combo("Transponder", &selectedTpIdx, tpNames.data(), tpNames.size());

        // LNB Converter
        std::vector<const char*> convNames;
        for (const auto& c : convs) convNames.push_back(c.name.c_str());
        ImGui::Combo("LNB Type", &selectedConvIdx, convNames.data(), convNames.size());

        // LNB Power / Voltage
        const char* lnbVoltages[] = { "0V (Off)", "5V", "13V (Vertical)", "13V + 22K", "18V (Horizontal)", "18V + 22K" };
        int voltOption = (selectedLnbVoltMode == 4) ? 4 : 2;
        if (ImGui::Combo("LNB Voltage", &voltOption, lnbVoltages, IM_ARRAYSIZE(lnbVoltages))) {
            selectedLnbVoltMode = voltOption;
        }

        // Apply tuning to serial driver
        const auto& activeTp = currentSat.transponders[selectedTpIdx];
        const auto& activeConv = convs[selectedConvIdx];
        int lof = (activeTp.frequencyMHz > 11700) ? activeConv.highFreqMHz : activeConv.lowFreqMHz;
        int ifFreq = std::abs(activeTp.frequencyMHz - lof);
        serial.setSatelliteTuningDetailed(
            currentSat.name,
            static_cast<float>(activeTp.frequencyMHz),
            ifFreq,
            activeTp.symbolRateKSps,
            activeTp.standard == DvbStandard::DvbS2,
            (activeTp.pol == Polarization::Horizontal) ? "H" : "V",
            selectedLnbVoltMode
        );

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Calculated IF: %d MHz", ifFreq);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "LOF Offset: %d MHz", lof);

    } else if (currentBand == WorkingBand::TerrestrialUHF) {
        // Terrestrial UHF European Channels (CH 21 - CH 69)
        const auto& uhfChannels = TerrestrialDB::instance().getUHFChannels();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "EUROPEAN UHF TV GRID (8 MHz)");
        
        if (std::abs(curTunedFreq - lastTunedFreq) > 0.5f) {
            lastTunedFreq = curTunedFreq;
            for (size_t i = 0; i < uhfChannels.size(); ++i) {
                if (std::abs(uhfChannels[i].frequencyMHz - curTunedFreq) < 1.0f) {
                    selectedUHFIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        bool chChanged = false;
        if (ImGui::Button("< Prev CH##TuningUHF") && selectedUHFIdx > 0) {
            selectedUHFIdx--;
            chChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next CH >##TuningUHF") && selectedUHFIdx < static_cast<int>(uhfChannels.size() - 1)) {
            selectedUHFIdx++;
            chChanged = true;
        }

        std::vector<std::string> chNames;
        std::vector<const char*> chLabels;
        for (const auto& ch : uhfChannels) {
            std::stringstream ss;
            ss << ch.name << " (" << std::fixed << std::setprecision(1) << ch.frequencyMHz << " MHz) - " << ch.modulation;
            chNames.push_back(ss.str());
        }
        for (const auto& str : chNames) chLabels.push_back(str.c_str());

        if (ImGui::Combo("Channel##UHF", &selectedUHFIdx, chLabels.data(), chLabels.size())) {
            chChanged = true;
        }

        if (chChanged) {
            const auto& activeCh = uhfChannels[selectedUHFIdx];
            lastTunedFreq = activeCh.frequencyMHz;
            serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialUHF, activeCh.name, activeCh.frequencyMHz, activeCh.bandwidthMHz, activeCh.isT2, false, activeCh.standard);
        }

        const auto& activeCh = uhfChannels[selectedUHFIdx];
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Center Frequency: %.1f MHz", activeCh.frequencyMHz);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Bandwidth: %d MHz (%s)", activeCh.bandwidthMHz, activeCh.standard.c_str());

    } else if (currentBand == WorkingBand::TerrestrialVHF) {
        // VHF Band III (CH 5 - 12 DVB-T & CH E5 - E8 Analog PAL B/G)
        const auto& vhfChannels = TerrestrialDB::instance().getVHFChannels();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "VHF BAND III (174 - 230 MHz)");

        if (std::abs(curTunedFreq - lastTunedFreq) > 0.5f) {
            lastTunedFreq = curTunedFreq;
            for (size_t i = 0; i < vhfChannels.size(); ++i) {
                if (std::abs(vhfChannels[i].frequencyMHz - curTunedFreq) < 1.0f) {
                    selectedVHFIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        bool chChanged = false;
        if (ImGui::Button("< Prev CH##TuningVHF") && selectedVHFIdx > 0) {
            selectedVHFIdx--;
            chChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next CH >##TuningVHF") && selectedVHFIdx < static_cast<int>(vhfChannels.size() - 1)) {
            selectedVHFIdx++;
            chChanged = true;
        }

        std::vector<std::string> chNames;
        std::vector<const char*> chLabels;
        for (const auto& ch : vhfChannels) {
            std::stringstream ss;
            ss << ch.name << " (" << std::fixed << std::setprecision(2) << ch.frequencyMHz << " MHz) - " << ch.standard;
            chNames.push_back(ss.str());
        }
        for (const auto& str : chNames) chLabels.push_back(str.c_str());

        if (ImGui::Combo("Channel##VHF", &selectedVHFIdx, chLabels.data(), chLabels.size())) {
            chChanged = true;
        }

        if (chChanged) {
            const auto& activeCh = vhfChannels[selectedVHFIdx];
            lastTunedFreq = activeCh.frequencyMHz;
            serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHF, activeCh.name, activeCh.frequencyMHz, activeCh.bandwidthMHz, activeCh.isT2, activeCh.isAnalog, activeCh.standard, activeCh.videoCarrierMHz, activeCh.audioCarrierMHz);
        }

        const auto& activeCh = vhfChannels[selectedVHFIdx];
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Frequency: %.2f MHz", activeCh.frequencyMHz);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Standard: %s (%s)", activeCh.standard.c_str(), activeCh.isAnalog ? "Analog PAL" : "Digital DVB");
        if (activeCh.isAnalog && activeCh.videoCarrierMHz > 0.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Vision: %.2fM | Sound: %.2fM", activeCh.videoCarrierMHz, activeCh.audioCarrierMHz);
        }

    } else if (currentBand == WorkingBand::TerrestrialVHFLow) {
        // VHF Low / VLF & FM (CH E2 - E4, CH R1 - R2, FM Radio)
        const auto& vhfLowChannels = TerrestrialDB::instance().getVHFLowChannels();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "VHF LOW / VLF & FM (47 - 108 MHz)");

        if (std::abs(curTunedFreq - lastTunedFreq) > 0.5f) {
            lastTunedFreq = curTunedFreq;
            for (size_t i = 0; i < vhfLowChannels.size(); ++i) {
                if (std::abs(vhfLowChannels[i].frequencyMHz - curTunedFreq) < 0.3f) {
                    selectedVHFLowIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        bool chChanged = false;
        if (ImGui::Button("< Prev CH##TuningVHFLow") && selectedVHFLowIdx > 0) {
            selectedVHFLowIdx--;
            chChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next CH >##TuningVHFLow") && selectedVHFLowIdx < static_cast<int>(vhfLowChannels.size() - 1)) {
            selectedVHFLowIdx++;
            chChanged = true;
        }

        std::vector<std::string> chNames;
        std::vector<const char*> chLabels;
        for (const auto& ch : vhfLowChannels) {
            std::stringstream ss;
            ss << ch.name << " (" << std::fixed << std::setprecision(2) << ch.frequencyMHz << " MHz) - " << ch.standard;
            chNames.push_back(ss.str());
        }
        for (const auto& str : chNames) chLabels.push_back(str.c_str());

        if (ImGui::Combo("Channel##VHFLow", &selectedVHFLowIdx, chLabels.data(), chLabels.size())) {
            chChanged = true;
        }

        if (chChanged) {
            const auto& activeCh = vhfLowChannels[selectedVHFLowIdx];
            lastTunedFreq = activeCh.frequencyMHz;
            serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHFLow, activeCh.name, activeCh.frequencyMHz, activeCh.bandwidthMHz, activeCh.isT2, activeCh.isAnalog, activeCh.standard, activeCh.videoCarrierMHz, activeCh.audioCarrierMHz);
        }

        const auto& activeCh = vhfLowChannels[selectedVHFLowIdx];
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Frequency: %.2f MHz", activeCh.frequencyMHz);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Transmission: %s", activeCh.standard.c_str());
        if (activeCh.isAnalog && activeCh.videoCarrierMHz > 0.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Vision: %.2fM | Sound: %.2fM", activeCh.videoCarrierMHz, activeCh.audioCarrierMHz);
        }

    } else { // CableTV
        // Cable TV S-Channels (S01 - S41, 106 - 466 MHz)
        const auto& cableChannels = TerrestrialDB::instance().getCableChannels();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "CABLE TV DVB-C (106 - 466 MHz)");

        if (std::abs(curTunedFreq - lastTunedFreq) > 0.5f) {
            lastTunedFreq = curTunedFreq;
            for (size_t i = 0; i < cableChannels.size(); ++i) {
                if (std::abs(cableChannels[i].frequencyMHz - curTunedFreq) < 1.0f) {
                    selectedCableIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        bool chChanged = false;
        if (ImGui::Button("< Prev CH##TuningCable") && selectedCableIdx > 0) {
            selectedCableIdx--;
            chChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next CH >##TuningCable") && selectedCableIdx < static_cast<int>(cableChannels.size() - 1)) {
            selectedCableIdx++;
            chChanged = true;
        }

        std::vector<std::string> chNames;
        std::vector<const char*> chLabels;
        for (const auto& ch : cableChannels) {
            std::stringstream ss;
            ss << ch.name << " (" << std::fixed << std::setprecision(1) << ch.frequencyMHz << " MHz) - " << ch.standard;
            chNames.push_back(ss.str());
        }
        for (const auto& str : chNames) chLabels.push_back(str.c_str());

        if (ImGui::Combo("Channel##Cable", &selectedCableIdx, chLabels.data(), chLabels.size())) {
            chChanged = true;
        }

        if (chChanged) {
            const auto& activeCh = cableChannels[selectedCableIdx];
            lastTunedFreq = activeCh.frequencyMHz;
            serial.setTerrestrialTuningDetailed(WorkingBand::CableTV, activeCh.name, activeCh.frequencyMHz, activeCh.bandwidthMHz, activeCh.isT2, false, activeCh.standard);
        }

        const auto& activeCh = cableChannels[selectedCableIdx];
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Center Frequency: %.1f MHz", activeCh.frequencyMHz);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Bandwidth: %d MHz (%s)", activeCh.bandwidthMHz, activeCh.standard.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Audio Beeper Control
    bool isMuted = ToneGenerator::instance().isMuted();
    if (ImGui::Checkbox("Audio Tone Beeper", &isMuted)) {
        ToneGenerator::instance().setMuted(!ToneGenerator::instance().isMuted());
    }
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Audio pitch increases with SNR");

    ImGui::EndChild();
}

void DashboardView::renderSignalGauges(
    const SignalMetrics& metrics,
    const HardwareTelemetry& hw,
    const TunedInfo& tp,
    const std::vector<ConstellationPoint>& constPts,
    SerialPort& serial
) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float availW = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("SignalMeterAndZapPanel", ImVec2(0, 0), false);

    // 1. Top Cyan Status Strip (TV 0V BTQ:100%)
    ImVec2 cyanPos = ImGui::GetCursorScreenPos();
    float cyanH = 24.0f;
    drawList->AddRectFilled(cyanPos, ImVec2(cyanPos.x + availW, cyanPos.y + cyanH), IM_COL32(0, 180, 235, 255));
    
    std::string stripText;
    if (tp.band == WorkingBand::Satellite) {
        stripText = "SAT " + tp.polarization + " BTQ:100%";
    } else if (tp.band == WorkingBand::TerrestrialUHF) {
        stripText = "TV UHF 0V BTQ:100%";
    } else if (tp.band == WorkingBand::TerrestrialVHF) {
        stripText = "TV VHF III 0V BTQ:100%";
    } else if (tp.band == WorkingBand::TerrestrialVHFLow) {
        stripText = "TV VHF LOW/FM 0V BTQ:100%";
    } else if (tp.band == WorkingBand::CableTV) {
        stripText = "CABLE DVB-C BTQ:100%";
    } else {
        stripText = "TV 0V BTQ:100%";
    }
    ImVec2 strSz = ImGui::CalcTextSize(stripText.c_str());
    drawList->AddText(ImVec2(cyanPos.x + (availW - strSz.x) * 0.5f, cyanPos.y + (cyanH - strSz.y) * 0.5f), IM_COL32(0, 0, 0, 255), stripText.c_str());
    ImGui::Dummy(ImVec2(availW, cyanH + 4.0f));

    // 2. Active Transponder / Mux Banner (474M CH21 GI:1/8 32K 2/3 64QAM 8M T2)
    ImVec2 bannerPos = ImGui::GetCursorScreenPos();
    float bannerH = 30.0f;
    drawList->AddRectFilled(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(18, 22, 28, 255));
    drawList->AddRect(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(50, 65, 80, 255));
    
    std::string bannerStr = tp.getBanner(metrics);
    ImVec2 bSz = ImGui::CalcTextSize(bannerStr.c_str());
    drawList->AddText(ImVec2(bannerPos.x + (availW - bSz.x) * 0.5f, bannerPos.y + (bannerH - bSz.y) * 0.5f), IM_COL32(245, 248, 250, 255), bannerStr.c_str());
    ImGui::Dummy(ImVec2(availW, bannerH + 4.0f));

    // 3. Transmission Type & Signal Intelligence Badge (Analog vs Digital)
    ImVec2 typePos = ImGui::GetCursorScreenPos();
    float typeH = 26.0f;
    bool isDigital = metrics.carrierLock || metrics.demodLock;
    bool isAnalog = tp.isAnalog || (!isDigital && metrics.pwrDbuV >= 26.0f);

    ImU32 typeBg = isDigital ? IM_COL32(12, 55, 25, 255) :
                   (isAnalog ? IM_COL32(65, 40, 10, 255) : IM_COL32(30, 35, 40, 255));
    ImU32 typeBorder = isDigital ? IM_COL32(35, 195, 75, 255) :
                       (isAnalog ? IM_COL32(235, 145, 25, 255) : IM_COL32(80, 90, 100, 255));
    drawList->AddRectFilled(typePos, ImVec2(typePos.x + availW, typePos.y + typeH), typeBg, 4.0f);
    drawList->AddRect(typePos, ImVec2(typePos.x + availW, typePos.y + typeH), typeBorder, 4.0f);

    std::string typeBadgeText;
    if (isDigital) {
        typeBadgeText = "🟢 DIGITAL TRANSMISSION: " + tp.standard + " (COFDM / QAM Multi-Carrier Locked)";
    } else if (tp.isAnalog) {
        typeBadgeText = "📻 ANALOG TRANSMISSION: " + tp.standard + " (Continuous RF Carrier)";
    } else if (metrics.pwrDbuV >= 26.0f) {
        typeBadgeText = "⚠️ UNLOCKED RF CARRIER: Continuous Power Detected (Analog / CW Carrier)";
    } else {
        typeBadgeText = "⚪ NO CARRIER / THERMAL NOISE (< 26 dBµV)";
    }
    ImVec2 tSz = ImGui::CalcTextSize(typeBadgeText.c_str());
    drawList->AddText(ImVec2(typePos.x + 12.0f, typePos.y + (typeH - tSz.y) * 0.5f), 
                      isDigital ? IM_COL32(140, 255, 160, 255) : (isAnalog ? IM_COL32(255, 215, 110, 255) : IM_COL32(180, 190, 200, 255)), 
                      typeBadgeText.c_str());
    ImGui::Dummy(ImVec2(availW, typeH + 6.0f));

    // 4. Giant Readout & Segmented Gauge
    ImGui::BeginGroup();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.60f, 0.05f, 1.0f));
    ImGui::SetWindowFontScale(1.8f);
    if (tp.isAnalog && !isDigital) {
        ImGui::Text("RF PWR, dBµV");
    } else {
        ImGui::Text("C/N, dB");
    }
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    char mainMeterStr[32];
    if (tp.isAnalog && !isDigital) {
        snprintf(mainMeterStr, sizeof(mainMeterStr), "%.1f", metrics.pwrDbuV);
    } else {
        snprintf(mainMeterStr, sizeof(mainMeterStr), "%.1f", metrics.snrDb);
    }
    ImGui::SameLine(availW - 130.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.58f, 0.05f, 1.0f));
    ImGui::SetWindowFontScale(2.8f);
    ImGui::Text("%s", mainMeterStr);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    // Segmented Horizontal Bar
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    float barW = availW - 8.0f;
    float barH = 34.0f;
    float segW = barW / 8.0f;
    float maxVal = (tp.isAnalog && !isDigital) ? 100.0f : 40.0f;
    float currentVal = (tp.isAnalog && !isDigital) ? 
        std::max(0.0f, std::min(100.0f, metrics.pwrDbuV)) : 
        std::max(0.0f, std::min(40.0f, metrics.snrDb));
    float threshold = (tp.isAnalog && !isDigital) ? 50.0f : 12.0f;

    // Segment boxes in orange
    for (int i = 0; i < 8; ++i) {
        float x0 = barPos.x + i * segW;
        float x1 = x0 + segW;
        drawList->AddRect(ImVec2(x0 + 1, barPos.y), ImVec2(x1 - 1, barPos.y + barH), IM_COL32(240, 130, 20, 255), 2.0f, 0, 2.0f);
    }

    // Red fill up to threshold
    float redEndVal = std::min(currentVal, threshold);
    if (redEndVal > 0.1f) {
        float rW = (redEndVal / maxVal) * barW;
        drawList->AddRectFilled(ImVec2(barPos.x + 2, barPos.y + 2), ImVec2(barPos.x + rW - 2, barPos.y + barH - 2), IM_COL32(245, 35, 20, 255));
    }

    // Bright green fill for link margin / good signal above threshold
    if (currentVal > threshold) {
        float gX0 = barPos.x + (threshold / maxVal) * barW;
        float gX1 = barPos.x + (currentVal / maxVal) * barW;
        drawList->AddRectFilled(ImVec2(gX0, barPos.y + 2), ImVec2(gX1 - 2, barPos.y + barH - 2), IM_COL32(0, 230, 60, 255));
    }

    ImGui::Dummy(ImVec2(barW, barH + 2.0f));

    // Numbers below bar
    float tickValsDigital[] = { 0.0f, 10.0f, 20.0f, 30.0f, 40.0f };
    float tickValsAnalog[] = { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f };
    float* tickVals = (tp.isAnalog && !isDigital) ? tickValsAnalog : tickValsDigital;
    for (int i = 0; i < 5; ++i) {
        float tv = tickVals[i];
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", tv);
        float tx = barPos.x + (tv / maxVal) * barW - (tv > 0 ? 8.0f : 0.0f);
        drawList->AddText(ImVec2(tx, barPos.y + barH + 3.0f), IM_COL32(240, 140, 30, 255), buf);
    }
    ImGui::Dummy(ImVec2(barW, 18.0f));

    ImGui::Spacing();

    // 5. Middle Section (Measures on Left, Scope/Constellation Box on Right)
    float midH = 205.0f;
    float colW = (availW - 16.0f) * 0.5f;

    // LEFT: Measures Column
    ImGui::BeginChild("MeasuresSubCol", ImVec2(colW, midH), false);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    ImGui::SetWindowFontScale(1.35f);

    auto renderMeasureRow = [&](const char* label, const char* valStr, bool isGreen = false) {
        ImGui::Text("%s", label);
        ImGui::SameLine(colW - 85.0f);
        if (isGreen) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", valStr);
        } else {
            ImGui::Text("%s", valStr);
        }
        ImGui::Spacing();
    };

    char valBuf[64];
    snprintf(valBuf, sizeof(valBuf), "%.0f", metrics.pwrDbuV);
    renderMeasureRow("RSSI, dBuV", valBuf);

    if (tp.isAnalog && !isDigital) {
        // Analog carrier specifics
        if (tp.videoCarrierMHz > 0.0f) {
            snprintf(valBuf, sizeof(valBuf), "%.2f", tp.videoCarrierMHz);
            renderMeasureRow("Vision, MHz", valBuf);

            snprintf(valBuf, sizeof(valBuf), "%.2f", tp.audioCarrierMHz);
            renderMeasureRow("Sound, MHz", valBuf);

            renderMeasureRow("Offset, MHz", "+5.50", true);
            renderMeasureRow("Video Mod", "AM-VSB");
            renderMeasureRow("Audio Mod", "FM Mono");
        } else {
            // FM radio or generic carrier
            snprintf(valBuf, sizeof(valBuf), "%.2f", tp.frequencyMHz);
            renderMeasureRow("Carrier, MHz", valBuf);
            renderMeasureRow("Demod", "W-FM (75k)");
            renderMeasureRow("De-emphasis", "50 us", true);
            renderMeasureRow("Multiplex", "Stereo MPX");
            renderMeasureRow("Pilot Tone", "19.0 kHz");
        }
    } else {
        // Digital DVB specifics
        snprintf(valBuf, sizeof(valBuf), "%.1f", metrics.snrDb);
        renderMeasureRow("C/N, dB", valBuf);

        snprintf(valBuf, sizeof(valBuf), "%.1f", metrics.merDb);
        renderMeasureRow("MER, dB", valBuf);

        if (metrics.hasLmCn) {
            snprintf(valBuf, sizeof(valBuf), "%.1f", metrics.lmCnDb);
            renderMeasureRow("LM c/n, dB", valBuf, true);
        } else {
            renderMeasureRow("LM c/n, dB", "-.-");
        }

        renderMeasureRow("LM mer, dB", "-.-");
        renderMeasureRow("PE", "-.-");
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 16.0f);

    // RIGHT: Mini Scope Box (Constellation for Digital / IRE Scope for Analog)
    ImGui::BeginChild("ConstellationSubCol", ImVec2(colW, midH), false);
    ImVec2 boxP0 = ImGui::GetCursorScreenPos();
    float boxDim = std::min(colW, midH - 10.0f);
    ImVec2 boxP1(boxP0.x + boxDim, boxP0.y + boxDim);

    drawList->AddRectFilled(boxP0, boxP1, IM_COL32(10, 14, 18, 255));
    drawList->AddRect(boxP0, boxP1, IM_COL32(50, 65, 80, 255));

    if (isDigital) {
        // Digital Constellation Box
        float midX = boxP0.x + boxDim * 0.5f;
        float midY = boxP0.y + boxDim * 0.5f;
        drawList->AddLine(ImVec2(boxP0.x, midY), ImVec2(boxP1.x, midY), IM_COL32(70, 85, 100, 200), 1.0f);
        drawList->AddLine(ImVec2(midX, boxP0.y), ImVec2(midX, boxP1.y), IM_COL32(70, 85, 100, 200), 1.0f);

        drawList->PushClipRect(boxP0, boxP1, true);
        float scaleR = boxDim * 0.44f;
        for (const auto& pt : constPts) {
            float sx = midX + pt.i * scaleR;
            float sy = midY - pt.q * scaleR;
            drawList->AddCircleFilled(ImVec2(sx, sy), 1.8f, IM_COL32(230, 255, 60, 220));
        }
        drawList->PopClipRect();

        drawList->AddText(ImVec2(boxP0.x + 6.0f, boxP0.y + 4.0f), IM_COL32(0, 210, 255, 200), "I/Q CONSTELLATION");
    } else {
        // Analog Video Waveform Monitor / Oscilloscope
        // Graticule horizontal lines (100 IRE, 50 IRE, 0 IRE blanking, -40 IRE sync tip)
        float y100 = boxP0.y + boxDim * 0.22f;
        float y50  = boxP0.y + boxDim * 0.44f;
        float y0   = boxP0.y + boxDim * 0.66f;
        float ySync= boxP0.y + boxDim * 0.88f;

        drawList->AddLine(ImVec2(boxP0.x, y100), ImVec2(boxP1.x, y100), IM_COL32(40, 70, 50, 180), 1.0f);
        drawList->AddLine(ImVec2(boxP0.x, y50),  ImVec2(boxP1.x, y50),  IM_COL32(35, 60, 45, 140), 1.0f);
        drawList->AddLine(ImVec2(boxP0.x, y0),   ImVec2(boxP1.x, y0),   IM_COL32(50, 95, 65, 220), 1.5f);
        drawList->AddLine(ImVec2(boxP0.x, ySync),ImVec2(boxP1.x, ySync),IM_COL32(40, 70, 50, 180), 1.0f);

        drawList->AddText(ImVec2(boxP0.x + 4.0f, y100 - 12.0f), IM_COL32(80, 160, 100, 180), "100 IRE");
        drawList->AddText(ImVec2(boxP0.x + 4.0f, y0 - 12.0f), IM_COL32(100, 200, 120, 220), "0 IRE (Blank)");
        drawList->AddText(ImVec2(boxP0.x + 4.0f, ySync - 12.0f), IM_COL32(80, 160, 100, 180), "-40 (Sync)");

        // Draw Oscilloscope Video Scanline trace
        if (metrics.pwrDbuV > 26.0f) {
            std::vector<ImVec2> wavePts;
            int numPts = 120;
            static float waveAnimPhase = 0.0f;
            waveAnimPhase += 0.05f;

            for (int k = 0; k < numPts; ++k) {
                float u = static_cast<float>(k) / static_cast<float>(numPts - 1);
                float wx = boxP0.x + u * boxDim;
                float wy = y0;

                if (u < 0.08f) {
                    // Front porch (0 IRE)
                    wy = y0;
                } else if (u < 0.20f) {
                    // Horizontal Sync pulse (-40 IRE)
                    wy = ySync;
                } else if (u < 0.32f) {
                    // Back porch with PAL color burst
                    float burst = std::sin(k * 1.8f + waveAnimPhase * 10.0f) * (boxDim * 0.06f);
                    wy = y0 + burst;
                } else {
                    // Active Video Luminance Steps (Color bar / Grey staircase)
                    float activeFrac = (u - 0.32f) / 0.68f;
                    int step = std::min(5, static_cast<int>(activeFrac * 6.0f));
                    float targetY = y0 - (static_cast<float>(step) / 5.0f) * (y0 - y100);
                    float rfNoise = ((rand() % 10) - 5) * 0.5f;
                    wy = targetY + rfNoise;
                }
                wavePts.push_back(ImVec2(wx, wy));
            }

            for (size_t k = 0; k + 1 < wavePts.size(); ++k) {
                drawList->AddLine(wavePts[k], wavePts[k + 1], IM_COL32(0, 255, 80, 235), 2.0f);
            }
            drawList->AddText(ImVec2(boxP0.x + 6.0f, boxP0.y + 4.0f), IM_COL32(0, 255, 120, 220), "ANALOG VIDEO SCOPE (IRE)");
        } else {
            // Low noise floor trace
            for (int k = 0; k < 60; ++k) {
                float x0 = boxP0.x + (static_cast<float>(k) / 59.0f) * boxDim;
                float yN = y0 + ((rand() % 10) - 5) * 0.6f;
                drawList->AddCircleFilled(ImVec2(x0, yN), 1.0f, IM_COL32(80, 120, 90, 160));
            }
            drawList->AddText(ImVec2(boxP0.x + 6.0f, boxP0.y + 4.0f), IM_COL32(150, 160, 170, 180), "ANALOG SCOPE (NO SIGNAL)");
        }
    }

    ImGui::Dummy(ImVec2(boxDim, boxDim));
    ImGui::EndChild();

    // 6. Bottom Section: Channel Zap / Multi-Band Service List
    ImVec2 divPos = ImGui::GetCursorScreenPos();
    drawList->AddLine(divPos, ImVec2(divPos.x + availW, divPos.y), IM_COL32(0, 180, 235, 255), 2.0f);
    ImGui::Dummy(ImVec2(availW, 6.0f));

    // Cell & Network info
    if (isDigital) {
        char netBuf[64];
        snprintf(netBuf, sizeof(netBuf), "CELL:%04X NET:%04X PLPN:%d", metrics.cellId, metrics.netId, metrics.plp);
        ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "%s", netBuf);
    } else if (tp.isAnalog) {
        if (tp.videoCarrierMHz > 0.0f) {
            ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "PAL B/G 625-LINES 50Hz (Vis: %.2f MHz | Sound: %.2f MHz)", tp.videoCarrierMHz, tp.audioCarrierMHz);
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "FM BROADCAST 88-108 MHz (W-FM 75kHz Deviation)");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CARRIER UNLOCKED (Searching Multiplex Framing...)");
    }

    // Fetch services for digital or show analog details
    std::vector<DvbService> svcList = metrics.services;
    if (svcList.empty() && tp.band == WorkingBand::TerrestrialUHF) {
        const auto* ch = TerrestrialDB::instance().findNearestUHF(tp.frequencyMHz);
        if (ch && !ch->services.empty()) {
            svcList = ch->services;
        }
    }

    if (isDigital) {
        ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "%s (services: %zu)", tp.standard.c_str(), svcList.size());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s (Analog Continuous Carrier)", tp.standard.c_str());
    }
    
    // Multi-band Quick step buttons
    ImGui::SameLine(availW - 180.0f);
    if (ImGui::Button("< Prev CH##Zap")) {
        if (tp.band == WorkingBand::TerrestrialUHF) {
            const auto& chs = TerrestrialDB::instance().getUHFChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i > 0) {
                    const auto& prevCh = chs[i - 1];
                    selectedUHFIdx = static_cast<int>(i - 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialUHF, prevCh.name, prevCh.frequencyMHz, prevCh.bandwidthMHz, prevCh.isT2, false, prevCh.standard);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::TerrestrialVHF) {
            const auto& chs = TerrestrialDB::instance().getVHFChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i > 0) {
                    const auto& prevCh = chs[i - 1];
                    selectedVHFIdx = static_cast<int>(i - 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHF, prevCh.name, prevCh.frequencyMHz, prevCh.bandwidthMHz, prevCh.isT2, prevCh.isAnalog, prevCh.standard, prevCh.videoCarrierMHz, prevCh.audioCarrierMHz);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::TerrestrialVHFLow) {
            const auto& chs = TerrestrialDB::instance().getVHFLowChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 0.3f && i > 0) {
                    const auto& prevCh = chs[i - 1];
                    selectedVHFLowIdx = static_cast<int>(i - 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHFLow, prevCh.name, prevCh.frequencyMHz, prevCh.bandwidthMHz, prevCh.isT2, prevCh.isAnalog, prevCh.standard, prevCh.videoCarrierMHz, prevCh.audioCarrierMHz);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::CableTV) {
            const auto& chs = TerrestrialDB::instance().getCableChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i > 0) {
                    const auto& prevCh = chs[i - 1];
                    selectedCableIdx = static_cast<int>(i - 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::CableTV, prevCh.name, prevCh.frequencyMHz, prevCh.bandwidthMHz, prevCh.isT2, false, prevCh.standard);
                    break;
                }
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next CH >##Zap")) {
        if (tp.band == WorkingBand::TerrestrialUHF) {
            const auto& chs = TerrestrialDB::instance().getUHFChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i + 1 < chs.size()) {
                    const auto& nextCh = chs[i + 1];
                    selectedUHFIdx = static_cast<int>(i + 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialUHF, nextCh.name, nextCh.frequencyMHz, nextCh.bandwidthMHz, nextCh.isT2, false, nextCh.standard);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::TerrestrialVHF) {
            const auto& chs = TerrestrialDB::instance().getVHFChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i + 1 < chs.size()) {
                    const auto& nextCh = chs[i + 1];
                    selectedVHFIdx = static_cast<int>(i + 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHF, nextCh.name, nextCh.frequencyMHz, nextCh.bandwidthMHz, nextCh.isT2, nextCh.isAnalog, nextCh.standard, nextCh.videoCarrierMHz, nextCh.audioCarrierMHz);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::TerrestrialVHFLow) {
            const auto& chs = TerrestrialDB::instance().getVHFLowChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 0.3f && i + 1 < chs.size()) {
                    const auto& nextCh = chs[i + 1];
                    selectedVHFLowIdx = static_cast<int>(i + 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHFLow, nextCh.name, nextCh.frequencyMHz, nextCh.bandwidthMHz, nextCh.isT2, nextCh.isAnalog, nextCh.standard, nextCh.videoCarrierMHz, nextCh.audioCarrierMHz);
                    break;
                }
            }
        } else if (tp.band == WorkingBand::CableTV) {
            const auto& chs = TerrestrialDB::instance().getCableChannels();
            for (size_t i = 0; i < chs.size(); ++i) {
                if (std::abs(chs[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i + 1 < chs.size()) {
                    const auto& nextCh = chs[i + 1];
                    selectedCableIdx = static_cast<int>(i + 1);
                    serial.setTerrestrialTuningDetailed(WorkingBand::CableTV, nextCh.name, nextCh.frequencyMHz, nextCh.bandwidthMHz, nextCh.isT2, false, nextCh.standard);
                    break;
                }
            }
        }
    }

    ImGui::Spacing();

    // Scrollable Table: Services if digital, or Analog Specification details if analog
    static int selectedServiceIdx = 0;
    ImGui::BeginChild("ZapListChild", ImVec2(availW, 0), true);

    if (isDigital) {
        for (size_t i = 0; i < svcList.size(); ++i) {
            const auto& svc = svcList[i];
            bool isSel = (selectedServiceIdx == static_cast<int>(i));
            
            ImGui::PushID(static_cast<int>(i));
            if (isSel) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.45f, 0.65f, 0.8f));
            }

            std::string sidDisp = svc.serviceProvider.empty() ? 
                "[" + svc.serviceType + "] sid:" + svc.sid : 
                svc.serviceProvider + " [" + svc.serviceType + "] sid:" + svc.sid;

            if (ImGui::Selectable(svc.serviceName.c_str(), isSel, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedServiceIdx = static_cast<int>(i);
            }
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "  %s", sidDisp.c_str());

            if (isSel) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
            ImGui::Separator();
        }
    } else {
        // Analog transmission parameters
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "ANALOG CHANNEL PARAMETERS");
        ImGui::Separator();

        if (tp.videoCarrierMHz > 0.0f) {
            ImGui::Text("Vision Carrier Frequency: %.2f MHz (AM Negative Video)", tp.videoCarrierMHz);
            ImGui::Text("Sound Carrier Frequency:  %.2f MHz (FM Mono / NICAM Stereo Subcarrier)", tp.audioCarrierMHz);
            ImGui::Text("Intercarrier Sound Offset: +5.50 MHz (CCIR System B/G standard)");
            ImGui::Text("Color Subcarrier:          4.433619 MHz (PAL Phase Alternating Line)");
            ImGui::Text("Active Video Lines:        625 Lines / 50 fields / 25 full frames");
            ImGui::Text("Channel Channel Width:     %d MHz", tp.bandwidthMHz);
            ImGui::Text("Current Carrier Power:     %.1f dBµV", metrics.pwrDbuV);
        } else {
            ImGui::Text("Carrier Frequency:         %.2f MHz", tp.frequencyMHz);
            ImGui::Text("Modulation:                Wideband FM (W-FM)");
            ImGui::Text("Peak Frequency Deviation:  ±75 kHz");
            ImGui::Text("Pre-emphasis Time Const:   50 µs (European Standard)");
            ImGui::Text("Multiplex Baseband:        19 kHz Stereo Pilot + 38 kHz (L-R) Subcarrier");
            ImGui::Text("Current Carrier Power:     %.1f dBµV", metrics.pwrDbuV);
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

} // namespace DrHD
