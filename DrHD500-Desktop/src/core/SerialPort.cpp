#include "SerialPort.hpp"
#include "BluetoothDriver.hpp"
#include "data/TerrestrialDB.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <glob.h>
#include <cmath>
#include <random>
#include <iostream>

namespace DrHD {

SerialPort::SerialPort() {
    // Initialize spectrum buffers
    spectrum_.levels.resize(300, 25.0f);
    spectrum_.maxHold.resize(300, 25.0f);
    spectrum_.avgTrace.resize(300, 25.0f);
    spectrum_.spanMHz = 700;
    spectrum_.centerFreqMHz = 600.0f;
    spectrum_.refLevelDbuV = 50;
}

SerialPort::~SerialPort() {
    closePort();
}

std::vector<std::string> SerialPort::listAvailablePorts() {
    std::vector<std::string> ports;
    glob_t globbuf;
    if (glob("/dev/cu.*", GLOB_NOSORT, nullptr, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            std::string p(globbuf.gl_pathv[i]);
            if (p.find("debug") == std::string::npos && p.find("Incoming") == std::string::npos) {
                ports.push_back(p);
            }
        }
    }
    globfree(&globbuf);
    return ports;
}

bool SerialPort::openPort(const std::string& portName, int baudRate) {
    closePort();

    portName_ = portName;
    baudRate_ = baudRate;
    shouldStop_ = false;

    // 1. On macOS, opening /dev/cu.DrHD500ComboBT via POSIX open() opens a dormant TTY node
    // that never triggers Bluetooth baseband/RFCOMM connection.
    // Therefore, for any Dr.HD Bluetooth device, prioritize native IOBluetooth RFCOMM driver!
    bool isDrHD = (portName.find("DrHD") != std::string::npos || 
                   portName.find("01-ac-78") != std::string::npos ||
                   portName.find("01:ac:78") != std::string::npos ||
                   portName.find("01:AC:78") != std::string::npos ||
                   portName.find("BT") != std::string::npos);

    if (isDrHD) {
        std::cout << "[SerialPort] Connecting to Dr.HD via native macOS IOBluetooth RFCOMM..." << std::endl;
        isUsingNativeBT_ = true;
        BluetoothDriver::instance().setDataCallback([this](const uint8_t* data, size_t len) {
            this->handleIncomingBytes(data, len);
        });

        BluetoothDriver::instance().setStatusCallback([this](bool connected, const std::string& info) {
            this->status_ = connected ? ConnectionStatus::Connected : ConnectionStatus::Disconnected;
            std::cout << "[SerialPort] Bluetooth status: " << info << std::endl;
        });

        std::string mac = (portName.find(":") != std::string::npos || portName.find("-") != std::string::npos) 
                          ? portName : "01-ac-78-00-04-55";

        if (BluetoothDriver::instance().connect(mac)) {
            status_ = ConnectionStatus::Connected;
            workerThread_ = std::thread(&SerialPort::workerThreadLoop, this);
            return true;
        } else {
            std::cerr << "[SerialPort] Native IOBluetooth connection to " << mac << " failed." << std::endl;
        }
    }

    // 2. Standard POSIX serial port (for physical USB UART dongles, FTDI, etc.)
    if (portName.rfind("/dev/", 0) == 0) {
        fd_ = open(portName_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ >= 0) {
            struct termios tty;
            if (tcgetattr(fd_, &tty) == 0) {
                cfsetospeed(&tty, B115200);
                cfsetispeed(&tty, B115200);

                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit
                tty.c_cflag |= (CLOCAL | CREAD);            // Ignore control lines, enable reading
                tty.c_cflag &= ~(PARENB | PARODD);          // No parity
                tty.c_cflag &= ~CSTOPB;                     // 1 stop bit
                tty.c_cflag &= ~CRTSCTS;                    // No hardware flow control

                tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
                tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // Raw mode
                tty.c_oflag &= ~OPOST;                                    // Raw output

                tty.c_cc[VMIN]  = 0;
                tty.c_cc[VTIME] = 1;

                tcsetattr(fd_, TCSANOW, &tty);
                tcflush(fd_, TCIOFLUSH);

                isUsingNativeBT_ = false;
                status_ = ConnectionStatus::Connected;
                std::cout << "[SerialPort] Successfully opened POSIX serial port: " << portName_ << std::endl;
                workerThread_ = std::thread(&SerialPort::workerThreadLoop, this);
                readerThread_ = std::thread(&SerialPort::readerThreadLoop, this);
                return true;
            }
            close(fd_);
            fd_ = -1;
        }
    }

    status_ = ConnectionStatus::Disconnected;
    return false;
}

void SerialPort::closePort() {
    shouldStop_ = true;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    if (readerThread_.joinable()) {
        readerThread_.join();
    }
    if (isUsingNativeBT_) {
        BluetoothDriver::instance().disconnect();
        isUsingNativeBT_ = false;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    status_ = ConnectionStatus::Disconnected;
}

void SerialPort::startSimulation() {
    closePort();
    shouldStop_ = false;
    status_ = ConnectionStatus::Simulation;
    workerThread_ = std::thread(&SerialPort::workerThreadLoop, this);
}

void SerialPort::stopSimulation() {
    closePort();
}

void SerialPort::setWorkingBand(WorkingBand band) {
    band_ = band;
    tunedInfo_.band = band;
    if (band == WorkingBand::Satellite) {
        std::lock_guard<std::mutex> lock(dataMutex_);
        currentMode_ = CmdMode::SatSNR;
        tunedInfo_.channelOrSatName = "Hotbird 13.0E";
        tunedInfo_.frequencyMHz = 10992.0f;
        tunedInfo_.standard = "DVB-S2";
        tunedInfo_.polarization = "V";
        tunedInfo_.symbolRateKSps = 27500;
        tunedInfo_.isAnalog = false;
        specSpanBegin_ = 950;
        specSpan_ = 1200;
        spectrum_.spanMHz = 1200;
        spectrum_.centerFreqMHz = 1550.0f;
    } else if (band == WorkingBand::TerrestrialVHF) {
        currentMode_ = CmdMode::TerSNR;
        setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHF, "CH 5", 177.5f, 7, false, false, "DVB-T");
    } else if (band == WorkingBand::TerrestrialVHFLow) {
        currentMode_ = CmdMode::TerSNR;
        setTerrestrialTuningDetailed(WorkingBand::TerrestrialVHFLow, "CH E2", 48.25f, 7, false, true, "Analog PAL B/G", 48.25f, 53.75f);
    } else if (band == WorkingBand::CableTV) {
        currentMode_ = CmdMode::CabSNR;
        setTerrestrialTuningDetailed(WorkingBand::CableTV, "CH S01", 106.0f, 8, false, false, "DVB-C");
    } else {
        currentMode_ = CmdMode::TerSNR;
        setTerrestrialTuningDetailed(WorkingBand::TerrestrialUHF, "CH 21", 474.0f, 8, true, false, "DVB-T2");
    }
}

void SerialPort::setSatelliteTuning(int ifFreqMHz, int symbolRateKSps, bool isS2, int powerMode) {
    satIfFreq_ = ifFreqMHz;
    satSymbolRate_ = symbolRateKSps;
    satIsS2_ = isS2;
    satPowerMode_ = powerMode;
    specSpanBegin_ = std::max(950, ifFreqMHz - 200);
    specSpan_ = 400;
    spectrum_.spanMHz = 400;
    spectrum_.centerFreqMHz = static_cast<float>(specSpanBegin_) + 200.0f;

    tunedInfo_.band = WorkingBand::Satellite;
    tunedInfo_.ifFreqMHz = ifFreqMHz;
    tunedInfo_.symbolRateKSps = symbolRateKSps;
    tunedInfo_.standard = isS2 ? "DVB-S2" : "DVB-S";
    tunedInfo_.polarization = (powerMode == 4) ? "H (18V)" : "V (13V)";
    tunedInfo_.isAnalog = false;
}

void SerialPort::setSatelliteTuningDetailed(const std::string& satName, float rfFreqMHz, int ifFreqMHz, int symbolRateKSps, bool isS2, const std::string& pol, int powerMode) {
    setSatelliteTuning(ifFreqMHz, symbolRateKSps, isS2, powerMode);
    tunedInfo_.channelOrSatName = satName;
    tunedInfo_.frequencyMHz = rfFreqMHz;
    tunedInfo_.polarization = pol;
}

void SerialPort::setTerrestrialTuning(int freqMHz, bool isT2, int bwCode, int powerMode) {
    setTerrestrialTuningDetailed(
        WorkingBand::TerrestrialUHF,
        "CH" + std::to_string(static_cast<int>((freqMHz - 474) / 8 + 21)),
        static_cast<float>(freqMHz),
        (bwCode == 1) ? 7 : (bwCode == 2) ? 6 : 8,
        isT2,
        false,
        isT2 ? "DVB-T2" : "DVB-T"
    );
    terPowerMode_ = powerMode;
}

void SerialPort::setTerrestrialTuningDetailed(
    WorkingBand band,
    const std::string& chName,
    float freqMHz,
    int bwMHz,
    bool isT2,
    bool isAnalog,
    const std::string& standard,
    float vidMHz,
    float audMHz
) {
    band_ = band;
    terFreqMHz_ = static_cast<int>(std::round(freqMHz));
    terIsT2_ = isT2;
    terBwCode_ = (bwMHz == 7) ? 1 : (bwMHz == 6) ? 2 : 0;
    terPowerMode_ = 0;
    
    // Maintain active zoom level
    if (band == WorkingBand::TerrestrialVHF) {
        specSpanBegin_ = 160;
        specSpan_ = 100;
        spectrum_.centerFreqMHz = 210.0f;
    } else if (band == WorkingBand::TerrestrialVHFLow) {
        specSpanBegin_ = 40;
        specSpan_ = 80;
        spectrum_.centerFreqMHz = 80.0f;
    } else if (band == WorkingBand::CableTV) {
        specSpanBegin_ = 100;
        specSpan_ = 400;
        spectrum_.centerFreqMHz = 300.0f;
    } else { // UHF
        if (specSpan_ == 700) {
            specSpanBegin_ = 250;
            spectrum_.centerFreqMHz = 600.0f;
        } else {
            specSpanBegin_ = std::max(250, static_cast<int>(freqMHz) - (specSpan_ / 2));
            spectrum_.centerFreqMHz = freqMHz;
        }
    }
    spectrum_.spanMHz = specSpan_;

    tunedInfo_.band = band;
    tunedInfo_.frequencyMHz = freqMHz;
    tunedInfo_.bandwidthMHz = bwMHz;
    tunedInfo_.standard = standard;
    tunedInfo_.channelNameOnly = chName;
    tunedInfo_.isAnalog = isAnalog;
    tunedInfo_.videoCarrierMHz = vidMHz;
    tunedInfo_.audioCarrierMHz = audMHz;

    if (isAnalog) {
        tunedInfo_.channelOrSatName = chName + " (" + standard + ")";
    } else {
        tunedInfo_.channelOrSatName = chName + " (" + std::to_string(static_cast<int>(freqMHz)) + " MHz)";
    }
}

void SerialPort::setSpectrumTuning(int spanBegin, int span, int refLevel) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    specSpanBegin_ = spanBegin;
    specSpan_ = span;
    specRefLevel_ = refLevel;
    spectrum_.spanMHz = span;
    if (span == 700) {
        spectrum_.centerFreqMHz = 600.0f;
    } else {
        spectrum_.centerFreqMHz = static_cast<float>(spanBegin) + (static_cast<float>(span) / 2.0f);
    }
    spectrum_.refLevelDbuV = refLevel;
}

