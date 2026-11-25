#pragma once

#include <string>
#include <vector>
#include <cmath>

namespace DrHD {

struct BandInspection {
    float freqMHz = 0.0f;
    std::string bandName;         // e.g. "4G LTE Band 20 (Cellular Downlink)"
    std::string category;         // "Cellular Telecom Base Station" / "Digital Terrestrial TV (DTT)" / etc.
    std::string technology;       // "3GPP LTE / 5G NR n20 (MIMO OFDM)"
    std::string channelOrService; // "eNodeB Base Station Downlink"
    std::string technicalDetails; // Detailed RF explanation
    std::string interferenceNote; // Advice regarding filters / interference
    bool isDvbTv = false;
    bool isCellular = false;
    int legacyUhfChannel = 0;     // e.g. 62
};

class RFBandClassifier {
public:
    static BandInspection inspect(float freqMHz) {
        BandInspection b;
        b.freqMHz = freqMHz;

        if (freqMHz >= 470.0f && freqMHz <= 694.0f) {
            b.isDvbTv = true;
            b.category = "Digital Terrestrial TV (DTT Broadcast)";
            b.bandName = "UHF Broadcast TV (ITU Region 1 Protected)";
            b.technology = "DVB-T / DVB-T2 / ISDB-T (COFDM, 8 MHz)";
            int ch = static_cast<int>(std::round((freqMHz - 474.0f) / 8.0f)) + 21;
            b.legacyUhfChannel = ch;
            b.channelOrService = "DTT Channel " + std::to_string(ch) + " (" + std::to_string(static_cast<int>(474 + (ch - 21) * 8)) + " MHz center)";
            b.technicalDetails = "Standard terrestrial television broadcast spectrum. Features steep brick-wall OFDM filtering (8 MHz bandwidth) carrying QPSK/16QAM/64QAM/256QAM digital multiplexes.";
            b.interferenceNote = "Protected broadcast band for digital television. Should demodulate cleanly with antenna pointed at transmitter.";
        }
        else if (freqMHz >= 791.0f && freqMHz <= 821.0f) {
            b.isCellular = true;
            b.category = "4G LTE / 5G Mobile Base Station (Tower Downlink)";
            b.bandName = "800 MHz Digital Dividend 1 (3GPP Band 20 / n20)";
            b.technology = "LTE-FDD / 5G NR (MIMO OFDMA, 10 - 20 MHz block)";
            int legacyCh = static_cast<int>(std::round((freqMHz - 474.0f) / 8.0f)) + 21;
            b.legacyUhfChannel = legacyCh;
            b.channelOrService = "Cellular eNodeB / gNodeB Downlink (791 - 821 MHz)";
            b.technicalDetails = "Continuous high-power mobile telecom base station downlink. In the ITU WRC-12 Digital Dividend 1 auction, channels 61-69 (790-862 MHz) were cleared from television and licensed to mobile network operators (e.g. du, Etisalat, Vodafone, Zain, STC).";
            b.interferenceNote = "⚠️ HIGH INTERFERENCE SOURCE: Because cell towers transmit high power continuously, TV antennas pick up massive 800 MHz energy (often 50-70 dBuV). This overloads TV tuner front-ends. Recommended: Install an inline 790 MHz LTE rejection low-pass filter.";
        }
        else if (freqMHz >= 832.0f && freqMHz <= 862.0f) {
            b.isCellular = true;
            b.category = "4G LTE Mobile Phone (Handset Uplink)";
            b.bandName = "800 MHz Digital Dividend Uplink (3GPP Band 20)";
            b.technology = "LTE-FDD SC-FDMA (Handset transmission)";
            b.channelOrService = "Mobile Phone to Tower (832 - 862 MHz)";
            b.technicalDetails = "Uplink frequency band used by mobile handsets transmitting back to the cell tower.";
            b.interferenceNote = "Mobile phone transmissions in the immediate vicinity.";
        }
        else if (freqMHz >= 694.0f && freqMHz <= 790.0f) {
            b.isCellular = true;
            b.category = "700 MHz Mobile Telecom (Digital Dividend 2)";
            b.bandName = "3GPP Band 28 (LTE / 5G NR n28)";
            b.technology = "LTE-FDD / 5G NR (MIMO OFDMA)";
            int legacyCh = static_cast<int>(std::round((freqMHz - 474.0f) / 8.0f)) + 21;
            b.legacyUhfChannel = legacyCh;
            b.channelOrService = (freqMHz >= 758.0f) ? "Cellular Downlink (758 - 788 MHz)" : "Cellular Uplink (703 - 733 MHz)";
            b.technicalDetails = "Cleared from TV broadcasting in the 700 MHz reallocation (UHF channels 49-60) and assigned to 4G/5G mobile carriers.";
            b.interferenceNote = "High power mobile network signals. Can cause front-end overload on wideband TV preamps.";
        }
        else if (freqMHz >= 925.0f && freqMHz <= 960.0f) {
            b.isCellular = true;
            b.category = "GSM-900 / UMTS / 4G LTE Band 8 Downlink";
            b.bandName = "900 MHz Cellular Band (Tower Downlink)";
            b.technology = "GSM GMSK / UMTS WCDMA / LTE-FDD (Downlink)";
            b.channelOrService = "Cellular Base Station Transmit (925 - 960 MHz)";
            b.technicalDetails = "Classic 900 MHz cellular band used globally for 2G GSM, 3G UMTS, and LTE Band 8 mobile communications.";
            b.interferenceNote = "High power mobile telecom transmitters located immediately above the UHF band.";
        }
        else if (freqMHz >= 880.0f && freqMHz < 915.0f) {
            b.isCellular = true;
            b.category = "GSM-900 / LTE Band 8 Uplink";
            b.bandName = "900 MHz Cellular Band (Handset Uplink)";
            b.technology = "GSM / LTE (Mobile Handset)";
            b.channelOrService = "Cellular Mobile Handsets (880 - 915 MHz)";
            b.technicalDetails = "Mobile phones transmitting upstream to 900 MHz cellular base stations.";
            b.interferenceNote = "Burst emissions from active 2G/4G smartphones.";
        }
        else if (freqMHz >= 863.0f && freqMHz <= 870.0f) {
            b.category = "Short Range Devices / ISM / IoT";
            b.bandName = "868 MHz SRD / ISM Band";
            b.technology = "LoRaWAN, Sigfox, Zigbee, RFID, Wireless Alarms";
            b.channelOrService = "License-Free IoT & Telemetry (868 MHz)";
            b.technicalDetails = "Short range wireless sensors, smart meters, garage doors, and wireless alarm systems.";
            b.interferenceNote = "Low power license-free devices.";
        }
        else if (freqMHz >= 174.0f && freqMHz <= 230.0f) {
            b.isDvbTv = true;
            b.category = "VHF Band III (DAB+ Radio / DVB-T)";
            b.bandName = "VHF Band III (Channels 5 - 12)";
            b.technology = "DAB / DAB+ (OFDM 1.5 MHz) or DVB-T (7 MHz)";
            b.channelOrService = "Terrestrial VHF Digital Radio & TV";
            b.technicalDetails = "Used throughout Europe, Middle East, and Australia for DAB/DAB+ digital radio multiplexes and VHF digital TV.";
            b.interferenceNote = "Standard VHF broadcast band.";
        }
        else if (freqMHz >= 250.0f && freqMHz < 470.0f) {
            b.category = "Cable TV / PMR / Military / Fixed Mobile";
            b.bandName = "VHF/UHF Mid-Band";
            b.technology = "DVB-C / Analog CATV / Tetra / Military Communications";
            b.channelOrService = "Cable TV Hyperband / Two-Way Radio";
            b.technicalDetails = "CATV distribution channels, professional mobile radio (PMR), aeronautical, and government allocations.";
            b.interferenceNote = "Non-broadcast terrestrial spectrum.";
        }
        else if (freqMHz >= 950.0f && freqMHz <= 2150.0f) {
            b.category = "Satellite L-Band Intermediate Frequency";
            b.bandName = "DVB-S / DVB-S2 Satellite IF";
            b.technology = "DVB-S (QPSK) / DVB-S2 (8PSK/16APSK/32APSK)";
            b.channelOrService = "Downconverted LNB Satellite Signal (950 - 2150 MHz)";
            b.technicalDetails = "Output of standard satellite dish LNBs (Universal Ku-band 9.75/10.6 GHz, C-band 5.15 GHz).";
            b.interferenceNote = "Satellite receiver downlead spectrum.";
        }
        else {
            b.category = "Radio Frequency Spectrum";
            b.bandName = "RF Band";
            b.technology = "RF Emission";
            b.channelOrService = "Carrier at " + std::to_string(static_cast<int>(freqMHz)) + " MHz";
            b.technicalDetails = "General radio frequency spectrum emission.";
            b.interferenceNote = "Observe demodulator sync and bandwidth to identify.";
        }

        return b;
    }
};

} // namespace DrHD
