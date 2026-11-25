#include "DrHDProtocol.hpp"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace DrHD {

uint8_t Protocol::computeCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8C;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool Protocol::validateChecksum(const uint8_t* packet104) {
    if (packet104[0] != PACKET_HEADER_RX) return false;
    uint32_t sum = 0;
    for (size_t i = 0; i < 102; ++i) {
        sum += packet104[i];
    }
    uint16_t expected = (static_cast<uint16_t>(packet104[102]) << 8) | packet104[103];
    return static_cast<uint16_t>(sum & 0xFFFF) == expected;
}

uint8_t Protocol::calculateSpanCode(int span, bool isRda) {
    uint8_t b = isRda ? ((span < 100) ? 8 : 0) : 11;
    switch (span) {
        case 8:
        case 700:
            return b + 4;
        case 16:
        case 800:
            return b + 3;
        case 24:
        case 26:
        case 27:
        case 28:
        case 1200:
            return b + 2;
        case 40:
        case 50:
        case 54:
        case 1600:
            return b + 1;
        case 72:
        case 2000:
            return b;
        case 300:
            return b + 6;
        case 600:
            return b + 5;
        default:
            return b + 7; // Span 400, etc.
    }
}

std::vector<uint8_t> Protocol::makeMeasuresSatPacket(
    int mode,
    int intermediateFreqMHz,
    int symbolRateKSps,
    bool isS2,
    int powerMode
) {
    std::vector<uint8_t> pkt(8, 0);
    pkt[0] = PACKET_HEADER_CMD;
    pkt[1] = static_cast<uint8_t>(((mode & 0x0F) << 4) | 0x08);
    pkt[2] = static_cast<uint8_t>((powerMode << 5) | (isS2 ? 0x10 : 0x00));
    pkt[3] = static_cast<uint8_t>((intermediateFreqMHz >> 8) & 0xFF);
    pkt[4] = static_cast<uint8_t>(intermediateFreqMHz & 0xFF);
    pkt[5] = static_cast<uint8_t>((symbolRateKSps >> 8) & 0xFF);
    pkt[6] = static_cast<uint8_t>(symbolRateKSps & 0xFF);
    pkt[7] = computeCRC8(pkt.data(), 7);
    return pkt;
}

std::vector<uint8_t> Protocol::makeMeasuresTerPacket(
    int mode,
    int frequencyMHz,
    bool isT2,
    int bandwidthCode,
    int powerMode
) {
    std::vector<uint8_t> pkt(6, 0);
    pkt[0] = PACKET_HEADER_CMD;
    pkt[1] = static_cast<uint8_t>(((mode & 0x0F) << 4) | 0x04);
    pkt[2] = static_cast<uint8_t>(powerMode << 5);
    if (isT2) {
        pkt[2] |= 0x10; // DVB-T2 bit
    }
    int bw = bandwidthCode >= 0 ? bandwidthCode : 0;
    pkt[1] += static_cast<uint8_t>(bw & 0x03);
    pkt[2] += static_cast<uint8_t>((bw & 0x04) << 5);
    pkt[3] = static_cast<uint8_t>((frequencyMHz >> 8) & 0xFF);
    pkt[4] = static_cast<uint8_t>(frequencyMHz & 0xFF);
    pkt[5] = computeCRC8(pkt.data(), 5);
    return pkt;
}

