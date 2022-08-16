#include <algorithm>

#include "imgui.h"

#include "app.h"

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::uiConfigWindow() {
    bool b; float f; int i;
    if (ImGui::Begin("Display Configuration", &m_showConfig, 0)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("view mode:");
        ImGui::SameLine(); if (ImGui::RadioButton("free",          (m_viewMode == vmFree))) { m_viewMode = vmFree; updateView(); }
        ImGui::SameLine(); if (ImGui::RadioButton("fit to screen", (m_viewMode == vmFit)))  { m_viewMode = vmFit;  updateView(); }
        ImGui::SameLine(); if (ImGui::RadioButton("fill screen",   (m_viewMode == vmFill))) { m_viewMode = vmFill; updateView(); }

        ImGui::BeginDisabled(!canDoIntegerZoom());
        b = m_integer && canDoIntegerZoom();
        if (ImGui::Checkbox("integer scaling", &b)) { m_integer = b; updateView(); }
        ImGui::EndDisabled();

        f = float(m_aspect);
        if (ImGui::SliderFloat("pixel aspect ratio", &f, 0.5f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) { m_aspect = f; updateView(); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::Selectable("reset to square pixels")) { m_aspect = 1.0; updateView(); }
            ImGui::EndPopup();
        }

        i = int(m_maxCrop * 100.0 + 0.5);
        ImGui::BeginDisabled(!m_integer);
        if (ImGui::SliderInt("max. allowed crop", &i, 0, 50, "%d%%")) { m_maxCrop = 0.01 * i; updateView(); }
        ImGui::EndDisabled();

        f = float(m_zoom);
        if (ImGui::SliderFloat("zoom factor", &f, 1.0f/16, 16.0f, "%.02fx", ImGuiSliderFlags_Logarithmic)) {
            m_zoom = f;  m_viewMode = vmFree;  updateView();
        }

        auto posSlider = [&] (double &pos, double minPos, const char* title) {
            bool centered = (minPos >= 0.0);
            float percent = std::min(100.0f, std::max(0.0f, float(centered ? 50.0 : (100.0 * (pos / minPos)))));
            ImGui::BeginDisabled(centered);
            if (ImGui::SliderFloat(title, &percent, 0.0f, 100.0f, "%.2f%%")) {
                pos = minPos * (percent / 100.0);
                m_viewMode = vmFree;
                updateView(false);
            }
            ImGui::EndDisabled();
        };
        posSlider(m_x0, m_minX0, "horizontal position");
        posSlider(m_y0, m_minY0, "vertical position");
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
