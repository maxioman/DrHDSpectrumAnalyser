# Dr.HD 500 Combo Satellite Finder — iOS Application

A high-performance, native SwiftUI application for iOS that interfaces with, monitors, and controls the **Dr.HD 500 Combo Satellite Finder** hardware.

Re-engineered directly from decompiled firmware analysis of `Dr.HD500ComboBT.apk` using `jadx`, translating all proprietary RF packet framing, CRC8 calculations, mathematical formulas (DVB-S / DVB-S2 SNR, Power dBµV interpolation, MER, CBER/PE), 300-point spectrum sweep parsing, and 40 I/Q constellation telemetry into modern Swift 6.

---

## 🛰 Features

- **Real-Time Signal Dashboard:**
  - True DVB-S & DVB-S2 SNR extraction (dB)
  - RF Power in dBµV with non-linear interpolation (35 dBµV to 130 dBµV)
  - Pre-Viterbi Channel Bit Error Rate (CBER) & Packet Errors (PE)
  - Multi-stage lock status indicators (Sync, Demod, Carrier, Viterbi)
  - Live DVB Service Information (NIT Network Name, SDS Orbital Position, SDT Channel List)
- **Interactive Audio Beeper:**
  - Real-time pitch-modulated tone synthesizer using `AVAudioEngine` (300 Hz – 1800 Hz) proportional to SNR, replicating the dish alignment beeper.
- **RF Spectrum Analyzer:**
  - 300-point sweep canvas at 60 FPS
  - Max Hold trace recording
  - Peak marker frequency & amplitude readout
  - Span selection (100 MHz, 200 MHz, 500 MHz, Full Ku/C-band)
- **Constellation Diagram:**
  - Real-time 2D I/Q scatter plot (40 pairs normalized around center `128, 128`)
  - Live Modulation Error Ratio (MER) calculation
  - Clear persistence & noise floor tracking
- **Hardware Telemetry Bar:**
  - Battery percentage & charge status
  - LNB supply voltage & current draw (mA)
  - Device PSU voltage, internal temperature, Hardware version, Software build, and Serial Number
- **Satellite & DiSEqC Management:**
  - Pre-loaded database of 141 satellites and 2,970 active transponders
  - 31 LNB Local Oscillator profiles (Universal Ku, C-band 5150, Circular, etc.)
  - DiSEqC 1.0 (4 ports) & DiSEqC 1.1 (16 ports) switching
  - DiSEqC 1.2 Motor drive (East/West continuous & step drive)
  - USALS (Goto X) automatic azimuth/elevation angle calculation

---

## ⚡ Hardware Protocol & Reverse Engineering

### Framing & Transport

| Property | Value | Description |
| :--- | :--- | :--- |
| **Sync Byte** | `0xFE` | Inbound packet header |
| **Packet Length** | `104 bytes` | Fixed frame size |
| **Checksum** | `Big-Endian 16-bit` | $\sum_{i=0}^{101} \text{byte}[i] == (\text{byte}[102] \ll 8) \mid \text{byte}[103]$ |
| **Polling Rate** | `130 ms` | Master timer sending tuning/spectrum/service command packets |
| **Chunking** | `2 bytes` | Firmware UART buffer requires commands to be written in 2-byte slices |
| **Command CRC** | `Maxim 1-Wire CRC-8` | Polynomial `0x8C` (`x^8 + x^5 + x^4 + 1`), initial value `0x00` |

### Signal Math Formulas

- **DVB-S SNR (Packet types 5, 7, 14):**
  $$\text{SNR (dB)} = 10.0 \times \log_{10}\left(\frac{\text{raw}}{8.0}\right) \quad (\text{if raw} > 8)$$
- **DVB-S2 SNR (Packet types 6, 8, 15):**
  $$i_2 = (\text{byte}[3] \ll 6) + (\text{byte}[4] \ \& \ 63)$$
  $$f_2 = \frac{2.0 \times \text{raw}^2}{i_2}$$
  $$\text{SNR (dB)} = 10.0 \times \log_{10}(f_2) \quad (\text{if } f_2 > 1.0)$$
- **RF Power (dBµV):**
  Interpolated from piecewise curve derived from OEM calibration lookup table across 35 dBµV to 130 dBµV.

---

## 🔌 Connecting to Physical Hardware from iOS

The Dr.HD 500 hardware natively uses **Classic Bluetooth SPP (RFCOMM)** with UUID `00001101-0000-1000-8000-00805F9B34FB`. Because Apple restricts Classic Bluetooth SPP on iOS (requiring an MFi cryptographic chip), this app provides three seamless connectivity options:

1. **Hardware Telemetry Simulator (Default):**
   - Built directly into the app. Simulates the Dr.HD 500 microcontroller, responding to tuning commands with authentic 104-byte frames, fluctuating signal lock, 300-point spectrum sweeps, and I/Q constellation data.
2. **CoreBluetooth BLE UART:**
   - Connects to an external BLE-to-Serial bridge (e.g., HM-10, Nordic NUS, or an ESP32) paired with the satellite finder's serial pins. Automatically discovers service UUID `FFE0` / `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`.
3. **Wi-Fi / TCP Socket:**
   - Connects over local Wi-Fi or ad-hoc hotspot to any transparent TCP serial bridge (e.g. ESP8266 / ESP32 running ESP-Link, Elfin-EW10, or USR-WIFI232). Default IP: `192.168.4.1:8080`.

---

## 🛠 Building and Running

### Prerequisites
- macOS 14.0+ with Xcode 15+ / 16+
- Swift 5.9+ / 6.0+

### Option A: Open in Xcode
1. Open `DrHD500-iOS/DrHD500.xcodeproj` in Xcode.
2. Select your target device (iPhone Simulator or Physical iPhone).
3. Press **Cmd + R** to build and run.

### Option B: Command Line Build & Test
```bash
cd DrHD500-iOS

# Run the full unit test suite (checksums, packet framing, CRC8, telemetry):
swift test

# Build for iOS Simulator via xcodebuild:
xcodebuild -project DrHD500.xcodeproj \
           -scheme DrHD500 \
           -sdk iphonesimulator \
           -destination 'platform=iOS Simulator,name=iPhone 17' \
           build
```

---

## 📂 Project Architecture

```
DrHD500-iOS/
├── DrHD500.xcodeproj/              # Native Xcode project
├── Package.swift                   # Swift Package Manager configuration
├── DrHD500/
│   ├── App/
│   │   ├── DrHD500App.swift        # SwiftUI App Entrypoint
│   │   └── Info.plist              # Bluetooth permissions & background modes
│   ├── Audio/
│   │   └── AudioToneGenerator.swift# AVAudioEngine real-time tone generator
│   ├── Communication/
│   │   ├── SatFinderTransport.swift# Abstract transport protocol
│   │   ├── DeviceSession.swift     # Central orchestrator & 130ms poll timer
│   │   ├── BLETransport.swift      # CoreBluetooth BLE UART implementation
│   │   ├── TCPTransport.swift      # Network.framework TCP socket client
│   │   └── MockTransport.swift     # Complete hardware telemetry emulator
│   ├── Models/
│   │   ├── CRC8.swift              # Dallas/Maxim 1-Wire polynomial 0x8C
│   │   ├── DrHDFramer.swift        # 104-byte 0xFE framer & 16-bit checksum
│   │   ├── DrHDPackets.swift       # Command builders (Sat, Spectrum, Service)
│   │   ├── SignalMetrics.swift     # SNR, Power, BER, DVB NIT/SDT decoder
│   │   ├── HardwareTelemetry.swift # Battery, LNB current/voltage, HW/SW build
│   │   ├── SpectrumModel.swift     # 300-point spectrum trace with Max Hold
│   │   ├── ConstellationModel.swift# 40 I/Q point stream & MER math
│   │   ├── SatelliteData.swift     # Transponder & satellite database models
│   │   └── DiSEqCModel.swift       # DiSEqC 1.0/1.1/1.2 & USALS calculators
│   ├── Resources/
│   │   ├── satellites.json         # 141 satellites, 2,970 transponders
│   │   ├── converters.json         # 31 LNB local oscillator profiles
│   │   ├── diseqc.json             # DiSEqC command definitions
│   │   └── beep.wav                # Audio beep waveform
│   └── Views/
│       ├── ContentView.swift       # Mode selector & main shell
│       ├── Dashboard/              # Signal alignment gauges & hardware status
│       ├── Spectrum/               # 60 FPS live spectrum analyzer
│       ├── Constellation/          # 2D I/Q constellation scatter plot
│       ├── Satellites/             # Searchable satellite & transponder picker
│       ├── DiSEqC/                 # Motor drive & switch sheet
│       └── Settings/               # Transport selector (BLE / Wi-Fi / Simulator)
└── Tests/
    └── DrHD500Tests/
        └── DrHD500Tests.swift      # Unit tests verifying reversed protocol logic
```