void SerialPort::clearConstellation() {
    std::lock_guard<std::mutex> lock(dataMutex_);
    constPoints_.clear();
}

void SerialPort::getTelemetry(SignalMetrics& metrics, HardwareTelemetry& hw, std::vector<ConstellationPoint>& constPts, SpectrumData& spec) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    metrics = metrics_;
    hw = telemetry_;
    constPts = constPoints_;
    spec = spectrum_;
}

void SerialPort::handleIncomingBytes(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    rxBuffer_.insert(rxBuffer_.end(), data, data + length);

    while (rxBuffer_.size() >= PACKET_RX_SIZE) {
        if (rxBuffer_[0] != PACKET_HEADER_RX) {
            auto it = std::find(rxBuffer_.begin() + 1, rxBuffer_.end(), PACKET_HEADER_RX);
            if (it != rxBuffer_.end()) {
                rxBuffer_.erase(rxBuffer_.begin(), it);
            } else {
                rxBuffer_.clear();
                break;
            }
            continue;
        }

        Protocol::parseTelemetryPacket(
            rxBuffer_.data(),
            metrics_,
            telemetry_,
            constPoints_,
            spectrum_
        );

        static int rxCount = 0;
        if (++rxCount % 10 == 0) {
            std::cout << "[DrHD RX] PktType=" << static_cast<int>(rxBuffer_[1]) 
                      << " | Lock=" << (metrics_.carrierLock ? "YES" : "NO")
                      << " | SNR=" << metrics_.snrDb << " dB"
                      << " | MER=" << metrics_.merDb << " dB"
                      << " | Pwr=" << metrics_.pwrDbuV << " dBuV"
                      << " | Batt=" << telemetry_.batteryPercent << "% (" << telemetry_.batteryMilliVolts << " mV)"
                      << " | ConstPts=" << constPoints_.size()
                      << " | Services=" << metrics_.services.size()
                      << std::endl;
        }

        rxBuffer_.erase(rxBuffer_.begin(), rxBuffer_.begin() + PACKET_RX_SIZE);
    }
}

