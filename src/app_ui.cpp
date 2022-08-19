#include <algorithm>

#include "imgui.h"

#include "app.h"

////////////////////////////////////////////////////////////////////////////////

static const char* helpText[] = {
    "F1",                  "show/hide help window",
    "F2 or Tab",           "show/hide display configuration window",
    "F10 or Q",            "quit application immediately",
    "F or Keypad *",       "toggle fit-to-screen / fill-screen mode",
    "Z or Keypad /",       "toggle 1:1 view / fit-to-screen mode",
    "I",                   "toggle integer scaling",
    "Keypad +/-",          "zoom in/out",
    "mouse wheel",         "zoom in/out",
    "left mouse button",   "move visible area",
    "middle mouse button", "move visible area",
    "cursor keys",         "move visible area (normal speed)",
    "Ctrl+cursor" ,        "move visible area (faster)",
    "Shift+cursor",        "move visible area (slower)",
    "Alt+cursor",          "start auto-scrolling in specified direction",
    "S",                   "stop auto-scrolling, or start in auto-detected direction",
    "1...9",               "set auto-scroll speed, start scrolling in auto direction",
    "Home / End",          "move to upper-left / lower-right corner",
    nullptr
};

void PixelViewApp::uiHelpWindow() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(
        vp->WorkPos.x + 0.5f * vp->WorkSize.x,
        vp->WorkPos.y + 0.5f * vp->WorkSize.y
    ), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("PixelView Help", &m_showConfig, ImGuiWindowFlags_NoNavInputs)) {
        if (ImGui::BeginTable("help", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (const char** p_help = helpText;  *p_help;  ++p_help) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(*p_help);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::uiConfigWindow() {
    bool b; float f; int i;
    if (ImGui::Begin("Display Configuration", &m_showConfig, ImGuiWindowFlags_NoNavInputs)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("view mode:");
        ImGui::SameLine(); if (ImGui::RadioButton("free",          (m_viewMode == vmFree))) { viewCfg("f"); }
        ImGui::SameLine(); if (ImGui::RadioButton("fit to screen", (m_viewMode == vmFit)))  { m_viewMode = vmFit;  viewCfg("sa"); }
        ImGui::SameLine(); if (ImGui::RadioButton("fill screen",   (m_viewMode == vmFill))) { m_viewMode = vmFill; viewCfg("sa"); }

        ImGui::BeginDisabled(!canDoIntegerZoom());
        b = m_integer && canDoIntegerZoom();
        if (ImGui::Checkbox("integer scaling", &b)) { m_integer = b; viewCfg("sa"); }
        ImGui::EndDisabled();

        f = float(m_aspect);
        if (ImGui::SliderFloat("pixel aspect", &f, 0.5f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) { m_aspect = f; viewCfg("sx"); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::Selectable("reset to square pixels")) { m_aspect = 1.0; viewCfg("sa"); }
            ImGui::EndPopup();
        }

        i = int(m_maxCrop * 100.0 + 0.5);
        ImGui::BeginDisabled(!m_integer);
        if (ImGui::SliderInt("max. crop", &i, 0, 50, "%d%%")) { m_maxCrop = 0.01 * i; viewCfg("sa"); }
        ImGui::EndDisabled();

        f = float(m_zoom);
        if (ImGui::SliderFloat("zoom factor", &f, float(std::max(m_minZoom, 1.0/16)), 16.0f, "%.02fx", ImGuiSliderFlags_Logarithmic)) {
            m_zoom = f;
            viewCfg("fsx");
        }

        auto posSlider = [&] (double &pos, double minPos, const char* title) {
            bool centered = (minPos >= 0.0);
            float percent = std::min(100.0f, std::max(0.0f, float(centered ? 50.0 : (100.0 * (pos / minPos)))));
            ImGui::BeginDisabled(centered);
            if (ImGui::SliderFloat(title, &percent, 0.0f, 100.0f, "%.2f%%")) {
                pos = minPos * (percent / 100.0);
                viewCfg("fsx");
            }
            ImGui::EndDisabled();
        };
        posSlider(m_x0, m_minX0, "X position");
        posSlider(m_y0, m_minY0, "Y position");

        i = int(m_scrollSpeed + 0.5);
        if (ImGui::SliderInt("scroll speed", &i, 1, 200, "%d px/frame")) { m_scrollSpeed = i; }
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
