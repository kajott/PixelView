// SPDX-FileCopyrightText: 2021-2022 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#include <algorithm>

#include "imgui.h"

#include "version.h"

#include "app.h"

////////////////////////////////////////////////////////////////////////////////

static const char* helpText[] = {
    "F1",                  "show/hide help window",
    "F2 or Tab",           "show/hide display configuration window",
    "F3",                  "show/hide filename display",
    "F5",                  "reload current image",
    "F10 or Q or 2x Esc",  "quit application immediately",
    "F or Numpad *",       "toggle fit-to-screen / fill-screen mode",
    "Z or Numpad /",       "toggle 1:1 view / fit-to-screen mode",
    "T",                   "set 1:1 view / fill-screen and show top-left corner",
    "I",                   "toggle integer scaling",
    "+/- or mouse wheel",  "zoom in/out",
    "left mouse button",   "move visible area",
    "middle mouse button", "move visible area",
    "cursor keys",         "move visible area (normal speed)",
    "Ctrl+cursor" ,        "move visible area (faster)",
    "Shift+cursor",        "move visible area (slower)",
    "Alt+cursor",          "start auto-scrolling in specified direction",
    "S",                   "stop auto-scrolling, or start in auto-detected direction",
    "1...9",               "set auto-scroll speed, start scrolling in auto direction",
    "Home / End",          "move to upper-left / lower-right corner",
    "Ctrl+S or F6",        "save view settings for the current file",
    "Explorer Drag&Drop",  "load another image",
    "PageUp / PageDown",   "load previous / next image file from the current directory",
    "Ctrl+Home / Ctrl+End","load first / last image file in the current directory",
    nullptr
};

void PixelViewApp::uiHelpWindow() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(
        vp->WorkPos.x + 0.5f * vp->WorkSize.x,
        vp->WorkPos.y + 0.5f * vp->WorkSize.y
    ), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin(PRODUCT_NAME " Help", &m_showHelp, ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        if (ImGui::BeginTable("help", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (const char** p_help = helpText;  *p_help;  ++p_help) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(*p_help);
            }
            ImGui::EndTable();
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted(PRODUCT_NAME " version " PRODUCT_VERSION);
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::uiConfigWindow() {
    bool b; float f; int i;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Display Configuration", &m_showConfig, ImGuiWindowFlags_NoNavInputs)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("view mode:");
        ImGui::SameLine(); if (ImGui::RadioButton("free",          (m_viewMode == vmFree)))  { viewCfg("f"); }
        ImGui::SameLine(); if (ImGui::RadioButton("fit to screen", (m_viewMode == vmFit)))   { m_viewMode = vmFit;   viewCfg("sa"); }
        ImGui::SameLine(); if (ImGui::RadioButton("fill screen",   (m_viewMode == vmFill)))  { m_viewMode = vmFill;  viewCfg("sa"); }
        ImGui::SameLine(); ImGui::BeginDisabled(!canUsePanelMode());
                           if (ImGui::RadioButton("panel",         (m_viewMode == vmPanel))) { m_viewMode = vmPanel; viewCfg("sx"); }
                           ImGui::EndDisabled();

        ImGui::BeginDisabled(!canDoIntegerZoom());
        b = m_integer && canDoIntegerZoom();
        if (ImGui::Checkbox("integer scaling", &b)) { m_integer = b; viewCfg("sa"); }
        ImGui::EndDisabled();

        f = float(m_aspect);
        if (ImGui::SliderFloat("pixel aspect", &f, 0.5f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) { m_aspect = f; computePanelGeometry(); viewCfg("sx"); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::Selectable("reset to square pixels")) { m_aspect = 1.0; computePanelGeometry(); viewCfg("sa"); }
            ImGui::EndPopup();
        }

        i = int(m_maxCrop * 100.0 + 0.5);
        ImGui::BeginDisabled(!m_integer);
        if (ImGui::SliderInt("max. crop", &i, 0, 50, "%d%%")) { m_maxCrop = 0.01 * i; viewCfg("sa"); }
        ImGui::EndDisabled();

        ImGui::BeginDisabled((m_viewMode == vmPanel));
        f = float(m_zoom);
        if (ImGui::SliderFloat("zoom factor", &f, float(std::max(m_minZoom, 1.0/16)), 16.0f, "%.02fx", ImGuiSliderFlags_Logarithmic)) {
            m_zoom = f;
            viewCfg("fsx");
        }
        ImGui::EndDisabled();

        auto posSlider = [&] (double &pos, double minPos, const char* title) {
            bool centered = (minPos >= 0.0) || (m_viewMode == vmPanel);
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

        if (m_isANSI) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (ImGui::CollapsingHeader("ANSI rendering options", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (m_ansi.ui()) {
                    double oldAspect = m_ansi.aspect;
                    loadImage(true);  // reload image with new settings
                    if (m_ansi.aspect != oldAspect) {
                        // recommended aspect ratio changed -> use that AR
                        m_aspect = m_ansi.aspect;
                        computePanelGeometry();
                        viewCfg("x");
                    }
                }
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::BeginDisabled(!imgValid());
        if (ImGui::Button("Save Settings")) { saveConfig(); }
        ImGui::SameLine();
        if (ImGui::Button("Reload Image")) { loadImage(); }
        ImGui::EndDisabled();
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::uiStatusWindow() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(
        vp->WorkPos.x + vp->WorkSize.x * 0.5f,
        vp->WorkPos.y + vp->WorkSize.y
    ), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(20.0f, 20.0f));
    ImU32 color = (m_statusType == stSuccess) ? 0xFF00A000 :
                  (m_statusType == stError)   ? 0xFF0000C0 :
                                                0xFFA00000;
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       color);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, color);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      color);
    ImGui::SetNextWindowBgAlpha(0.375f);
    bool show = true;
    if (ImGui::Begin(
        (m_statusType == stSuccess) ? "Success##statusWindow" :
        (m_statusType == stError)   ? "Error##statusWindow" :
                                      "Message##statusWindow",
        &show,
        ImGuiWindowFlags_NoNavInputs |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse))
    {
        if (m_statusMessage) {
            ImGui::TextUnformatted(m_statusMessage);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    if (!show) { clearStatus(); }
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::uiInfoWindow() {
    if (!m_infoStr) { return; }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y),
        ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.375f);
    if (ImGui::Begin("##infoStr", nullptr,
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | 
        ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::Text("%s", m_infoStr);
    }
    ImGui::End();
}
