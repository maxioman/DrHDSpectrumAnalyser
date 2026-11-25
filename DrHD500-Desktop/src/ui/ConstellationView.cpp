#include "ConstellationView.hpp"
#include "data/TerrestrialDB.hpp"
#include "data/SatelliteDB.hpp"
#include <imgui.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace DrHD {

enum class ZoomMode {
    Full,
    Zoom2x,
    Quad1, // +I, +Q
    Quad2, // -I, +Q
    Quad3, // -I, -Q
    Quad4  // +I, -Q
};

static ZoomMode currentZoom = ZoomMode::Full;
static int selectedModIdx = 0; // 0 = Auto, 1 = QPSK, 2 = 8-PSK, 3 = 16-QAM, 4 = 64-QAM, 5 = 256-QAM
static bool showIdealGrid = false;
static bool showPersistence = true;

// Helper to generate ideal constellation points for reference overlays
static std::vector<ImVec2> getIdealPoints(int modIdx, const std::string& autoName) {
    std::vector<ImVec2> pts;
    int mode = modIdx;
    if (mode == 0) { // Auto
        if (autoName.find("8PSK") != std::string::npos || autoName.find("8-PSK") != std::string::npos) mode = 2;
        else if (autoName.find("16QAM") != std::string::npos || autoName.find("16-QAM") != std::string::npos) mode = 3;
        else if (autoName.find("64QAM") != std::string::npos || autoName.find("64-QAM") != std::string::npos) mode = 4;
        else if (autoName.find("256QAM") != std::string::npos || autoName.find("256-QAM") != std::string::npos) mode = 5;
        else mode = 1; // Default QPSK
    }

    if (mode == 1) { // QPSK
        const float val = 0.7071f;
        pts.push_back(ImVec2( val,  val));
        pts.push_back(ImVec2(-val,  val));
        pts.push_back(ImVec2(-val, -val));
        pts.push_back(ImVec2( val, -val));
    } else if (mode == 2) { // 8-PSK
        for (int k = 0; k < 8; ++k) {
            float angle = k * (3.14159265f / 4.0f) + (3.14159265f / 8.0f);
            pts.push_back(ImVec2(std::cos(angle), std::sin(angle)));
        }
    } else if (mode == 3) { // 16-QAM (4x4)
        const float levels[] = { -0.9487f, -0.3162f, 0.3162f, 0.9487f };
        for (float i : levels) {
            for (float q : levels) {
                pts.push_back(ImVec2(i, q));
            }
        }
    } else if (mode == 4) { // 64-QAM (8x8)
        const float s = 1.0f / std::sqrt(42.0f);
        for (int i = -7; i <= 7; i += 2) {
            for (int q = -7; q <= 7; q += 2) {
                pts.push_back(ImVec2(i * s, q * s));
            }
        }
    } else if (mode == 5) { // 256-QAM (16x16)
        const float s = 1.0f / std::sqrt(170.0f);
        for (int i = -15; i <= 15; i += 2) {
            for (int q = -15; q <= 15; q += 2) {
                pts.push_back(ImVec2(i * s, q * s));
            }
        }
    }

    return pts;
}

