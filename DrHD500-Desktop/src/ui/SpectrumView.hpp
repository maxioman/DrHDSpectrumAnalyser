#pragma once

#include "core/SerialPort.hpp"

namespace DrHD {

class SpectrumView {
public:
    static void render(SerialPort& serial);
};

} // namespace DrHD
