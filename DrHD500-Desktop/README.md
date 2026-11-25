# Dr.HD 500 Satellite & Terrestrial Combo Analyzer (Desktop C++ App)

A native macOS C++ application for the **Dr.HD 500 Combo Satellite & Terrestrial Meter / Sat Finder**. Built using **Dear ImGui**, **ImPlot**, **GLFW**, **OpenGL**, and **AudioToolbox**.

---

## Features

1. **Direct Bluetooth Serial Communication:**
   - Connects directly to `/dev/cu.DrHD500ComboBT` at 115200 baud, 8N1.
   - 104-byte binary telemetry framing with CRC-8 and 16-bit additive checksums reversed from the official Android APK.
   - Built-in `blueutil` trigger to re-establish Bluetooth baseband link if disconnected.
   - Automatic offline simulation mode when device is unpowered.

2. **Measures & Dashboard:**
   - High-contrast circular analog arc gauge for **SNR (dB)** with color gradation (Red < 6 dB, Yellow 6–12 dB, Green > 12 dB).
   - Horizontal progress meters for **Signal Power Level ($dB\mu V$)** and **MER (dB)**.
   - Pre-BER (Bit Error Rate) and Uncorrected Packet Errors counter.
   - Demodulator & Carrier Lock badges.
   - European UHF Terrestrial Channel grid (CH 21–69, 474–866 MHz) and Satellite LNB LOF tuning presets (Universal Ku 9750/10600 MHz).

3. **60 FPS Real-Time RF Spectrum Analyzer:**
   - Hardware-accelerated sweep rendering with ImPlot.
   - Cyan Neon live sweep trace with shaded area gradient fill.
   - Orange **Max Hold** peak envelope trace.
   - Green exponential **Moving Average** smoothing trace.
   - Real-time Peak Tracking indicator with automatic UHF channel matching.
   - Interactive draggable marker cursor.
   - Span selector (400 MHz Full UHF, 100 MHz, 50 MHz, 8 MHz Channel Zoom, 1200 MHz Full SAT) and Reference Level selector (60–110 $dB\mu V$).

4. **Phosphor Oscilloscope Constellation Scope:**
   - 1:1 circular radar scope with range rings ($0.25, 0.50, 0.707, 1.00$) and In-Phase ($I$) / Quadrature ($Q$) crosshairs.
   - Multi-stage phosphor decay persistence:
     - Age < 0.2: Phosphor Yellow-White (hot)
     - Age < 0.5: Cyan Neon
     - Age < 0.8: Electric Blue
     - Age < 1.0: Deep Blue fade
   - Overlays for ideal constellation grids: **QPSK**, **8-PSK**, **16-QAM**, **64-QAM**, and **256-QAM**.
   - Dynamic real-time calculation of **EVM (% RMS)** and **MER (dB)**.
   - Quadrant zoom controls: Full 1x, 2x Zoom, Q1, Q2, Q3, Q4.

5. **Settings & DiSEqC 1.0 / 1.1 Control:**
   - Port scanner and selector.
   - Offline simulation start/stop toggle.
   - DiSEqC 1.0 4-port switch commander (Port A, B, C, D).
   - 22 kHz continuous tone toggle (Ku high/low band).
   - 13V / 18V LNB voltage selector.
   - Raw Hex DiSEqC command injector.
   - Hardware voltage & battery pack telemetry readouts ($mV$, %, charging state, LNB mA current).

6. **Acoustic Alignment Beeper:**
   - Native macOS `AudioToolbox` sine generator.
   - Pitch dynamically scales with SNR (500 Hz to 2400 Hz).
   - Fast pulsing (8 Hz) on carrier lock for eyes-free antenna alignment.
   - Mute and volume slider in Settings.

---

## Quick Start / Launch

To run the application:

```bash
cd DrHD500-Desktop/build
./DrHD500Desktop
```

To recompile:

```bash
cd DrHD500-Desktop/build
cmake --build . -j4
```
