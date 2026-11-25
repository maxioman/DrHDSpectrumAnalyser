#pragma once

#include "core/SerialPort.hpp"

namespace DrHD {

class DashboardView {
public:
    static void render(SerialPort& serial);

private:
    static void renderHardwareStatusBar(const HardwareTelemetry& hw, ConnectionStatus status);
    static void renderTuningControls(SerialPort& serial);
    static void renderSignalGauges(const SignalMetrics& metrics, const HardwareTelemetry& hw, const TunedInfo& tp, const std::vector<ConstellationPoint>& constPts, SerialPort& serial);
};

} // namespace DrHD
