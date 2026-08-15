#include "VideoEditorPanel.h"
#include "UIManager.h"
#include "LibraryPanel.h"
#include "DesignSystem.h"
#include "overlay/OverlayLibraryTab.h"
#include "frontend/ui/AppIcons.h"
#include "biblio/LibraryIcons.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ProyecThor::UI {

namespace {

// Rail vertical -- mismo lenguaje visual que LibrarySidebar::
// RenderSidebarButton (icono + label debajo + barra de acento a la
// izquierda + hover), reescrito local a proposito (esa funcion es "static"
// dentro de LibrarySidebar.cpp, no exportada -- mismo criterio de "helper
// chico duplicado" que el resto de la app) para darle a Producción "estilo"
// igual al rail de Biblioteca, pedido explicito.
using DrawIconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

bool RailButton(ImDrawList* dl, float railW, float btnH, const char* label, DrawIconFn drawIcon, bool active)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 bMin = cursor, bMax = { cursor.x + railW, cursor.y + btnH };

    ImGuiID hovId = ImGui::GetID(label);
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* pT = storage->GetFloatRef(hovId ^ 0x5A11u, 0.0f);
    bool hovered = ImGui::IsMouseHoveringRect(bMin, bMax, false);
    *pT += ((hovered ? 1.0f : 0.0f) - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float t = *pT;

    ImVec4 accentF = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);
    if (active) {
        dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ImVec4(accentF.x, accentF.y, accentF.z, 0.14f)), 5.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 14.0f)), 5.0f);
    }

    float barH  = btnH * 0.55f * (active ? 1.0f : t);
    float barY0 = cursor.y + (btnH - barH) * 0.5f;
    float barA  = active ? 1.0f : t * 0.55f;
    dl->AddRectFilled({ bMin.x, barY0 }, { bMin.x + 3.0f, barY0 + barH },
        ImGui::ColorConvertFloat4ToU32(ImVec4(accentF.x, accentF.y, accentF.z, barA)), 2.0f);

    ImGui::SetCursorScreenPos(bMin);
    std::string btnId = std::string("##railBtn_") + label;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), { railW, btnH });

    const float iconSz = 20.0f;
    ImVec2 lblSz = ImGui::CalcTextSize(label);
    float  totalH = iconSz + 5.0f + lblSz.y;
    float  startY = cursor.y + (btnH - totalH) * 0.5f;
    float  iconX  = cursor.x + (railW - iconSz) * 0.5f;

    ImVec4 textDim = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
    ImVec4 textPri = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
    float  brightT = active ? 1.0f : t;
    ImVec4 icF = { textDim.x + (textPri.x - textDim.x) * brightT,
                   textDim.y + (textPri.y - textDim.y) * brightT,
                   textDim.z + (textPri.z - textDim.z) * brightT, 1.0f };
    if (active) icF = { icF.x + (accentF.x - icF.x) * 0.35f, icF.y + (accentF.y - icF.y) * 0.35f,
                         icF.z + (accentF.z - icF.z) * 0.35f, 1.0f };

    drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));
    dl->AddText({ cursor.x + (railW - lblSz.x) * 0.5f, startY + iconSz + 5.0f },
        ImGui::ColorConvertFloat4ToU32(icF), label);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", label);

    return clicked;
}

} // namespace

VideoEditorPanel::~VideoEditorPanel() = default;

LibraryPanel* VideoEditorPanel::ResolveLibraryPanel()
{
    return m_UIManagerRef ? m_UIManagerRef->GetLibraryPanelRef() : nullptr;
}

void VideoEditorPanel::SetUIManager(UIManager* mgr)
{
    m_UIManagerRef = mgr;
    if (m_UIManagerRef && !m_OverlayTab)
        m_OverlayTab = std::make_unique<OverlayLibraryTab>(m_UIManagerRef);
}

void VideoEditorPanel::Render()
{
    // Pump de grabacion/exportacion del DAW -- corre siempre que este panel
    // se renderice, sin importar que seccion este activa, para que grabar o
    // exportar no se quede pegado si el operador cambia de seccion mientras
    // corre.
    m_Daw.Update();

    bool visible = false;
    if (m_UIManagerRef)
        visible = DS::BeginGlassPanel("Producción", m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    else
        visible = ImGui::Begin("Producción");

    if (!visible) {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    constexpr float kRailW = 68.0f;
    float bodyH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##videoRail", ImVec2(kRailW, bodyH), false, ImGuiWindowFlags_NoScrollbar);
    RenderRail();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 10.0f);

    ImGui::BeginChild("##videoContent", ImVec2(0.0f, bodyH), false);
    switch (m_Tab) {
        case Tab::Render:   RenderRenderTab();   break;
        case Tab::Audio:    RenderAudioTab();    break;
        case Tab::Overlays: RenderOverlaysTab(); break;
    }
    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

void VideoEditorPanel::RenderRail()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float railW = ImGui::GetContentRegionAvail().x;
    const float btnH = 60.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));

    struct Entry { const char* label; DrawIconFn icon; Tab tab; };
    static const Entry kEntries[3] = {
        { "Render",   ProyecThor::UI::AppIcons::DrawIcon_Swap,    Tab::Render },
        { "Audio",    ProyecThor::Library::DrawIcon_Audio,        Tab::Audio },
        { "Overlays", ProyecThor::UI::AppIcons::DrawIcon_Overlay, Tab::Overlays },
    };
    for (const auto& e : kEntries) {
        ImGui::PushID(e.label);
        if (RailButton(dl, railW, btnH, e.label, e.icon, m_Tab == e.tab))
            m_Tab = e.tab;
        ImGui::PopID();
    }

    ImGui::PopStyleVar();
}

void VideoEditorPanel::RenderOverlaysTab()
{
    if (m_OverlayTab)
        m_OverlayTab->Render();
    else
        ImGui::TextDisabled("Overlays no disponible.");
}

void VideoEditorPanel::RenderRenderTab()
{
    LibraryPanel* lib = ResolveLibraryPanel();
    if (lib)
        lib->RenderConverterSection();
    else
        ImGui::TextDisabled("Conversor no disponible.");
}

void VideoEditorPanel::RenderAudioTab()
{
    // DAW funcional (grabar/cortar/mover/exportar, ver AudioDawPanel para el
    // alcance exacto).
    m_Daw.Render();
}

} // namespace ProyecThor::UI
