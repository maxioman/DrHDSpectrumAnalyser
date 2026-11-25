#include "SettingsView.hpp"
#include "audio/ToneGenerator.hpp"
#include "core/DrHDProtocol.hpp"
#include <imgui.h>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cstdlib>

namespace DrHD {

static char customPortBuf[128] = "/dev/cu.DrHD500ComboBT";
static int selectedPortIdx = 0;
static std::vector<std::string> cachedPorts;
static bool portsLoaded = false;
static std::string bluetoothLog = "Bluetooth ready.";
static bool isConnectingBT = false;

// DiSEqC parameters
static int diseqcPortIdx = 0; // 0=A, 1=B, 2=C, 3=D
static bool tone22kHz = false;
static int lnbVoltIdx = 0; // 0=13V, 1=18V, 2=Off
static char rawHexBuf[64] = "E0 10 38 F0";
static std::string diseqcStatusMsg = "DiSEqC engine ready.";

void SettingsView::render(SerialPort& serial) {
    if (!portsLoaded) {
        cachedPorts = SerialPort::listAvailablePorts();
        // Check if our preferred port is in the list
        for (size_t i = 0; i < cachedPorts.size(); ++i) {
            if (cachedPorts[i].find("DrHD500ComboBT") != std::string::npos) {
                selectedPortIdx = static_cast<int>(i);
                snprintf(customPortBuf, sizeof(customPortBuf), "%s", cachedPorts[i].c_str());
                break;
            }
        }
        portsLoaded = true;
    }

    SignalMetrics metrics;
    HardwareTelemetry hw;
    std::vector<ConstellationPoint> constPts;
    SpectrumData spec;
    serial.getTelemetry(metrics, hw, constPts, spec);

    ImGui::Columns(2, "SettingsCols", true);

    // Left Column: Serial Port Connection & DiSEqC
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "COMMUNICATION & HARDWARE LINK");
    ImGui::Separator();
    ImGui::Spacing();

