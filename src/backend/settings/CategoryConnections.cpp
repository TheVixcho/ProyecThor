#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "frontend/panels/OSCPanel.h"
#include "frontend/panels/BroadcastPanel.h"
#include "frontend/panels/StreamingPanel.h"
#include "frontend/panels/SyncPanel.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

static void SubDivider(const char* label) {
    const auto& theme = SettingsManager::Get().GetSettings().theme;
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    ImVec2      ts  = ImGui::CalcTextSize(label);
    float       cy  = pos.y + ts.y * 0.5f;

    ImVec4 accent(theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
    ImU32  colSolid = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.65f));
    ImU32  colFade  = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
    ImU32  textCol  = ImGui::ColorConvertFloat4ToU32(
        ImVec4(theme.textDim[0], theme.textDim[1], theme.textDim[2], theme.textDim[3]));

    const float leadW = 18.0f, gap = 10.0f;
    dl->AddRectFilledMultiColor(ImVec2(pos.x, cy), ImVec2(pos.x + leadW, cy + 1.5f),
        colFade, colSolid, colSolid, colFade);
    dl->AddText(ImVec2(pos.x + leadW + gap, pos.y), textCol, label);
    float tailX = pos.x + leadW + gap + ts.x + gap;
    dl->AddRectFilledMultiColor(ImVec2(tailX, cy), ImVec2(pos.x + w, cy + 1.5f),
        colSolid, colFade, colFade, colFade);

    ImGui::Dummy(ImVec2(w, ts.y + 10.0f));
}

void SettingsPanel::RenderCategoryConnections() {

    if (SectionTitle("Red (LAN)")) {
        if (m_StreamingPanelRef)
            m_StreamingPanelRef->RenderContent();
        else
            ImGui::TextDisabled("Red no disponible.");
    }

    ImGui::Spacing();

    if (SectionTitle("Mobile")) {
        if (m_SyncPanelRef)
            m_SyncPanelRef->RenderContent();
        else
            ImGui::TextDisabled("Mobile no disponible.");
    }

    ImGui::Spacing();

    if (SectionTitle("Captura", "Streaming")) {
        if (m_BroadcastPanelRef)
            m_BroadcastPanelRef->RenderCaptureSection();
        else
            ImGui::TextDisabled("Streaming no disponible.");

        ImGui::Spacing();

        if (m_BroadcastPanelRef) {
            SubDivider("CAPA (LAYER)");
            m_BroadcastPanelRef->RenderLayerSection();

            ImGui::Spacing();

            SubDivider("INICIAR");
            m_BroadcastPanelRef->RenderStartSection();
        }
    }

    ImGui::Spacing();

    if (SectionTitle("OSC")) {
        if (m_OSCPanelRef)
            m_OSCPanelRef->RenderContent();
        else
            ImGui::TextDisabled("OSC no disponible.");
    }
}

} // namespace ProyecThor::UI::Settings