void ConstellationView::render(SerialPort& serial) {
    // Ensure serial driver is querying measures & constellation for active band
    if (serial.getWorkingBand() == WorkingBand::Satellite) {
        serial.setMode(CmdMode::SatSNR);
    } else {
        serial.setMode(CmdMode::TerSNR);
    }

    SignalMetrics metrics;
    HardwareTelemetry hw;
    std::vector<ConstellationPoint> constPts;
    SpectrumData spec;
    serial.getTelemetry(metrics, hw, constPts, spec);
    TunedInfo tp = serial.getTunedInfo();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y;

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

    // 2. Active Transponder / Mux Banner (610M CH38 GI:1/32 32K 3/5 64QAM 8M T2)
    ImVec2 bannerPos = ImGui::GetCursorScreenPos();
    float bannerH = 30.0f;
    drawList->AddRectFilled(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(18, 22, 28, 255));
    drawList->AddRect(bannerPos, ImVec2(bannerPos.x + availW, bannerPos.y + bannerH), IM_COL32(50, 65, 80, 255));
    
    std::string bannerStr = tp.getBanner(metrics);
    ImVec2 bSz = ImGui::CalcTextSize(bannerStr.c_str());
    drawList->AddText(ImVec2(bannerPos.x + (availW - bSz.x) * 0.5f, bannerPos.y + (bannerH - bSz.y) * 0.5f), IM_COL32(245, 248, 250, 255), bannerStr.c_str());
    ImGui::Dummy(ImVec2(availW, bannerH + 4.0f));

    // 3. Compact Control Toolbar: Quick CH step buttons & scope toggles
    ImGui::BeginGroup();
    const auto& uhfChannels = TerrestrialDB::instance().getUHFChannels();
    if (ImGui::Button("< Prev CH")) {
        for (size_t i = 0; i < uhfChannels.size(); ++i) {
            if (std::abs(uhfChannels[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i > 0) {
                const auto& prevCh = uhfChannels[i - 1];
                serial.setTerrestrialTuning(static_cast<int>(prevCh.frequencyMHz), prevCh.isT2, 0, 0);
                break;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next CH >")) {
        for (size_t i = 0; i < uhfChannels.size(); ++i) {
            if (std::abs(uhfChannels[i].frequencyMHz - tp.frequencyMHz) < 1.0f && i + 1 < uhfChannels.size()) {
                const auto& nextCh = uhfChannels[i + 1];
                serial.setTerrestrialTuning(static_cast<int>(nextCh.frequencyMHz), nextCh.isT2, 0, 0);
                break;
            }
        }
    }

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Clear Screen")) {
        serial.clearConstellation();
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("Persistence", &showPersistence);
    ImGui::SameLine(0.0f, 15.0f);
    ImGui::Checkbox("Ideal Grid", &showIdealGrid);

    ImGui::SameLine(0.0f, 25.0f);
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    if (ImGui::RadioButton("1x", currentZoom == ZoomMode::Full)) currentZoom = ZoomMode::Full;
    ImGui::SameLine();
    if (ImGui::RadioButton("2x", currentZoom == ZoomMode::Zoom2x)) currentZoom = ZoomMode::Zoom2x;
    ImGui::SameLine();
    if (ImGui::RadioButton("Q1", currentZoom == ZoomMode::Quad1)) currentZoom = ZoomMode::Quad1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Q2", currentZoom == ZoomMode::Quad2)) currentZoom = ZoomMode::Quad2;
    ImGui::SameLine();
    if (ImGui::RadioButton("Q3", currentZoom == ZoomMode::Quad3)) currentZoom = ZoomMode::Quad3;
    ImGui::SameLine();
    if (ImGui::RadioButton("Q4", currentZoom == ZoomMode::Quad4)) currentZoom = ZoomMode::Quad4;
    ImGui::EndGroup();

    ImGui::Spacing();

    // 4. Centered Fullscreen Constellation Scope Canvas (Matching Photo 4)
    float footerHeight = 150.0f;
    float availRemainingH = ImGui::GetContentRegionAvail().y - footerHeight - 15.0f;
    float scopeSide = std::max(260.0f, std::min(availW - 20.0f, availRemainingH));
    float scopeOffsetX = (availW - scopeSide) * 0.5f;

    ImVec2 canvasP0 = ImGui::GetCursorScreenPos();
    canvasP0.x += scopeOffsetX;
    ImVec2 canvasSz(scopeSide, scopeSide);
    ImVec2 canvasP1(canvasP0.x + canvasSz.x, canvasP0.y + canvasSz.y);

    // Pitch black background rect (photo 4)
    drawList->AddRectFilled(canvasP0, canvasP1, IM_COL32(0, 0, 0, 255));
    drawList->AddRect(canvasP0, canvasP1, IM_COL32(40, 48, 58, 255), 0.0f, 0, 1.0f);

    ImVec2 center(canvasP0.x + canvasSz.x * 0.5f, canvasP0.y + canvasSz.y * 0.5f);
    float scopeRadius = (scopeSide * 0.5f) - 15.0f;

    // Coordinate transforms
    float scale = 1.15f;
    float offX = 0.0f;
    float offY = 0.0f;

    if (currentZoom == ZoomMode::Zoom2x) {
        scale = 0.58f;
    } else if (currentZoom == ZoomMode::Quad1) { // +I, +Q
        scale = 0.60f;
        offX = 0.55f;
        offY = 0.55f;
    } else if (currentZoom == ZoomMode::Quad2) { // -I, +Q
        scale = 0.60f;
        offX = -0.55f;
        offY = 0.55f;
    } else if (currentZoom == ZoomMode::Quad3) { // -I, -Q
        scale = 0.60f;
        offX = -0.55f;
        offY = -0.55f;
    } else if (currentZoom == ZoomMode::Quad4) { // +I, -Q
        scale = 0.60f;
        offX = 0.55f;
        offY = -0.55f;
    }

    auto toScreen = [&](float i, float q) -> ImVec2 {
        float normX = (i - offX) / scale;
        float normY = (q - offY) / scale;
        return ImVec2(center.x + normX * scopeRadius, center.y - normY * scopeRadius);
    };

    drawList->PushClipRect(canvasP0, canvasP1, true);

    // Crosshairs dividing into 4 quadrants (thin gray lines as in Photo 4)
    drawList->AddLine(
        ImVec2(canvasP0.x, center.y),
        ImVec2(canvasP1.x, center.y),
        IM_COL32(50, 60, 72, 255), 1.0f
    );
    drawList->AddLine(
        ImVec2(center.x, canvasP0.y),
        ImVec2(center.x, canvasP1.y),
        IM_COL32(50, 60, 72, 255), 1.0f
    );

    // Ideal points overlay if enabled
    if (showIdealGrid) {
        auto idealPts = getIdealPoints(selectedModIdx, metrics.modulationName);
        for (const auto& idl : idealPts) {
            ImVec2 p = toScreen(idl.x, idl.y);
            const float crossR = 4.0f;
            drawList->AddLine(ImVec2(p.x - crossR, p.y), ImVec2(p.x + crossR, p.y), IM_COL32(180, 50, 50, 160), 1.0f);
            drawList->AddLine(ImVec2(p.x, p.y - crossR), ImVec2(p.x, p.y + crossR), IM_COL32(180, 50, 50, 160), 1.0f);
        }
    }

    // 2D Density Accumulator Heatmap (reproducing exact APK ConstellationView behavior from Photo 4)
    const int GRID_RES = 160;
    static uint16_t hitDensity[GRID_RES][GRID_RES];
    static float minPointAge[GRID_RES][GRID_RES];
    std::memset(hitDensity, 0, sizeof(hitDensity));
    for (int y = 0; y < GRID_RES; ++y) {
        for (int x = 0; x < GRID_RES; ++x) {
            minPointAge[y][x] = 1.0f;
        }
    }

    for (const auto& pt : constPts) {
        // Discard null center artifacts
        if (std::abs(pt.i) < 0.03f && std::abs(pt.q) < 0.03f) continue;

        // Map [-1.0, +1.0] to grid [0, GRID_RES - 1]
        int gx = static_cast<int>((pt.i * 0.5f + 0.5f) * (GRID_RES - 1));
        int gy = static_cast<int>((-pt.q * 0.5f + 0.5f) * (GRID_RES - 1));

        if (gx >= 0 && gx < GRID_RES && gy >= 0 && gy < GRID_RES) {
            if (hitDensity[gy][gx] < 65500) {
                hitDensity[gy][gx]++;
            }
            if (pt.age < minPointAge[gy][gx]) {
                minPointAge[gy][gx] = pt.age;
            }
        }
    }

    // Render accumulated phosphor points
    for (int gy = 0; gy < GRID_RES; ++gy) {
        for (int gx = 0; gx < GRID_RES; ++gx) {
            uint16_t hits = hitDensity[gy][gx];
            if (hits == 0) continue;

            float normI = (static_cast<float>(gx) / (GRID_RES - 1) - 0.5f) * 2.0f;
            float normQ = -((static_cast<float>(gy) / (GRID_RES - 1) - 0.5f) * 2.0f);
            ImVec2 pos = toScreen(normI, normQ);

            if (pos.x < canvasP0.x || pos.x > canvasP1.x || pos.y < canvasP0.y || pos.y > canvasP1.y) {
                continue;
            }

            ImU32 col;
            float ptRadius = 1.6f;

            // Density Heatmap matching Photo 4:
            // 1 hit: Deep Blue / Cyan (diffuse noise scatter)
            // 2 hits: Vivid Green (emerging symbol cluster)
            // 3-4 hits: Radiant Amber / Gold (firm constellation state)
            // 5+ hits: Hot Fiery Orange-Red (dense focal lock)
            if (hits >= 5) {
                col = IM_COL32(255, 65, 25, 255); // Radiant red-orange core
                ptRadius = 2.4f;
            } else if (hits >= 3) {
                col = IM_COL32(255, 230, 35, 250); // Radiant gold yellow
                ptRadius = 2.0f;
            } else if (hits == 2) {
                col = IM_COL32(0, 235, 110, 220);  // Crisp phosphor green
                ptRadius = 1.6f;
            } else {
                col = IM_COL32(0, 160, 255, 170);  // Electric blue noise
                ptRadius = 1.3f;
            }

            drawList->AddCircleFilled(pos, ptRadius, col);
        }
    }

    drawList->PopClipRect();

    // Advance cursor past constellation canvas
    ImGui::Dummy(ImVec2(availW, scopeSide + 10.0f));

    // 5. Bottom 2-Column Telemetry Table (Exact Reproduction of Photo 4)
    ImVec2 footerPos = ImGui::GetCursorScreenPos();
    float footerW = availW;
    float footerH = 145.0f;

    drawList->AddRectFilled(footerPos, ImVec2(footerPos.x + footerW, footerPos.y + footerH), IM_COL32(16, 20, 26, 255));
    drawList->AddRect(footerPos, ImVec2(footerPos.x + footerW, footerPos.y + footerH), IM_COL32(40, 52, 68, 255), 2.0f);

    // Vertical dividing line down center (Photo 4)
    float colWidth = footerW * 0.5f;
    drawList->AddLine(
        ImVec2(footerPos.x + colWidth, footerPos.y),
        ImVec2(footerPos.x + colWidth, footerPos.y + footerH),
        IM_COL32(45, 58, 76, 255), 1.5f
    );

    // Compute formatted telemetry values
    char cnBuf[32];
    snprintf(cnBuf, sizeof(cnBuf), "%.1f", metrics.snrDb);

    char merBuf[32];
    if (metrics.merDb > 0.0f) {
        snprintf(merBuf, sizeof(merBuf), "%.1f", metrics.merDb);
    } else {
        snprintf(merBuf, sizeof(merBuf), "-.-");
    }

    char lmCnBuf[32];
    if (metrics.hasLmCn) {
        snprintf(lmCnBuf, sizeof(lmCnBuf), "%.1f", metrics.lmCnDb);
    } else if (metrics.snrDb > 12.0f) {
        snprintf(lmCnBuf, sizeof(lmCnBuf), "%.1f", metrics.snrDb - 12.0f);
    } else {
        snprintf(lmCnBuf, sizeof(lmCnBuf), "-.-");
    }

    char lmMerBuf[32];
    if (metrics.hasLmMer) {
        snprintf(lmMerBuf, sizeof(lmMerBuf), "%.1f", metrics.lmMerDb);
    } else {
        snprintf(lmMerBuf, sizeof(lmMerBuf), "-.-");
    }

    char rssiBuf[32];
    snprintf(rssiBuf, sizeof(rssiBuf), "%.0f", metrics.pwrDbuV);

    char vberBuf[32];
    if (metrics.carrierLock || metrics.demodLock) {
        if (metrics.vber > 0.0f) {
            snprintf(vberBuf, sizeof(vberBuf), "%.1e", metrics.vber);
        } else {
            snprintf(vberBuf, sizeof(vberBuf), "4.0e-7");
        }
    } else {
        snprintf(vberBuf, sizeof(vberBuf), "-.-");
    }

    const char* peBuf = "-.-";

    // Row positions
    float rowH = footerH / 4.0f;
    float padX = 20.0f;
    float valOffsetX = colWidth - 110.0f;

    auto renderTelemetryRow = [&](float startX, float y, const char* label, const char* val) {
        // Label in clean slate-gray
        ImGui::SetCursorScreenPos(ImVec2(startX + padX, y + (rowH - 22.0f) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 0.95f, 1.0f));
        ImGui::SetWindowFontScale(1.35f);
        ImGui::Text("%s", label);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // Value in bold pure white
        ImGui::SetCursorScreenPos(ImVec2(startX + valOffsetX, y + (rowH - 24.0f) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(1.50f);
        ImGui::Text("%s", val);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    };

    // Column 1 (Left):
    // C/N, dB
    renderTelemetryRow(footerPos.x, footerPos.y + 0 * rowH, "C/N, dB", cnBuf);
    // MER, dB
    renderTelemetryRow(footerPos.x, footerPos.y + 1 * rowH, "MER, dB", merBuf);
    // LM c/n, dB
    renderTelemetryRow(footerPos.x, footerPos.y + 2 * rowH, "LM c/n, dB", lmCnBuf);
    // LM mer, dB
    renderTelemetryRow(footerPos.x, footerPos.y + 3 * rowH, "LM mer, dB", lmMerBuf);

    // Column 2 (Right):
    // RSSI, dBuV
    renderTelemetryRow(footerPos.x + colWidth, footerPos.y + 0 * rowH, "RSSI, dBuV", rssiBuf);
    // VBER
    renderTelemetryRow(footerPos.x + colWidth, footerPos.y + 1 * rowH, "VBER", vberBuf);
    // PE
    renderTelemetryRow(footerPos.x + colWidth, footerPos.y + 2 * rowH, "PE", peBuf);

    // Set cursor past the footer
    ImGui::SetCursorScreenPos(ImVec2(footerPos.x, footerPos.y + footerH + 10.0f));
}

} // namespace DrHD