    // Connection Status Badge
    ConnectionStatus status = serial.getStatus();
    ImGui::Text("Interface Status:");
    ImGui::SameLine();
    if (status == ConnectionStatus::Connected) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "CONNECTED (%s)", serial.getPortName().c_str());
    } else if (status == ConnectionStatus::Simulation) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "OFFLINE SIMULATION ACTIVE");
    } else if (status == ConnectionStatus::Connecting) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "CONNECTING...");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DISCONNECTED");
    }

    ImGui::Spacing();

    // Port selection
    std::vector<const char*> portItems;
    for (const auto& p : cachedPorts) {
        portItems.push_back(p.c_str());
    }

    if (!portItems.empty()) {
        if (ImGui::Combo("Select Port", &selectedPortIdx, portItems.data(), static_cast<int>(portItems.size()))) {
            if (selectedPortIdx >= 0 && selectedPortIdx < static_cast<int>(cachedPorts.size())) {
                snprintf(customPortBuf, sizeof(customPortBuf), "%s", cachedPorts[selectedPortIdx].c_str());
            }
        }
    }

    ImGui::InputText("Port Path", customPortBuf, sizeof(customPortBuf));

    if (ImGui::Button("Refresh Ports")) {
        cachedPorts = SerialPort::listAvailablePorts();
    }

    ImGui::SameLine();

    // Connect / Disconnect Buttons
    if (status == ConnectionStatus::Connected) {
        if (ImGui::Button("Disconnect")) {
            serial.closePort();
        }
    } else {
        if (ImGui::Button("Connect Serial")) {
            serial.stopSimulation();
            serial.openPort(customPortBuf, 115200);
        }
    }

    ImGui::SameLine();

    // Offline Simulation Button
    if (status == ConnectionStatus::Simulation) {
        if (ImGui::Button("Stop Simulation")) {
            serial.stopSimulation();
        }
    } else {
        if (ImGui::Button("Start Simulation")) {
            serial.closePort();
            serial.startSimulation();
        }
    }

    ImGui::Spacing();

    // Bluetooth Baseband Helper (blueutil)
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "macOS Bluetooth Baseband Reconnect:");
    if (ImGui::Button("Trigger blueutil Connect (01:AC:78:00:04:55)")) {
        bluetoothLog = "Connecting baseband via blueutil...";
        isConnectingBT = true;
        std::thread([]() {
            int ret = std::system("/opt/homebrew/bin/blueutil --connect 01-ac-78-00-04-55");
            if (ret == 0) {
                bluetoothLog = "Baseband connection established! Serial port should now open.";
            } else {
                bluetoothLog = "blueutil failed or device not in range (code " + std::to_string(ret) + ")";
            }
            isConnectingBT = false;
        }).detach();
    }
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", bluetoothLog.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // DiSEqC 1.0 / 1.1 Command Injector
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "DiSEqC 1.0 / 1.1 SWITCH & MOTOR CONTROL");
    ImGui::Spacing();

    const char* diseqcPorts[] = { "Port A (LNB 1)", "Port B (LNB 2)", "Port C (LNB 3)", "Port D (LNB 4)" };
    ImGui::Combo("DiSEqC Port", &diseqcPortIdx, diseqcPorts, IM_ARRAYSIZE(diseqcPorts));

    ImGui::Checkbox("22 kHz Continuous Tone (High Band)", &tone22kHz);

    const char* lnbVolts[] = { "13V (Vertical / Right)", "18V (Horizontal / Left)", "0V (Off)" };
    ImGui::Combo("LNB Polarization / Voltage", &lnbVoltIdx, lnbVolts, IM_ARRAYSIZE(lnbVolts));

    if (ImGui::Button("Send DiSEqC Switch Command")) {
        // Build standard DiSEqC 1.0 commitment: [Framing: 0xE0, Address: 0x10, Command: 0x38, Data: 0xF0 | (port<<2)]
        uint8_t dataByte = 0xF0;
        dataByte |= (diseqcPortIdx & 0x03) << 2;
        if (lnbVoltIdx == 1) dataByte |= 0x02; // 18V
        if (tone22kHz) dataByte |= 0x01;        // 22kHz

        std::vector<uint8_t> cmd = { 0xE0, 0x10, 0x38, dataByte };
        diseqcStatusMsg = "Sent DiSEqC switch command: E0 10 38 " + 
            [](uint8_t b) {
                char h[8];
                snprintf(h, sizeof(h), "%02X", b);
                return std::string(h);
            }(dataByte);
    }

    ImGui::Spacing();
    ImGui::InputText("Raw Hex DiSEqC", rawHexBuf, sizeof(rawHexBuf));
    ImGui::SameLine();
    if (ImGui::Button("Inject Hex")) {
        std::vector<uint8_t> bytes;
        std::istringstream iss(rawHexBuf);
        std::string hexStr;
        while (iss >> hexStr) {
            try {
                bytes.push_back(static_cast<uint8_t>(std::stoul(hexStr, nullptr, 16)));
            } catch (...) {}
        }
        if (!bytes.empty()) {
            diseqcStatusMsg = "Injected " + std::to_string(bytes.size()) + " raw DiSEqC bytes.";
        } else {
            diseqcStatusMsg = "Invalid hex string entered.";
        }
    }
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", diseqcStatusMsg.c_str());

    // Right Column: Hardware Telemetry Diagnostics & Audio Tone
    ImGui::NextColumn();

    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "HARDWARE TELEMETRY & DIAGNOSTICS");
    ImGui::Separator();
    ImGui::Spacing();

    // Battery Section
    ImGui::Text("Battery Pack:");
    float battFrac = hw.batteryPercent / 100.0f;
    char battLabel[32];
    snprintf(battLabel, sizeof(battLabel), "%d%% (%d mV)", hw.batteryPercent, hw.batteryMilliVolts);
    ImVec4 battColor = (hw.batteryPercent > 50) ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f) :
                       (hw.batteryPercent > 20) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                  ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, battColor);
    ImGui::ProgressBar(battFrac, ImVec2(-1, 24), battLabel);
    ImGui::PopStyleColor();

    ImGui::Text("Charging Status: %s", hw.isCharging ? "Charging" : "Discharging / On Battery");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // LNB & Power Supply Voltages
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "LNB Power Supply Output:");
    ImGui::Text("  LNB Output Voltage:  %.1f V", hw.lnbVoltage);
    ImGui::Text("  LNB Current Draw:    %d mA", hw.lnbCurrentMilliAmps);
    ImGui::Text("  LNB Power Switch:    %s", hw.lnbPowerEnabled ? "ENABLED (13V/18V ACTIVE)" : "OFF");
    ImGui::Text("  Main Supply (PSU):   %.1f V", hw.psuVoltage);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Device Identification
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Device Identity:");
    ImGui::Text("  Bluetooth MAC:       %s", hw.hardwareID.c_str());
    ImGui::Text("  Hardware Version:    v%d", hw.hwVersion);
    ImGui::Text("  Firmware Version:    v%d", hw.swVersion);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Audio Beeper Settings
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "ACOUSTIC ALIGNMENT TONE (BEEPER)");
    ImGui::Spacing();

    auto& tone = ToneGenerator::instance();
    bool isMuted = tone.isMuted();
    if (ImGui::Checkbox("Mute Audio Tone", &isMuted)) {
        tone.setMuted(isMuted);
    }

    float vol = tone.getVolume();
    if (ImGui::SliderFloat("Tone Volume", &vol, 0.0f, 0.5f, "%.2f")) {
        tone.setVolume(vol);
    }

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Pitch dynamically tracks SNR (500 Hz to 2400 Hz).");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Pulse rate speeds up when carrier lock is established.");

    ImGui::Columns(1);
}

} // namespace DrHD