std::vector<uint8_t> Protocol::makeSpectrumPacket(
    int mode,
    int spanBeginFreq,
    int span,
    int refLevel,
    int powerMode,
    bool isRdaMode
) {
    std::vector<uint8_t> pkt(6, 0);
    pkt[0] = PACKET_HEADER_CMD;
    pkt[1] = static_cast<uint8_t>(((mode & 0x0F) << 4) | 0x04);
    pkt[2] = static_cast<uint8_t>(powerMode << 5);

    int spanFrameBegin = spanBeginFreq + (span > 100 ? 0 : span / 2);
    pkt[3] = static_cast<uint8_t>((spanFrameBegin >> 8) & 0xFF);
    pkt[4] = static_cast<uint8_t>(spanFrameBegin & 0xFF);

    uint8_t sc = calculateSpanCode(span, isRdaMode);
    pkt[3] |= static_cast<uint8_t>((static_cast<int>(sc) << 5) & 0xE0);

    if (mode == static_cast<int>(CmdMode::SpectrumRDACal) || mode == static_cast<int>(CmdMode::SpectrumMXLCal)) {
        pkt[1] |= static_cast<uint8_t>((refLevel & 0x60) >> 5);
        pkt[2] |= static_cast<uint8_t>(refLevel & 0x1F);
        pkt[3] |= static_cast<uint8_t>((refLevel & 0x80) >> 4);
    } else {
        int adjustedRef = refLevel;
        if (span < 100) {
            adjustedRef = std::min(110, refLevel + 20);
        }
        pkt[2] |= static_cast<uint8_t>(((110 - adjustedRef) / 5) & 0x1F);
        pkt[1] |= static_cast<uint8_t>((static_cast<int>(sc) >> 3) & 0x03);
    }

    pkt[5] = computeCRC8(pkt.data(), 5);
    return pkt;
}

std::vector<uint8_t> Protocol::makeServicePacket(int serviceMode) {
    std::vector<uint8_t> pkt(4, 0);
    pkt[0] = PACKET_HEADER_CMD;
    pkt[1] = static_cast<uint8_t>((static_cast<int>(CmdMode::Service) & 0x0F) << 4);
    pkt[2] = static_cast<uint8_t>(serviceMode & 0xFF);
    pkt[3] = computeCRC8(pkt.data(), 3);
    return pkt;
}

std::vector<uint8_t> Protocol::makeDiseqcPacket(const std::vector<uint8_t>& rawCmd) {
    std::vector<uint8_t> pkt;
    pkt.push_back(PACKET_HEADER_CMD);
    pkt.push_back(0);
    pkt.insert(pkt.end(), rawCmd.begin(), rawCmd.end());
    pkt.push_back(computeCRC8(pkt.data(), pkt.size()));
    return pkt;
}

