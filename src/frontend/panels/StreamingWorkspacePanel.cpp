#include "StreamingWorkspacePanel.h"
#include "BroadcastPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include <imgui.h>

namespace ProyecThor::UI {

void StreamingWorkspacePanel::Render()
{
    bool visible = false;
    if (m_UIManagerRef)
    {
        visible = DS::BeginGlassPanel(GetName().c_str(), m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        visible = ImGui::Begin(GetName().c_str());
        ImGui::PopStyleVar();
    }

    if (!visible)
    {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##streamingWsContent", ImVec2(0.f, 0.f), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    // Mismas 3 secciones y mismo orden que Ajustes > Conexiones > Streaming
    // (ver CategoryConnections.cpp) -- literal "un panel de transmision como
    // el de ajustes", pedido explicito.
    if (m_BroadcastRef)
    {
        m_BroadcastRef->RenderCaptureSection();
        ImGui::Spacing();

        ImGui::SeparatorText("Capa (Layer)");
        m_BroadcastRef->RenderLayerSection();
        ImGui::Spacing();

        ImGui::SeparatorText("Iniciar");
        m_BroadcastRef->RenderStartSection();
    }
    else
    {
        ImGui::TextDisabled("Streaming no disponible.");
    }

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