void SerialPort::readerThreadLoop() {
    uint8_t readBuf[512];

    while (!shouldStop_) {
        if (fd_ >= 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd_, &rfds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 20000; // 20ms polling timeout

            int sel = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
            if (sel > 0 && FD_ISSET(fd_, &rfds)) {
                ssize_t n = read(fd_, readBuf, sizeof(readBuf));
                if (n > 0) {
                    handleIncomingBytes(readBuf, static_cast<size_t>(n));
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void SerialPort::sendCommandPacket() {
    tickCounter_++;
    std::vector<uint8_t> pkt;

    // Send service telemetry query every 30 ticks
    if (tickCounter_ % 30 == 0) {
        pkt = Protocol::makeServicePacket(0);
    } else if (currentMode_ == CmdMode::SpectrumRDA || currentMode_ == CmdMode::SpectrumRDACal) {
        pkt = Protocol::makeSpectrumPacket(
            static_cast<int>(currentMode_),
            specSpanBegin_,
            specSpan_,
            specRefLevel_,
            (band_ == WorkingBand::Satellite) ? satPowerMode_ : terPowerMode_,
            true
        );
    } else if (band_ == WorkingBand::Satellite) {
        pkt = Protocol::makeMeasuresSatPacket(
            static_cast<int>(currentMode_),
            satIfFreq_,
            satSymbolRate_,
            satIsS2_,
            satPowerMode_
        );
    } else {
        pkt = Protocol::makeMeasuresTerPacket(
            static_cast<int>(currentMode_),
            terFreqMHz_,
            terIsT2_,
            terBwCode_,
            terPowerMode_
        );
    }

    if (pkt.empty()) return;

    if (isUsingNativeBT_) {
        BluetoothDriver::instance().sendPacketPaced(pkt);
    } else if (fd_ >= 0) {
        for (size_t i = 0; i < pkt.size(); i += 2) {
            size_t chunkLen = std::min<size_t>(2, pkt.size() - i);
            write(fd_, pkt.data() + i, chunkLen);
            if (i + 2 < pkt.size()) {
                usleep(130000);
            }
        }
    }
}

void SerialPort::workerThreadLoop() {
    while (!shouldStop_) {
        if (status_ == ConnectionStatus::Simulation) {
            simulationTick();
            std::this_thread::sleep_for(std::chrono::milliseconds(130));
            continue;
        }

        // Hardware mode
        if (isUsingNativeBT_) {
            if (BluetoothDriver::instance().isConnected()) {
                status_ = ConnectionStatus::Connected;
                sendCommandPacket();
            } else {
                status_ = ConnectionStatus::Disconnected;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
        } else if (fd_ >= 0) {
            sendCommandPacket();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}

void SerialPort::simulationTick() {
    tickCounter_++;
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(dataMutex_);

    // 1. Hardware Telemetry
    telemetry_.batteryMilliVolts = 7650 + static_cast<int>(std::sin(tickCounter_ * 0.05f) * 60.0f);
    telemetry_.batteryPercent = 78;
    telemetry_.isCharging = false;
    telemetry_.lnbVoltage = (satPowerMode_ == 4) ? 18.2f : 13.7f;
    telemetry_.lnbCurrentMilliAmps = 142 + static_cast<int>(std::sin(tickCounter_ * 0.1f) * 5.0f);
    telemetry_.psuVoltage = 12.0f;
    telemetry_.lnbPowerEnabled = true;
    telemetry_.hwVersion = 134;
    telemetry_.swVersion = 49;
    telemetry_.hardwareID = "01:AC:78:00:04:55";
    telemetry_.lastUpdate = now;

    // 2. Realistic Channel Signal Map based on actual terrestrial UHF broadcast layout
    // CH 21: 474 MHz (Live antenna signal - 13.6 dB C/N, 73 dBuV)
    // CH 38: 610 MHz (Bahrain DTT - 14.2 dB C/N, 68 dBuV)
    // CH 57: 762 MHz (Secondary mux - 15.3 dB C/N, 76 dBuV)
    float curFreq = (band_ == WorkingBand::Satellite) ? static_cast<float>(satIfFreq_) : static_cast<float>(terFreqMHz_);
    
    bool isLocked = false;
    float targetSNR = 0.0f;
    float targetPwr = 28.0f; // RF noise floor ~28 dBuV
    float targetMER = 0.0f;
    float targetLM = 0.0f;
    float noise = static_cast<float>((rand() % 10) - 5) * 0.05f;

    if (band_ == WorkingBand::Satellite) {
        // Satellite lock if active TP
        isLocked = true;
        targetSNR = 13.8f + noise;
        targetPwr = 64.0f + noise * 2.0f;
        targetMER = 12.5f;
        targetLM = targetSNR - 10.5f;
    } else if (band_ == WorkingBand::TerrestrialVHFLow) {
        if (curFreq >= 87.0f && curFreq <= 108.5f) {
            // FM Broadcast Radio Carrier (Analog W-FM)
            isLocked = false; // Analog has no COFDM lock
            targetSNR = 0.0f;
            targetPwr = 62.5f + noise * 1.5f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        } else if (std::abs(curFreq - 48.25f) < 2.0f) {
            // Channel E2 (48.25 MHz) - Analog PAL B/G Carrier
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 57.0f + noise * 1.5f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        } else {
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 24.0f + ((rand() % 15) - 7) * 0.3f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        }
    } else if (band_ == WorkingBand::TerrestrialVHF) {
        if (std::abs(curFreq - 177.5f) < 2.0f) {
            // Channel 5 (177.5 MHz) - VHF Band III DVB-T Multiplex
            isLocked = true;
            targetSNR = 16.2f + std::sin(tickCounter_ * 0.08f) * 0.3f + noise;
            targetPwr = 67.0f + noise * 2.0f;
            targetMER = 15.0f + noise * 0.2f;
            targetLM = targetSNR - 12.0f;
        } else if (std::abs(curFreq - 189.25f) < 2.0f) {
            // Channel E7 (189.25 MHz) - Analog PAL B/G Carrier
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 61.0f + noise * 1.5f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        } else {
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 25.0f + ((rand() % 15) - 7) * 0.3f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        }
    } else if (band_ == WorkingBand::CableTV) {
        if (std::abs(curFreq - 114.0f) < 2.0f) {
            // Channel S02 (114 MHz) - DVB-C 64QAM Cable Stream
            isLocked = true;
            targetSNR = 31.8f + std::sin(tickCounter_ * 0.08f) * 0.2f + noise;
            targetPwr = 71.5f + noise * 1.5f;
            targetMER = 30.5f + noise * 0.2f;
            targetLM = 6.5f;
        } else {
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 28.0f + ((rand() % 15) - 7) * 0.3f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        }
    } else { // TerrestrialUHF
        if (std::abs(curFreq - 474.0f) < 2.0f) {
            // Channel 21 (474 MHz) - Live broadcast
            isLocked = true;
            targetSNR = 13.6f + std::sin(tickCounter_ * 0.08f) * 0.3f + noise;
            targetPwr = 73.0f + noise * 2.0f;
            targetMER = 11.6f + noise * 0.2f;
            targetLM = targetSNR - 12.0f; // 1.6 dB LM
        } else if (std::abs(curFreq - 610.0f) < 2.0f) {
            // Channel 38 (610 MHz) - Live broadcast
            isLocked = true;
            targetSNR = 14.2f + std::sin(tickCounter_ * 0.08f) * 0.3f + noise;
            targetPwr = 68.0f + noise * 2.0f;
            targetMER = 12.2f + noise * 0.2f;
            targetLM = targetSNR - 12.0f; // 2.2 dB LM
        } else if (std::abs(curFreq - 762.0f) < 2.0f) {
            // Channel 57 (762 MHz)
            isLocked = true;
            targetSNR = 15.3f + std::sin(tickCounter_ * 0.08f) * 0.3f + noise;
            targetPwr = 76.0f + noise * 2.0f;
            targetMER = 15.6f + noise * 0.2f;
            targetLM = targetSNR - 12.0f;
        } else if (std::abs(curFreq - 802.0f) < 8.0f) {
            // 802 MHz LTE Band 20 Downlink (Cell tower - Non-DVB carrier)
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 52.0f + noise * 1.5f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        } else if (std::abs(curFreq - 935.0f) < 15.0f) {
            // 935 MHz GSM 900 Downlink (Cell tower - Non-DVB carrier)
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 54.0f + noise * 1.5f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        } else {
            // Empty UHF channel (No transmitter)
            isLocked = false;
            targetSNR = 0.0f;
            targetPwr = 28.0f + ((rand() % 15) - 7) * 0.3f;
            targetMER = 0.0f;
            targetLM = 0.0f;
        }
    }

    metrics_.carrierLock = isLocked;
    metrics_.demodLock = isLocked;
    metrics_.snrDb = targetSNR;
    metrics_.pwrDbuV = targetPwr;
    metrics_.merDb = targetMER;
    metrics_.hasLmCn = isLocked;
    metrics_.lmCnDb = std::max(0.0f, targetLM);
    metrics_.ber = isLocked ? 1.0e-7f : 0.5f;
    metrics_.vber = isLocked ? 4.0e-7f : 0.0f;
    metrics_.packetErrors = isLocked ? 0 : 99;
    metrics_.lastUpdate = now;

    // 3. Realistic Full UHF Spectrum Profile based on active zoom span
    if (spectrum_.levels.size() < 300) {
        spectrum_.levels.resize(300, 25.0f);
        spectrum_.maxHold.resize(300, 25.0f);
        spectrum_.avgTrace.resize(300, 25.0f);
    }

    spectrum_.spanMHz = specSpan_;
    spectrum_.refLevelDbuV = specRefLevel_;

    float spanStart = (specSpan_ == 700) ? 250.0f : (spectrum_.centerFreqMHz - (specSpan_ / 2.0f));

    // Known broadcast carriers across UHF band
    struct BroadcastCarrier {
        float freqMHz;
        float powerDbuV;
    };
    static const BroadcastCarrier carriers[] = {
        { 265.0f, 45.0f }, // CATV / VHF Mid-band
        { 310.0f, 44.0f }, // CATV / PMR
        { 474.0f, 38.0f }, // CH 21 TV (DVB-T2)
        { 506.0f, 32.0f }, // CH 25
        { 610.0f, 45.0f }, // CH 38 TV (DVB-T2)
        { 666.0f, 30.0f }, // CH 45
        { 706.0f, 35.0f }, // CH 50
        { 802.0f, 52.0f }, // 4G LTE Band 20 Downlink (791-821 MHz cell tower)
        { 811.0f, 51.0f }, // 4G LTE Band 20 Secondary block
        { 935.0f, 54.0f }, // GSM 900 / LTE Band 8 Downlink
        { 945.0f, 55.0f }  // GSM 900
    };

    float peakVal = -999.0f;
    int peakIdx = 150;

    for (int i = 0; i < 300; ++i) {
        float f = spanStart + (static_cast<float>(i) / 299.0f) * spectrum_.spanMHz;
        
        // Base RF thermal noise floor with subtle frequency tilt and jitter
        float noiseFloor = 30.0f + std::sin(f * 0.02f) * 2.5f + ((rand() % 20) - 10) * 0.15f;
        float totalSignal = noiseFloor;

        for (const auto& bc : carriers) {
            float delta = std::abs(f - bc.freqMHz);
            // DVB-T2 COFDM brickwall channel profile (7.6 MHz plateau with steep roll-off skirts)
            if (delta <= 3.8f) {
                // Inside channel: flat plateau with slight OFDM ripple
                float ripple = std::sin(f * 20.0f) * 0.8f + ((rand() % 10) - 5) * 0.1f;
                float level = bc.powerDbuV + ripple;
                totalSignal = std::max(totalSignal, level);
            } else if (delta < 12.0f) {
                // Out-of-band emissions & skirts
                float skirtDist = delta - 3.8f;
                float atten = skirtDist * 6.5f;
                float skirtLevel = bc.powerDbuV - atten;
                totalSignal = std::max(totalSignal, skirtLevel);
            }
        }

        float val = std::min(110.0f, std::max(15.0f, totalSignal));
        spectrum_.levels[i] = val;
        spectrum_.maxHold[i] = std::max(spectrum_.maxHold[i], val);
        spectrum_.avgTrace[i] = (spectrum_.avgTrace[i] * 0.85f) + (val * 0.15f);

        if (val > peakVal) {
            peakVal = val;
            peakIdx = i;
        }
    }
    spectrum_.peakIndex = peakIdx;
    spectrum_.peakDbuV = peakVal;
    spectrum_.peakFreqMHz = spanStart + (static_cast<float>(peakIdx) / 299.0f) * spectrum_.spanMHz;
    spectrum_.lastSweepTime = now;

    // 4. Constellation Simulation (Active only when locked)
    if (isLocked) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 0.06f);

        for (int i = 0; i < 4; ++i) {
            int quad = (tickCounter_ + i) % 4;
            float idealI = (quad == 0 || quad == 1) ? 0.707f : -0.707f;
            float idealQ = (quad == 0 || quad == 2) ? 0.707f : -0.707f;

            ConstellationPoint pt;
            pt.i = idealI + dist(gen);
            pt.q = idealQ + dist(gen);
            pt.age = 0.0f;
            pt.timestamp = now;
            constPoints_.push_back(pt);
        }

        if (constPoints_.size() > 300) {
            constPoints_.erase(constPoints_.begin(), constPoints_.begin() + (constPoints_.size() - 300));
        }

        for (auto& pt : constPoints_) {
            float elapsedSec = std::chrono::duration<float>(now - pt.timestamp).count();
            pt.age = std::min(1.0f, elapsedSec / 1.5f);
        }
    } else {
        constPoints_.clear();
    }
}

} // namespace DrHD
