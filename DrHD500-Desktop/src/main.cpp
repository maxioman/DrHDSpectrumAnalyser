#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "core/SerialPort.hpp"
#include "audio/ToneGenerator.hpp"
#include "ui/DashboardView.hpp"
#include "ui/SpectrumView.hpp"
#include "ui/ConstellationView.hpp"
#include "ui/SettingsView.hpp"

static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void applyDarkOscilloscopeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Dark high-contrast RF instrument styling
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.45f, 0.50f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.10f, 0.14f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.18f, 0.26f, 0.36f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.11f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.09f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.09f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.08f, 0.11f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.25f, 0.35f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.70f, 0.95f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.75f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.12f, 0.18f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.18f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.60f, 0.85f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.14f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.20f, 0.30f, 0.42f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.18f, 0.26f, 0.36f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.00f, 0.75f, 0.95f, 0.80f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.09f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.18f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.00f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.11f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.00f, 0.75f, 0.95f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.20f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.14f, 0.18f, 0.25f, 1.00f);

    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 6.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
}

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // GL 3.2 + GLSL 150 (Core Profile for modern macOS)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1120, 800, "Dr.HD 500 Satellite & Terrestrial Combo Analyzer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync for buttery 60 FPS

    // Setup Dear ImGui and ImPlot contexts
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyDarkOscilloscopeStyle();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Dr.HD serial driver
    DrHD::SerialPort serial;

    // Default to Dr.HD Bluetooth MAC address, or command line argument if provided
    std::string defaultPort = (argc > 1) ? argv[1] : "01-ac-78-00-04-55";
    if (!serial.openPort(defaultPort, 115200)) {
        std::cout << "[DrHD] Could not connect to " << defaultPort << ", starting in offline simulation mode." << std::endl;
        serial.startSimulation();
    } else {
        std::cout << "[DrHD] Successfully connected to physical device on " << defaultPort << std::endl;
    }

    // Start low-latency audio tone generator (beeper)
    DrHD::ToneGenerator::instance().start();

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update Tone Generator from latest telemetry
        DrHD::SignalMetrics metrics;
        DrHD::HardwareTelemetry hw;
        std::vector<DrHD::ConstellationPoint> constPts;
        DrHD::SpectrumData spec;
        serial.getTelemetry(metrics, hw, constPts, spec);

        DrHD::ToneGenerator::instance().updateSignal(metrics.snrDb, metrics.carrierLock);

        // Background auto-reconnect every 3 seconds if in simulation or disconnected
        static auto lastConnectAttempt = std::chrono::steady_clock::now();
        auto nowTime = std::chrono::steady_clock::now();
        if (serial.getStatus() != DrHD::ConnectionStatus::Connected) {
            if (std::chrono::duration_cast<std::chrono::seconds>(nowTime - lastConnectAttempt).count() >= 3) {
                lastConnectAttempt = nowTime;
                if (serial.openPort(defaultPort, 115200)) {
                    std::cout << "[DrHD] Successfully reconnected to physical device on " << defaultPort << std::endl;
                }
            }
        }

        // Fullscreen dockable background window
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

        ImGui::Begin("DrHD500MainWindow", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        // Menu Bar / Global Status Bar
        if (ImGui::BeginMenuBar()) {
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), " Dr.HD 500 COMBO ");
            ImGui::Separator();

            // Link status badge
            DrHD::ConnectionStatus status = serial.getStatus();
            if (status == DrHD::ConnectionStatus::Connected) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "● HARDWARE CONNECTED (%s)", serial.getPortName().c_str());
            } else if (status == DrHD::ConnectionStatus::Simulation) {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "● SIMULATION MODE");
                ImGui::SameLine();
                if (ImGui::SmallButton("Connect Finder")) {
                    serial.openPort(defaultPort, 115200);
                }
            } else if (status == DrHD::ConnectionStatus::Connecting) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "● CONNECTING...");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "● DISCONNECTED");
                ImGui::SameLine();
                if (ImGui::SmallButton("Retry Connect")) {
                    serial.openPort(defaultPort, 115200);
                }
            }

            ImGui::Separator();

            // Lock badge
            if (metrics.carrierLock || metrics.demodLock || metrics.snrDb >= 5.0f) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "LOCK: CARRIER & DEMOD SYNC");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "LOCK: SEARCHING");
            }

            ImGui::Separator();

            // Battery badge
            ImGui::Text("Battery: %d%% (%d mV)", hw.batteryPercent, hw.batteryMilliVolts);

            ImGui::EndMenuBar();
        }

        // Main Tab Bar Navigation
        if (ImGui::BeginTabBar("DrHDMainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("  Measures & Dashboard  ")) {
                DrHD::DashboardView::render(serial);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  RF Spectrum Analyzer  ")) {
                DrHD::SpectrumView::render(serial);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  Constellation Scope  ")) {
                DrHD::ConstellationView::render(serial);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  Settings & DiSEqC  ")) {
                DrHD::SettingsView::render(serial);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.08f, 0.11f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup resources
    DrHD::ToneGenerator::instance().stop();
    serial.closePort();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
