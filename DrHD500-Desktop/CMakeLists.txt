cmake_minimum_required(VERSION 3.20)
project(DrHD500Desktop VERSION 1.0.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GLFW (installed via brew)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

# macOS Frameworks
find_library(OPENGL_LIBRARY OpenGL REQUIRED)
find_library(COCOA_LIBRARY Cocoa REQUIRED)
find_library(IOKIT_LIBRARY IOKit REQUIRED)
find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/implot
    ${GLFW_INCLUDE_DIRS}
)

link_directories(
    ${GLFW_LIBRARY_DIRS}
    /opt/homebrew/lib
)

# ImGui Source Files
set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
    vendor/implot/implot.cpp
    vendor/implot/implot_items.cpp
    vendor/implot/implot_demo.cpp
)

# Application Sources
set(APP_SOURCES
    src/main.cpp
    src/core/SerialPort.cpp
    src/core/BluetoothDriver.mm
    src/core/DrHDProtocol.cpp
    src/audio/ToneGenerator.cpp
    src/data/TerrestrialDB.cpp
    src/data/SatelliteDB.cpp
    src/ui/DashboardView.cpp
    src/ui/SpectrumView.cpp
    src/ui/ConstellationView.cpp
    src/ui/SettingsView.cpp
)

add_executable(DrHD500Desktop
    ${IMGUI_SOURCES}
    ${APP_SOURCES}
)

find_library(IOBLUETOOTH_LIBRARY IOBluetooth REQUIRED)

target_link_libraries(DrHD500Desktop PRIVATE
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARY}
    ${COCOA_LIBRARY}
    ${IOKIT_LIBRARY}
    ${COREVIDEO_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${IOBLUETOOTH_LIBRARY}
)

target_compile_definitions(DrHD500Desktop PRIVATE
    GL_SILENCE_DEPRECATION
)
