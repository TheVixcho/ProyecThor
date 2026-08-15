#include "StreamingWorkspacePanel.h"
#include "BroadcastPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include <algorithm>
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

    if (!m_BroadcastRef)
    {
        ImGui::TextDisabled("Streaming no disponible.");
        ImGui::EndChild();
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    // Layout estilo OBS -- pedido explicito ("q muestre los paneles de
    // Conexiones > Transmision a servidor, layout como si fuera un OBS"):
    // vista previa grande a la izquierda, Fuente + Controles apilados en
    // una columna angosta a la derecha, en vez de las mismas 3 secciones
    // apiladas en una sola columna angosta como en Ajustes > Conexiones >
    // Streaming (ese layout sigue igual ahi, este es solo un reacomodo
    // aca -- mismo BroadcastPanel de siempre, ver CategoryConnections.cpp).
    constexpr float kGap          = 16.0f;
    constexpr float kSidebarWMax  = 340.0f;
    constexpr float kNarrowBreak  = 640.0f; // debajo de esto, se apila vertical
    const float totalW = ImGui::GetContentRegionAvail().x;

    if (totalW < kNarrowBreak)
    {
        // Ventana/franja angosta: vuelve a la columna unica de siempre en
        // vez de comprimir la vista previa o la barra lateral hasta que
        // dejen de servir.
        ImGui::TextDisabled("FUENTE");
        ImGui::Spacing();
        m_BroadcastRef->RenderCaptureSection();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::TextDisabled("VISTA PREVIA");
        ImGui::Spacing();
        m_BroadcastRef->RenderLayerSection();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::TextDisabled("CONTROLES");
        ImGui::Spacing();
        m_BroadcastRef->RenderStartSection();
    }
    else
    {
        const float sidebarW = std::clamp(totalW * 0.28f, 260.0f, kSidebarWMax);
        const float previewW = std::max(240.0f, totalW - sidebarW - kGap);

        ImGui::BeginChild("##streamingWsPreview", ImVec2(previewW, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextDisabled("VISTA PREVIA");
        ImGui::Spacing();
        m_BroadcastRef->RenderLayerSection();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, kGap);

        ImGui::BeginChild("##streamingWsSidebar", ImVec2(sidebarW, 0.0f), false);
        ImGui::TextDisabled("FUENTE");
        ImGui::Spacing();
        m_BroadcastRef->RenderCaptureSection();

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::TextDisabled("CONTROLES");
        ImGui::Spacing();
        m_BroadcastRef->RenderStartSection();
        ImGui::EndChild();
    }

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
