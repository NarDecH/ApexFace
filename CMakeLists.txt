cmake_minimum_required(VERSION 3.20)
project(ApexFace VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /MP /W3 /Zc:__cplusplus /permissive-)
endif()

# ---------------------------------------------------------------------------
# OpenCV (found through the CMake user package registry on this machine;
# on other machines pass -DOpenCV_DIR="C:/_OpenCV/install")
# ---------------------------------------------------------------------------
# This OpenCV build was made WITH CUDA; its config re-runs FindCUDA, which
# needs the toolkit root. Default to the local toolkit unless overridden.
if(NOT DEFINED CUDA_TOOLKIT_ROOT_DIR AND EXISTS "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9")
    set(CUDA_TOOLKIT_ROOT_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9" CACHE PATH "CUDA toolkit root")
endif()
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs objdetect dnn)

# ---------------------------------------------------------------------------
# Third-party: GLFW (windowing), Dear ImGui (UI), nfd (native folder dialog)
# ---------------------------------------------------------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

add_subdirectory(third_party/nfd)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# ---------------------------------------------------------------------------
# Core engine (shared by GUI and CLI)
# ---------------------------------------------------------------------------
add_library(apexface_core STATIC
    src/core/Logger.cpp
    src/core/Platform.cpp
    src/core/ImageIO.cpp
    src/core/Settings.cpp
    src/core/Sharpness.cpp
    src/core/FaceDetector.cpp
    src/core/Analyzer.cpp
    src/core/ReportGenerator.cpp
)
target_include_directories(apexface_core PUBLIC src)
target_link_libraries(apexface_core PUBLIC ${OpenCV_LIBS})
target_compile_definitions(apexface_core PUBLIC APEXFACE_VERSION="1.0.0")

# ---------------------------------------------------------------------------
# GUI application
# ---------------------------------------------------------------------------
add_executable(ApexFace WIN32
    src/app/main_gui.cpp
    src/gui/Theme.cpp
    src/gui/App.cpp
)
target_link_libraries(ApexFace PRIVATE apexface_core imgui nfd opengl32)

# ---------------------------------------------------------------------------
# Command-line application
# ---------------------------------------------------------------------------
add_executable(apexface-cli src/app/main_cli.cpp)
target_link_libraries(apexface-cli PRIVATE apexface_core)