void Protocol::parseTelemetryPacket(
    const uint8_t* packet,
    SignalMetrics& metrics,
    HardwareTelemetry& telemetry,
    std::vector<ConstellationPoint>& constPoints,
    SpectrumData& spectrum
) {
    if (packet[0] != PACKET_HEADER_RX) return;
    uint8_t pktType = packet[1];

    auto now = std::chrono::steady_clock::now();

    // Packet 13: Hardware Voltages and Battery
    if (pktType == 13) {
        int battRaw = (static_cast<int>(packet[2]) << 8) | packet[3];
        int battMV = battRaw * 10; // Firmware transmits centivolts / 10x millivolts
        telemetry.batteryMilliVolts = battMV;

        // Battery percentage curve (2S Li-ion battery: 6.8V min to 8.4V max)
        float pct = (battMV - 6800.0f) / (8400.0f - 6800.0f) * 100.0f;
        telemetry.batteryPercent = std::max(0, std::min(100, static_cast<int>(std::round(pct))));
        
        int lnb10x = (static_cast<int>(packet[4]) << 8) | packet[5];
        telemetry.lnbVoltage = lnb10x / 10.0f;

        int psu10x = (static_cast<int>(packet[6]) << 8) | packet[7];
        telemetry.psuVoltage = psu10x / 10.0f;

        telemetry.isCharging = (telemetry.psuVoltage > 11.0f && telemetry.batteryPercent < 98);

        int lnbA = (static_cast<int>(packet[8]) << 8) | packet[9];
        telemetry.lnbCurrentMilliAmps = lnbA;

        telemetry.lnbPowerEnabled = (packet[10] & 0x04) != 0;
        telemetry.hwVersion = packet[11];
        telemetry.swVersion = packet[13];

        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(packet[14 + i]);
            if (i < 7) ss << ":";
        }
        telemetry.hardwareID = ss.str();
        telemetry.lastUpdate = now;
        return;
    }

    // Packet 0, 1, 2, 32..49, 128..139: Spectrum sweep chunks (100 points per chunk -> 300 total)
    if (pktType <= 2 || (pktType >= 32 && pktType <= 49) || (pktType >= 128 && pktType <= 139)) {
        if (spectrum.levels.size() < 300) {
            spectrum.levels.resize(300, 25.0f);
            spectrum.maxHold.resize(300, 25.0f);
            spectrum.avgTrace.resize(300, 25.0f);
        }

        int chunk = (pktType & 0x0F) % 3;
        int chunkOffset = chunk * 100;
        float refL = static_cast<float>(spectrum.refLevelDbuV);

        for (int i = 0; i < 100; ++i) {
            int idx = chunkOffset + i;
            if (idx >= 300) break;
            
            float rawVal = static_cast<float>(packet[i + 2]);
            // Linear full-scale mapping from 0.0 dBµV up to active reference level refL
            float valDbuV = (rawVal / 254.0f) * refL;
            
            spectrum.levels[idx] = valDbuV;
            spectrum.maxHold[idx] = std::max(spectrum.maxHold[idx], valDbuV);
            spectrum.avgTrace[idx] = (spectrum.avgTrace[idx] * 0.85f) + (valDbuV * 0.15f);
        }

        // Recompute peak when Chunk 2 finishes sweep
        if (chunk == 2) {
            float maxVal = -999.0f;
            int maxIdx = 0;
            for (size_t i = 0; i < spectrum.levels.size(); ++i) {
                if (spectrum.levels[i] > maxVal) {
                    maxVal = spectrum.levels[i];
                    maxIdx = static_cast<int>(i);
                }
            }
            spectrum.peakIndex = maxIdx;
            spectrum.peakDbuV = maxVal;
            
            float spanStart = spectrum.centerFreqMHz - (spectrum.spanMHz / 2.0f);
            spectrum.peakFreqMHz = spanStart + (static_cast<float>(maxIdx) / 299.0f) * spectrum.spanMHz;
            spectrum.lastSweepTime = now;
        }
        return;
    }

    // Packet 3..12, 14, 15: Measures & Constellation
    static const uint8_t lockByteIdx[] = {0, 0, 0, 21, 13, 13, 13, 21, 21, 5, 21, 13, 5, 0, 5, 5};
    uint8_t lockIdx = (pktType < 16) ? lockByteIdx[pktType] : 13;
    bool hwLock = (packet[lockIdx] & 0x01) != 0;

    if (pktType == 3 || pktType == 4 || pktType == 9 || pktType == 10 || pktType == 11 || pktType == 12) {
        // Terrestrial (DVB-T / DVB-T2 / ISDB-T)
        // 1. Calculate C/N (SNR) from packet[2..4]
        int i2 = ((static_cast<int>(packet[2]) & 0xFF) << 8) | (static_cast<int>(packet[3]) & 0xFF);
        int flag = (static_cast<int>(packet[4]) & 64) << 8;
        float cnVal = 0.0f;
        if (i2 > 0) {
            if (flag == 0) {
                cnVal = static_cast<float>(std::log10(65536.0 / static_cast<double>(i2)) * 10.0) + 2.0f;
            } else {
                cnVal = static_cast<float>(std::log10(16384.0 / static_cast<double>(i2)) * 10.0) - 6.0f;
            }
            cnVal = std::max(0.0f, std::min(40.0f, cnVal));
        }

        // 2. Determine carrier & demod lock
        metrics.carrierLock = hwLock || (cnVal >= 5.0f);
        metrics.demodLock = metrics.carrierLock;

        if (metrics.carrierLock) {
            metrics.snrDb = cnVal;
        } else {
            metrics.snrDb = 0.0f;
        }

        // 3. Calculate Power (RSSI) - always available from RF tuner front-end
        int rawPwrWord = ((static_cast<int>(packet[5]) & 0xFF) << 8) | (static_cast<int>(packet[6]) & 0xFF);
        float pwrVal = static_cast<float>(((rawPwrWord >> 6) * 0.14545f) - 12.7272f);
        if (pwrVal < 15.0f || pwrVal > 115.0f) {
            int rawPwr = ((static_cast<int>(packet[5]) & 0xFF) << 5) | (static_cast<int>(packet[6]) & 0x1F);
            pwrVal = 20.0f + (static_cast<float>(rawPwr) / 4096.0f) * 50.0f;
        }
        metrics.pwrDbuV = std::max(0.0f, std::min(130.0f, pwrVal));

        // 4. Calculate MER
        int iMer = ((static_cast<int>(packet[7]) & 0xFF) << 8) | (static_cast<int>(packet[8]) & 0xFF);
        if (iMer > 0) {
            metrics.merDb = static_cast<float>(std::log10(65536.0 / static_cast<double>(iMer)) * 10.0);
        } else if (metrics.carrierLock) {
            metrics.merDb = std::max(0.0f, metrics.snrDb - 2.0f);
        } else {
            metrics.merDb = 0.0f;
        }

        // Parse NetInfo & FEC parameters from byte 11 to 18 (for Packet 3 & 4)
        metrics.plp = static_cast<int>(packet[11]);
        int fecModel = packet[12];
        int modIdx = (fecModel & 224) >> 5;
        const char* mods[] = {"QPSK", "16QAM", "64QAM", "256-QAM"};
        if (modIdx < 4) metrics.modulationName = mods[modIdx];
        else metrics.modulationName = "64QAM";

        int crIdx = (fecModel & 28) >> 2;
        const char* crs[] = {"1/2", "3/5", "2/3", "3/4", "4/5", "5/6"};
        if (crIdx < 6) metrics.codeRate = crs[crIdx];
        else metrics.codeRate = "2/3";

        metrics.cellId = (static_cast<int>(packet[13]) << 8) | packet[14];
        metrics.netId = (static_cast<int>(packet[15]) << 8) | packet[16];

        int fftCode = packet[17] & 7;
        const char* ffts[] = {"", "2K", "4K", "8K", "16K", "32K", "", ""};
        if (fftCode < 8 && ffts[fftCode][0] != '\0') metrics.fftSize = ffts[fftCode];
        else metrics.fftSize = "32K";

        int giCode = (packet[17] & 56) >> 3;
        const char* gis[] = {"1/32", "1/16", "1/8", "1/4", "1/128", "19/128", "19/256", ""};
        if (giCode < 7) metrics.guardInterval = gis[giCode];
        else metrics.guardInterval = "1/8";

        // Calculate Link Margin C/N (e.g. for 64QAM 2/3 threshold is ~12.0 dB)
        float noiseMargin = (modIdx == 2 && crIdx == 1) ? 12.0f : (modIdx == 2 && crIdx == 2) ? 12.0f : 12.0f;
        metrics.lmCnDb = metrics.snrDb - noiseMargin;
        metrics.hasLmCn = (metrics.carrierLock || metrics.snrDb >= 5.0f) && (metrics.lmCnDb >= 0.05f);

        // Parse NIT / SDT Descriptors (Packet 4 or subsequent packets)
        if (pktType == 4) {
            size_t off = 14;
            while (off + 2 < PACKET_RX_SIZE) {
                uint8_t descType = packet[off];
                if (descType == '@') { // 0x40 NIT
                    size_t len = packet[off + 1];
                    off += 2;
                    if (off + len <= 102 && len > 0) {
                        metrics.networkName = std::string(reinterpret_cast<const char*>(&packet[off]), len);
                    }
                    off += len;
                } else if (descType == 'H') { // 0x48 SDT Service
                    size_t sdtLen = static_cast<size_t>(packet[off + 1]) + 3;
                    off += 2;
                    if (off + sdtLen <= 102 && sdtLen >= 4) {
                        DvbService svc;
                        uint8_t st = packet[off];
                        if (st == 17 || st == 25) svc.serviceType = "HDTV";
                        else if (st == 22) svc.serviceType = "SDTV";
                        else if (st == 2 || st == 7 || st == 10) svc.serviceType = "Radio";
                        else svc.serviceType = "TV";

                        size_t pLen = packet[off + 1];
                        size_t pOff = off + 2;
                        if (pOff + pLen <= off + sdtLen) {
                            svc.serviceProvider = std::string(reinterpret_cast<const char*>(&packet[pOff]), pLen);
                        }

                        size_t sOff = pOff + pLen;
                        if (sOff < off + sdtLen) {
                            size_t sLen = packet[sOff];
                            sOff++;
                            if (sOff + sLen <= off + sdtLen) {
                                svc.serviceName = std::string(reinterpret_cast<const char*>(&packet[sOff]), sLen);
                            }
                            size_t sidOff = sOff + sLen;
                            if (sidOff + 1 < off + sdtLen) {
                                std::stringstream sidSS;
                                sidSS << std::hex << std::uppercase << std::setfill('0')
                                      << std::setw(2) << static_cast<int>(packet[sidOff])
                                      << std::setw(2) << static_cast<int>(packet[sidOff + 1]);
                                svc.sid = sidSS.str();
                            }
                        }

                        if (!svc.serviceName.empty()) {
                            bool exists = false;
                            for (const auto& existing : metrics.services) {
                                if (existing.serviceName == svc.serviceName) { exists = true; break; }
                            }
                            if (!exists) metrics.services.push_back(svc);
                        }
                    }
                    off += sdtLen;
                } else {
                    break;
                }
            }
        }
    } else {
        // Satellite (DVB-S / DVB-S2)
        metrics.modulationName = (pktType == 6 || pktType == 8 || pktType == 15) ? "DVB-S2" : "DVB-S";
        metrics.snrDb = static_cast<float>(packet[2]) / 10.0f;
        metrics.pwrDbuV = static_cast<float>(packet[3]);
        metrics.merDb = static_cast<float>(packet[4]) / 10.0f;
        metrics.lmCnDb = metrics.snrDb - 7.5f;
        metrics.hasLmCn = metrics.carrierLock && (metrics.lmCnDb >= 0.0f);
    }

    // Packet errors
    metrics.packetErrors = (static_cast<uint32_t>(packet[9]) << 8) | packet[10];

    // Constellation points (40 points from byte 22 to 101) - only in constellation-bearing packets
    if (pktType == 3 || pktType == 7 || pktType == 8 || pktType == 9 || pktType == 10 || pktType == 11 || pktType == 14 || pktType == 15) {
        for (int i = 0; i < 40; ++i) {
            int off = 22 + (i * 2);
            int rawI = static_cast<int>(packet[off + 1]) & 0xFF;
            int rawQ = static_cast<int>(packet[off]) & 0xFF;

            if ((rawI * rawI + rawQ * rawQ) < 200 || (rawI == 128 && rawQ == 128)) continue;

            ConstellationPoint pt;
            pt.i = (static_cast<float>(rawI) - 128.0f) / 127.0f;
            pt.q = (static_cast<float>(rawQ) - 128.0f) / 127.0f;
            pt.age = 0.0f;
            pt.timestamp = now;
            constPoints.push_back(pt);
        }

        // Store sufficient constellation history to form dense, focused clusters
        if (constPoints.size() > 3000) {
            constPoints.erase(constPoints.begin(), constPoints.begin() + (constPoints.size() - 3000));
        }
    }

    // Update ages of existing constellation points and erase points older than 8.0s
    constPoints.erase(
        std::remove_if(constPoints.begin(), constPoints.end(), [&](const ConstellationPoint& pt) {
            return std::chrono::duration<float>(now - pt.timestamp).count() > 8.0f;
        }),
        constPoints.end()
    );
    for (auto& pt : constPoints) {
        float elapsedSec = std::chrono::duration<float>(now - pt.timestamp).count();
        pt.age = std::min(1.0f, elapsedSec / 8.0f);
    }

    metrics.lastUpdate = now;
}

} // namespace DrHD
