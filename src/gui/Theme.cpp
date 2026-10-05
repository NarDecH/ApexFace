#include "Theme.h"
#include "../core/Platform.h"

#include "imgui.h"

#include <cstdint>

namespace theme {

void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle{};
    s.WindowRounding = 6;
    s.ChildRounding = 8;
    s.FrameRounding = 6;
    s.PopupRounding = 8;
    s.GrabRounding = 6;
    s.TabRounding = 7;
    s.ScrollbarRounding = 8;
    s.WindowBorderSize = 1;
    s.ChildBorderSize = 1;
    s.FrameBorderSize = 0;
    s.FramePadding = {10, 7};
    s.ItemSpacing = {9, 8};
    s.WindowTitleAlign = {0.5f, 0.5f};
    s.ScrollbarSize = 13;

    auto rgb = [](int r, int g, int b, int a = 255) {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    };
    ImVec4 cText = rgb(230, 234, 242);
    ImVec4 cMuted = rgb(139, 149, 167);
    ImVec4 cBg = rgb(13, 17, 23);
    ImVec4 cPanel = rgb(21, 27, 35);
    ImVec4 cPanel2 = rgb(26, 33, 43);
    ImVec4 cBorder = rgb(35, 43, 54);
    ImVec4 cAcc = rgb(79, 140, 255);
    ImVec4 cAccHover = rgb(106, 160, 255);
    ImVec4 cAccActive = rgb(62, 118, 226);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = cText;
    c[ImGuiCol_TextDisabled] = cMuted;
    c[ImGuiCol_WindowBg] = cPanel;
    c[ImGuiCol_ChildBg] = cPanel;
    c[ImGuiCol_PopupBg] = rgb(24, 30, 39, 250);
    c[ImGuiCol_Border] = cBorder;
    c[ImGuiCol_FrameBg] = rgb(15, 20, 27);
    c[ImGuiCol_FrameBgHovered] = rgb(22, 29, 38);
    c[ImGuiCol_FrameBgActive] = rgb(28, 37, 48);
    c[ImGuiCol_TitleBg] = rgb(16, 21, 28);
    c[ImGuiCol_TitleBgActive] = rgb(20, 26, 34);
    c[ImGuiCol_TitleBgCollapsed] = rgb(16, 21, 28);
    c[ImGuiCol_MenuBarBg] = rgb(16, 21, 28);
    c[ImGuiCol_ScrollbarBg] = rgb(16, 21, 28);
    c[ImGuiCol_ScrollbarGrab] = rgb(48, 59, 74);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(62, 76, 95);
    c[ImGuiCol_ScrollbarGrabActive] = cAcc;
    c[ImGuiCol_CheckMark] = cAcc;
    c[ImGuiCol_SliderGrab] = cAcc;
    c[ImGuiCol_SliderGrabActive] = cAccHover;
    c[ImGuiCol_Button] = rgb(30, 39, 51);
    c[ImGuiCol_ButtonHovered] = rgb(40, 52, 67);
    c[ImGuiCol_ButtonActive] = rgb(50, 64, 82);
    c[ImGuiCol_Header] = rgb(34, 44, 57);
    c[ImGuiCol_HeaderHovered] = rgb(44, 57, 73);
    c[ImGuiCol_HeaderActive] = rgb(52, 67, 86);
    c[ImGuiCol_Separator] = cBorder;
    c[ImGuiCol_Tab] = rgb(18, 24, 31);
    c[ImGuiCol_TabHovered] = cAccHover;
    c[ImGuiCol_TabSelected] = rgb(32, 52, 92);
    c[ImGuiCol_TabDimmed] = rgb(16, 21, 28);
    c[ImGuiCol_TabDimmedSelected] = rgb(24, 32, 42);
    c[ImGuiCol_DockingPreview] = rgb(79, 140, 255, 70);
    c[ImGuiCol_DockingEmptyBg] = cBg;
    c[ImGuiCol_PlotHistogram] = cAcc;
    c[ImGuiCol_TableHeaderBg] = rgb(24, 31, 40);
    c[ImGuiCol_TableRowBg] = rgb(21, 27, 35);
    c[ImGuiCol_TableRowBgAlt] = rgb(24, 31, 40);
    c[ImGuiCol_TextSelectedBg] = rgb(79, 140, 255, 90);
    c[ImGuiCol_NavCursor] = cAcc;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);
}

bool loadFonts(float pixelSize, float* headingSizeOut) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    fs::path regular = platform::findResource("fonts/Sarabun-Regular.ttf");
    fs::path semibold = platform::findResource("fonts/Sarabun-SemiBold.ttf");
    if (regular.empty()) {
        io.Fonts->AddFontDefault();
        if (headingSizeOut) *headingSizeOut = pixelSize;
        return false;
    }
    const ImWchar* ranges = io.Fonts->GetGlyphRangesThai(); // includes basic latin + Thai
    ImFont* body = io.Fonts->AddFontFromFileTTF(regular.string().c_str(), pixelSize, nullptr, ranges);
    if (headingSizeOut) *headingSizeOut = pixelSize * 1.22f;
    if (!semibold.empty() && io.Fonts->Fonts.size() < 3) {
        io.Fonts->AddFontFromFileTTF(semibold.string().c_str(), pixelSize * 1.22f, nullptr, ranges);
    }
    io.FontDefault = body;
    return body != nullptr;
}

} // namespace theme
