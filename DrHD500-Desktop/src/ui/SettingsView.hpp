#pragma once

#include "core/SerialPort.hpp"

namespace DrHD {

class SettingsView {
public:
    static void render(SerialPort& serial);
};

} // namespace DrHD
