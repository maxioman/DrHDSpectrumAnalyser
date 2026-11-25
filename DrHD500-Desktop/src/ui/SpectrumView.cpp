#include "SpectrumView.hpp"
#include "data/TerrestrialDB.hpp"
#include "core/RFBandClassifier.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace DrHD {

static bool showLive = true;
static bool showMaxHold = true;
static bool showAverage = false;
static bool trackPeak = true;
static double markerFreqMHz = 802.0; // Default to 802 MHz (user's requested inspection peak)
static int selectedSpanIdx = 0;
static int selectedRefIdx = 5;

// Demodulator Probe State
static bool isProbingDemod = false;
static double probedFreqMHz = 0.0;
static std::chrono::steady_clock::time_point probeStartTime;

void SpectrumView::render(SerialPort& serial) {
    SignalMetrics metrics;
    HardwareTelemetry hw;
    std::vector<ConstellationPoint> constPts;
    SpectrumData spec;
    serial.getTelemetry(metrics, hw, constPts, spec);
    TunedInfo tp = serial.getTunedInfo();

    // If probing demodulator, maintain TerSNR mode to query hardware COFDM demodulator.
    // Automatically resume SpectrumRDA mode after 3 seconds so live sweeps keep running continuously.
    if (isProbingDemod) {
        serial.setMode(CmdMode::TerSNR);
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - probeStartTime).count();
        if (elapsedMs > 3000) {
            isProbingDemod = false;
            serial.setMode(CmdMode::SpectrumRDA);
        }
    } else {
        serial.setMode(CmdMode::SpectrumRDA);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float availW = ImGui::GetContentRegionAvail().x;

    // 1. Top Cyan Status Strip (TV 0V BTQ:100%)
    ImVec2 cyanPos = ImGui::GetCursorScreenPos();
    float cyanH = 24.0f;
    drawList->AddRectFilled(cyanPos, ImVec2(cyanPos.x + availW, cyanPos.y + cyanH), IM_COL32(0, 180, 235, 255));
    
    std::string stripText;
    if (tp.band == WorkingBand::Satellite) {
        stripText = "SAT " + tp.polarization + " BTQ:100%";
    } else {
        stripText = "TV 0V BTQ:100%";
    }
    ImVec2 strSz = ImGui::CalcTextSize(stripText.c_str());
    drawList->AddText(ImVec2(cyanPos.x + (availW - strSz.x) * 0.5f, cyanPos.y + (cyanH - strSz.y) * 0.5f), IM_COL32(0, 0, 0, 255), stripText.c_str());
    ImGui::Dummy(ImVec2(availW, cyanH + 4.0f));

    // 2. Active Transponder / Mux Banner
    ImVec2 bannerPos = ImGui::GetCursorScreenPos();
    float bannerH = 30.0f;
    drawList->AddRectFilled(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(18, 22, 28, 255));
    drawList->AddRect(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(50, 65, 80, 255));
    
    std::string bannerStr = tp.getBanner(metrics);
    ImVec2 bSz = ImGui::CalcTextSize(bannerStr.c_str());
    drawList->AddText(ImVec2(bannerPos.x + (availW - bSz.x) * 0.5f, bannerPos.y + (bannerH - bSz.y) * 0.5f), IM_COL32(245, 248, 250, 255), bannerStr.c_str());
    ImGui::Dummy(ImVec2(availW, bannerH + 6.0f));

    // 3. Top Controls: [ SP:700MHz ] and [ Ref.L:50 ]
    const char* spans[] = { "SP:700MHz", "SP:100MHz", "SP:50MHz", "SP:8MHz" };
    int spanValues[] = { 700, 100, 50, 8 };
    static int currentSpanIdx = 0;
    for (int s = 0; s < IM_ARRAYSIZE(spanValues); ++s) {
        if (spanValues[s] == spec.spanMHz) {
            currentSpanIdx = s;
            break;
        }
    }

    const char* refs[] = { "Ref.L:50", "Ref.L:60", "Ref.L:70", "Ref.L:80", "Ref.L:85", "Ref.L:90" };
    int refValues[] = { 50, 60, 70, 80, 85, 90 };
    static int currentRefIdx = 0;
    for (int r = 0; r < IM_ARRAYSIZE(refValues); ++r) {
        if (refValues[r] == spec.refLevelDbuV) {
            currentRefIdx = r;
            break;
        }
    }

    ImGui::SetNextItemWidth(140);
    if (ImGui::Combo("##SpanCombo", &currentSpanIdx, spans, IM_ARRAYSIZE(spans))) {
        int newSpan = spanValues[currentSpanIdx];
        int startFreq = 250;
        if (newSpan == 700) {
            startFreq = 250;
        } else {
            startFreq = std::max(250, static_cast<int>(tp.frequencyMHz - (newSpan / 2)));
        }
        serial.setSpectrumTuning(startFreq, newSpan, refValues[currentRefIdx]);
    }

    ImGui::SameLine(0.0f, 15.0f);
    ImGui::SetNextItemWidth(130);
    if (ImGui::Combo("##RefCombo", &currentRefIdx, refs, IM_ARRAYSIZE(refs))) {
        int startFreq = (spec.spanMHz == 700) ? 250 : std::max(250, static_cast<int>(tp.frequencyMHz - (spec.spanMHz / 2)));
        serial.setSpectrumTuning(startFreq, spec.spanMHz, refValues[currentRefIdx]);
    }

    ImGui::SameLine(0.0f, 20.0f);
    const auto& uhfChannels = TerrestrialDB::instance().getUHFChannels();
    if (ImGui::Button("< Prev CH##Spec")) {
        for (size_t i = 0; i < uhfChannels.size(); ++i) {
            if (std::abs(uhfChannels[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i > 0) {
                const auto& prevCh = uhfChannels[i - 1];
                serial.setTerrestrialTuning(static_cast<int>(prevCh.frequencyMHz), prevCh.isT2, 0, 0);
                break;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next CH >##Spec")) {
        for (size_t i = 0; i < uhfChannels.size(); ++i) {
            if (std::abs(uhfChannels[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i + 1 < uhfChannels.size()) {
                const auto& nextCh = uhfChannels[i + 1];
                serial.setTerrestrialTuning(static_cast<int>(nextCh.frequencyMHz), nextCh.isT2, 0, 0);
                break;
            }
        }
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Peak: %.1f MHz (%.0f dBµV)", spec.peakFreqMHz, spec.peakDbuV);

    ImGui::Spacing();

    // 4. Main Spectrometer Graph (Retained Full Original Dimensions)
    float spanStart = (spec.spanMHz == 700) ? 250.0f : (spec.centerFreqMHz - (spec.spanMHz / 2.0f));
    float spanEnd = spanStart + spec.spanMHz;
    float refMax = static_cast<float>(refValues[currentRefIdx]);

    std::vector<double> xs(300);
    std::vector<double> ys(300);
    for (int i = 0; i < 300; ++i) {
        xs[i] = spanStart + (static_cast<float>(i) / 299.0f) * spec.spanMHz;
        ys[i] = (i < static_cast<int>(spec.levels.size())) ? std::max(0.0f, std::min(refMax, spec.levels[i])) : 15.0;
    }

    // Retained original expansive plot height
    float plotHeight = ImGui::GetContentRegionAvail().y - 105.0f;
    ImVec2 plotSize(-1, std::max(360.0f, plotHeight));

    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    ImVec2 plotScreenPos(0, 0);
    ImVec2 plotScreenSize(0, 0);

    if (ImPlot::BeginPlot("##RFSpectrumPlot", plotSize, ImPlotFlags_NoTitle | ImPlotFlags_Crosshairs)) {
        ImPlot::SetupAxes("Frequency (MHz)", "Amplitude (dBµV)", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
        ImPlot::SetupAxisLimits(ImAxis_X1, spanStart, spanEnd, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, static_cast<double>(refMax), ImPlotCond_Always);

        plotScreenPos = ImPlot::GetPlotPos();
        plotScreenSize = ImPlot::GetPlotSize();

        // Continuous Live Inspection: tracks cursor live when mouse is over the graph
        if (ImPlot::IsPlotHovered()) {
            ImPlotPoint mousePt = ImPlot::GetPlotMousePos();
            if (mousePt.x >= spanStart && mousePt.x <= spanEnd) {
                markerFreqMHz = std::round(mousePt.x * 2.0) / 2.0; // snap to 0.5 MHz
            }
        }

        // Shaded Band Regions for Context
        // 1. LTE 800 Band 20 Downlink (791 - 821 MHz)
        if (spanStart <= 821.0f && spanEnd >= 791.0f) {
            double lteX[2] = { std::max(static_cast<double>(spanStart), 791.0), std::min(static_cast<double>(spanEnd), 821.0) };
            double lteY1[2] = { 0.0, 0.0 };
            double lteY2[2] = { static_cast<double>(refMax), static_cast<double>(refMax) };
            ImPlotSpec lteSpec;
            lteSpec.FillColor = ImVec4(1.0f, 0.35f, 0.1f, 0.12f); // Orange-red shading
            ImPlot::PlotShaded("##LTE800Bg", lteX, lteY1, lteY2, 2, lteSpec);
        }

        // 2. GSM / LTE 900 Band (925 - 960 MHz)
        if (spanStart <= 960.0f && spanEnd >= 925.0f) {
            double gsmX[2] = { std::max(static_cast<double>(spanStart), 925.0), std::min(static_cast<double>(spanEnd), 960.0) };
            double gsmY1[2] = { 0.0, 0.0 };
            double gsmY2[2] = { static_cast<double>(refMax), static_cast<double>(refMax) };
            ImPlotSpec gsmSpec;
            gsmSpec.FillColor = ImVec4(0.9f, 0.15f, 0.5f, 0.10f); // Purple shading
            ImPlot::PlotShaded("##GSM900Bg", gsmX, gsmY1, gsmY2, 2, gsmSpec);
        }

        // 3. UHF DTT TV Broadcast Band (470 - 694 MHz)
        if (spanStart <= 694.0f && spanEnd >= 470.0f) {
            double tvX[2] = { std::max(static_cast<double>(spanStart), 470.0), std::min(static_cast<double>(spanEnd), 694.0) };
            double tvY1[2] = { 0.0, 0.0 };
            double tvY2[2] = { static_cast<double>(refMax), static_cast<double>(refMax) };
            ImPlotSpec tvSpec;
            tvSpec.FillColor = ImVec4(0.0f, 0.7f, 0.8f, 0.06f); // Subtle cyan tint
            ImPlot::PlotShaded("##DvbTvBg", tvX, tvY1, tvY2, 2, tvSpec);
        }

        // Background shaded band for FM Radio (87.5 - 108.0 MHz)
        if (spanStart <= 108.0f && spanEnd >= 87.5f) {
            double fmX[2] = { std::max(static_cast<double>(spanStart), 87.5), std::min(static_cast<double>(spanEnd), 108.0) };
            double fmY1[2] = { 0.0, 0.0 };
            double fmY2[2] = { static_cast<double>(refMax), static_cast<double>(refMax) };
            ImPlotSpec fmSpec;
            fmSpec.FillColor = ImVec4(0.5f, 0.2f, 0.8f, 0.07f); // Subtle violet tint
            ImPlot::PlotShaded("##FmRadioBg", fmX, fmY1, fmY2, 2, fmSpec);
        }

        // Background shaded band for VHF Band III (174 - 230 MHz)
        if (spanStart <= 230.0f && spanEnd >= 174.0f) {
            double vhfX[2] = { std::max(static_cast<double>(spanStart), 174.0), std::min(static_cast<double>(spanEnd), 230.0) };
            double vhfY1[2] = { 0.0, 0.0 };
            double vhfY2[2] = { static_cast<double>(refMax), static_cast<double>(refMax) };
            ImPlotSpec vhfSpec;
            vhfSpec.FillColor = ImVec4(0.1f, 0.8f, 0.4f, 0.07f); // Subtle green tint
            ImPlot::PlotShaded("##VhfBandBg", vhfX, vhfY1, vhfY2, 2, vhfSpec);
        }

        // Channel Raster Vertical Lines (Dark Teal / Cyan)
        auto drawRaster = [&](const std::vector<ChannelInfo>& chList) {
            for (const auto& ch : chList) {
                if (ch.frequencyMHz >= spanStart && ch.frequencyMHz <= spanEnd) {
                    double chF = ch.frequencyMHz;
                    ImPlotSpec gridSpec;
                    gridSpec.LineColor = ImVec4(0.0f, 0.45f, 0.50f, 0.35f);
                    gridSpec.LineWeight = 1.0f;
                    ImPlot::PlotInfLines("##ChGrid", &chF, 1, gridSpec);
                }
            }
        };
        drawRaster(TerrestrialDB::instance().getUHFChannels());
        drawRaster(TerrestrialDB::instance().getVHFChannels());
        drawRaster(TerrestrialDB::instance().getVHFLowChannels());
        drawRaster(TerrestrialDB::instance().getCableChannels());

        // 1. Canary Yellow Filled Silhouette
        ImPlotSpec fillSpec;
        fillSpec.FillColor = ImVec4(1.0f, 0.85f, 0.08f, 1.0f);
        ImPlot::PlotShaded("##LiveYellowFill", xs.data(), ys.data(), 300, 0.0, fillSpec);

        // 2. Royal Blue Upper Contour Line
        ImPlotSpec blueSpec;
        blueSpec.LineColor = ImVec4(0.10f, 0.50f, 1.0f, 1.0f);
        blueSpec.LineWeight = 2.0f;
        ImPlot::PlotLine("##LiveBlueLine", xs.data(), ys.data(), 300, blueSpec);

        // 3. Red Vertical Cursor Line at Active Channel Frequency (e.g. 474 MHz)
        double curFreq = tp.frequencyMHz;
        ImPlotSpec curSpec;
        curSpec.LineColor = ImVec4(0.95f, 0.15f, 0.15f, 1.0f);
        curSpec.LineWeight = 1.8f;
        ImPlot::PlotInfLines("##CursorLine", &curFreq, 1, curSpec);

        // Red horizontal marker tick on cursor at tuned signal RSSI level
        double tickX[2] = { curFreq - 6.0, curFreq + 6.0 };
        double tickY[2] = { static_cast<double>(metrics.pwrDbuV), static_cast<double>(metrics.pwrDbuV) };
        ImPlotSpec tickSpec;
        tickSpec.LineColor = ImVec4(0.95f, 0.15f, 0.15f, 1.0f);
        tickSpec.LineWeight = 3.0f;
        ImPlot::PlotLine("##CursorTick", tickX, tickY, 2, tickSpec);

        // 4. Gold / Amber Vertical Line at Inspector Marker (markerFreqMHz)
        double inspFreq = markerFreqMHz;
        ImPlotSpec inspSpec;
        inspSpec.LineColor = ImVec4(1.0f, 0.82f, 0.15f, 1.0f);
        inspSpec.LineWeight = 2.0f;
        ImPlot::PlotInfLines("##InspectorLine", &inspFreq, 1, inspSpec);

        // Compute estimated power at marker frequency from spectrum levels
        float markerPowerDbuV = 0.0f;
        if (spec.spanMHz > 0 && !spec.levels.empty()) {
            float frac = static_cast<float>((markerFreqMHz - spanStart) / spec.spanMHz);
            frac = std::max(0.0f, std::min(1.0f, frac));
            int idx = static_cast<int>(std::round(frac * (spec.levels.size() - 1)));
            if (idx >= 0 && idx < static_cast<int>(spec.levels.size())) {
                markerPowerDbuV = spec.levels[idx];
            }
        }

        // Horizontal marker tick at inspector power
        double inspTickX[2] = { markerFreqMHz - 5.0, markerFreqMHz + 5.0 };
        double inspTickY[2] = { static_cast<double>(markerPowerDbuV), static_cast<double>(markerPowerDbuV) };
        ImPlotSpec inspTickSpec;
        inspTickSpec.LineColor = ImVec4(1.0f, 0.95f, 0.35f, 1.0f);
        inspTickSpec.LineWeight = 3.0f;
        ImPlot::PlotLine("##InspectorTick", inspTickX, inspTickY, 2, inspTickSpec);

        // Informational band labels inside plot
        if (spanStart <= 806.0f && spanEnd >= 806.0f) {
            ImPlotSpec lteLblSpec;
            lteLblSpec.LineColor = ImVec4(1.0f, 0.55f, 0.35f, 0.9f);
            ImPlot::PlotText("LTE-800 B20 Downlink (791-821M)", 806.0, refMax - 4.0, ImVec2(0, 0), lteLblSpec);
        }
        if (spanStart <= 582.0f && spanEnd >= 582.0f) {
            ImPlotSpec dvbLblSpec;
            dvbLblSpec.LineColor = ImVec4(0.2f, 0.85f, 0.9f, 0.7f);
            ImPlot::PlotText("DVB-T/T2 TV Broadcast (470-694M)", 582.0, refMax - 4.0, ImVec2(0, 0), dvbLblSpec);
        }

        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);

    // 5. TOP-RIGHT FLOATING SIGNAL INSPECTOR PANEL (User requested top-right placement)
    if (plotScreenSize.x > 380.0f && plotScreenSize.y > 200.0f) {
        float panelW = 345.0f;
        ImVec2 panelPos(plotScreenPos.x + plotScreenSize.x - panelW - 12.0f, plotScreenPos.y + 12.0f);

        ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.88f);

        ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoNav;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.13f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.28f, 0.38f, 0.90f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

        if (ImGui::Begin("##SignalInspectorTopRight", nullptr, overlayFlags)) {
            // Frequency classification lookup
            BandInspection insp = RFBandClassifier::inspect(static_cast<float>(markerFreqMHz));

            // Calculate marker amplitude from spectrum trace
            float markerPowerDbuV = 0.0f;
            if (spec.spanMHz > 0 && !spec.levels.empty()) {
                float frac = static_cast<float>((markerFreqMHz - spanStart) / spec.spanMHz);
                frac = std::max(0.0f, std::min(1.0f, frac));
                int idx = static_cast<int>(std::round(frac * (spec.levels.size() - 1)));
                if (idx >= 0 && idx < static_cast<int>(spec.levels.size())) {
                    markerPowerDbuV = spec.levels[idx];
                }
            }

            // Header: Live Target Frequency & Power
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "🔍 LIVE SIGNAL INSPECTOR");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.6f, 0.7f, 1.0f), "(Hover graph)");

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%.1f MHz", markerFreqMHz);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "| Level: %.1f dBµV", markerPowerDbuV);

            // Classification badge
            if (insp.isCellular) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.2f, 1.0f), "● [ %s ]", insp.category.c_str());
            } else if (insp.isDvbTv) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "● [ %s ]", insp.category.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "● [ %s ]", insp.category.c_str());
            }

            ImGui::Separator();

            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Band: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", insp.bandName.c_str());

            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Tech: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.8f, 1.0f), "%s", insp.technology.c_str());

            if (insp.isCellular) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Note: High power cell tower (Digital Dividend 1).");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Note: Terrestrial broadcast TV multiplex.");
            }

            ImGui::Spacing();

            // Preset Target Buttons
            if (ImGui::SmallButton("802M LTE")) { markerFreqMHz = 802.0; }
            ImGui::SameLine();
            if (ImGui::SmallButton("474M CH21")) { markerFreqMHz = 474.0; }
            ImGui::SameLine();
            if (ImGui::SmallButton("610M CH38")) { markerFreqMHz = 610.0; }
            ImGui::SameLine();
            if (ImGui::SmallButton("935M GSM")) { markerFreqMHz = 935.0; }
            if (spec.peakFreqMHz > 100.0f) {
                ImGui::SameLine();
                char pBuf[16];
                snprintf(pBuf, sizeof(pBuf), "Peak %.0fM", spec.peakFreqMHz);
                if (ImGui::SmallButton(pBuf)) { markerFreqMHz = spec.peakFreqMHz; }
            }

            ImGui::Separator();

            // Hardware Demodulator Probe Action
            char probeBtn[48];
            snprintf(probeBtn, sizeof(probeBtn), "📡 Probe Demod (%.1fM)", markerFreqMHz);
            if (ImGui::Button(probeBtn, ImVec2(panelW - 20.0f, 24.0f))) {
                isProbingDemod = true;
                probedFreqMHz = markerFreqMHz;
                probeStartTime = std::chrono::steady_clock::now();
                serial.setTerrestrialTuning(static_cast<int>(markerFreqMHz), true, 0, 0);
                serial.setMode(CmdMode::TerSNR);
            }

            if (isProbingDemod) {
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - probeStartTime).count();
                if (std::abs(tp.frequencyMHz - probedFreqMHz) < 1.0f) {
                    if (metrics.carrierLock) {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "✅ DVB LOCK: SNR: %.1f dB, PWR: %.0f dBµV", metrics.snrDb, metrics.pwrDbuV);
                    } else if (elapsedMs >= 1500) {
                        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "❌ NO DVB SYNC: Confirmed Cellular/LTE.");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "⏳ Testing COFDM framing sync...");
                    }
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // 6. Controls below plot: [ LNB: 0V ] and [ BAND ]
    if (ImGui::Button("LNB: 0V", ImVec2(100, 26))) {
        // Toggle LNB voltage
    }
    ImGui::SameLine(0.0f, 12.0f);
    if (ImGui::Button("BAND", ImVec2(100, 26))) {
        int next = (static_cast<int>(tp.band) + 1) % 5;
        serial.setWorkingBand(static_cast<WorkingBand>(next));
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextColored(ImVec4(0.85f, 0.9f, 0.95f, 1.0f), "Tuned Ch RSSI: %.0f dBµV", metrics.pwrDbuV);

    ImGui::Spacing();

    // 7. Horizontal RSSI scale bar
    ImVec2 rssiBarPos = ImGui::GetCursorScreenPos();
    float rssiBarW = availW - 8.0f;
    float rssiBarH = 14.0f;
    float rssiMin = -50.0f;
    float rssiMax = 140.0f;
    float clampedRSSI = std::max(rssiMin, std::min(rssiMax, metrics.pwrDbuV));
    float fillFrac = (clampedRSSI - rssiMin) / (rssiMax - rssiMin);

    drawList->AddRect(rssiBarPos, ImVec2(rssiBarPos.x + rssiBarW, rssiBarPos.y + rssiBarH), IM_COL32(180, 185, 195, 255), 1.0f, 0, 1.0f);
    if (fillFrac > 0.01f) {
        float fW = fillFrac * rssiBarW;
        drawList->AddRectFilled(ImVec2(rssiBarPos.x + 1, rssiBarPos.y + 1), ImVec2(rssiBarPos.x + fW, rssiBarPos.y + rssiBarH - 1), IM_COL32(235, 40, 30, 255));
    }
    ImGui::Dummy(ImVec2(rssiBarW, rssiBarH + 6.0f));
}

} // namespace DrHD


