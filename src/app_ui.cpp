#include <algorithm>

#include "imgui.h"

#include "app.h"

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
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
