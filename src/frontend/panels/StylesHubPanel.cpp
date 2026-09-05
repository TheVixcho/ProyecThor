#include "StylesHubPanel.h"
#include "TransitionPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

StylesHubPanel::StylesHubPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{
    // Se registra a si mismo (direccion de sus propios miembros) para que
    // ViewPanel pueda pedir "Limpiar anuncios"/"Limpiar captura" sin
    // depender de StylesHubPanel directamente — ver PresentationCore::
    // SetAnnouncementsRef/SetCapturePanelRef.
    Core::PresentationCore::Get().SetAnnouncementsRef(&m_Announcements);
    Core::PresentationCore::Get().SetCapturePanelRef(&m_Capture);

    m_Styles.SetUIManager(uiManager);
}

void StylesHubPanel::RenderTransitionQuickBar()
{
    if (!m_TransitionsRef) return;

    const ImU32  kAccent  = DS::AccentColor;
    const ImVec4 kAccentV = ImGui::ColorConvertU32ToFloat4(kAccent);
    const ImVec4 kMutedV  = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);

    TransitionType current  = m_TransitionsRef->GetCurrentType();
    float          duration = m_TransitionsRef->GetDuration();
    bool           isAdvanced = current != TransitionType::None && current != TransitionType::Fade && current != TransitionType::Iris;

    ImGui::PushStyleColor(ImGuiCol_Text, DS::AccentColor);
    ImGui::TextUnformatted("TRANSICIÓN RÁPIDA");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // 4 Mode selection pills
    float availW = ImGui::GetContentRegionAvail().x;
    float pillW = std::max(60.0f, (availW - 18.0f) / 4.0f);

    auto RenderTransOption = [&](const char* label, bool active, TransitionType type, bool isAdv) {
        ImVec4 bg = active ? ImVec4(kAccentV.x, kAccentV.y, kAccentV.z, 0.22f) : ImVec4(1,1,1,0.06f);
        ImVec4 bdr = active ? kAccentV : ImVec4(1,1,1,0.12f);
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(kAccentV.x, kAccentV.y, kAccentV.z, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Border, bdr);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1,1,1,1) : kMutedV);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button(label, ImVec2(pillW, 28.0f))) {
            if (isAdv) {
                ImGui::OpenPopup("##transAdvancedPopup");
            } else {
                m_TransitionsRef->SetType(type);
            }
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    };

    RenderTransOption("Corte", current == TransitionType::None, TransitionType::None, false);
    ImGui::SameLine(0.0f, 6.0f);
    RenderTransOption("Disolver", current == TransitionType::Fade, TransitionType::Fade, false);
    ImGui::SameLine(0.0f, 6.0f);
    RenderTransOption("🎭 Teatro", current == TransitionType::Iris, TransitionType::Iris, false);
    ImGui::SameLine(0.0f, 6.0f);
    RenderTransOption("Más...", isAdvanced, TransitionType::None, true);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Duración slider
    ImGui::TextUnformatted("Duración:");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, DS::AccentLight);
    ImGui::Text("%.2fs", duration);
    ImGui::PopStyleColor();

    if (DS::ModernSlider("##quickTransDur", &duration, 0.1f, 3.0f, 290.0f, kAccent))
        m_TransitionsRef->SetDuration(duration);

    // Quick preset buttons
    ImGui::Spacing();
    const float presets[4] = { 0.3f, 0.5f, 1.0f, 1.5f };
    const char* presetLabels[4] = { "0.3s", "0.5s", "1.0s", "1.5s" };
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine(0.0f, 6.0f);
        bool sel = (std::abs(duration - presets[i]) < 0.05f);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(kAccentV.x, kAccentV.y, kAccentV.z, 0.30f) : ImVec4(1,1,1,0.06f));
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : kMutedV);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button(presetLabels[i], ImVec2(67.0f, 22.0f))) {
            m_TransitionsRef->SetDuration(presets[i]);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    if (ImGui::BeginPopup("##transAdvancedPopup"))
    {
        ImGui::SetNextItemWidth(340.0f);
        m_TransitionsRef->RenderContent();
        ImGui::EndPopup();
    }
}

// Icono en el rail de arriba con dibujo vectorial y popup moderno
void StylesHubPanel::RenderTransitionRailButton()
{
    if (!m_TransitionsRef) return;

    TransitionType current  = m_TransitionsRef->GetCurrentType();
    float          duration = m_TransitionsRef->GetDuration();
    bool           hasTrans = (current != TransitionType::None);

    const ImVec2 size = { 32.0f, 32.0f };
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 bMin   = cursor;
    ImVec2 bMax   = { cursor.x + size.x, cursor.y + size.y };

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID hovId = ImGui::GetID("##transRailBtn");
    float*  pT    = storage->GetFloatRef(hovId ^ 0x7654ABCDu, 0.0f);
    bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
    *pT += ((hovered ? 1.0f : 0.0f) - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float t = *pT;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr float rounding = 6.0f;

    if (hasTrans) {
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);
        ac.w = 0.18f;
        dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        ac.w = 0.35f;
        dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 20.0f)), rounding);
    } else {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 8), rounding);
    }

    ImGui::SetCursorScreenPos(bMin);
    if (ImGui::InvisibleButton("##transRailBtn", size)) {
        ImGui::OpenPopup("##transQuickPopup");
    }

    ImVec2 center = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
    ImU32 icCol = hasTrans ? DS::AccentLight : (hovered ? DS::TextPrimary : DS::TextSecondary);
    const float iconSz = 18.0f;
    ImVec2 iconPos = { center.x - iconSz * 0.5f, center.y - iconSz * 0.5f };
    AppIcons::DrawIcon_Swap(dl, iconPos, iconSz, icCol);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        const char* typeName = (current == TransitionType::None) ? "Sin transición" :
                               (current == TransitionType::Fade) ? "Disolver" :
                               (current == TransitionType::Iris) ? "Teatro (Iris)" : "Avanzada";
        ImGui::SetTooltip("Transición rápida: %s (%.2fs)", typeName, duration);
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.11f, 0.12f, 0.15f, 0.98f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopup("##transQuickPopup")) {
        RenderTransitionQuickBar();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// Divisor con gradiente entre el rail de iconos y el contenido -- horizontal
// (linea abajo del rail) cuando el rail va arriba, vertical (linea al lado)
// cuando el rail pasa a columna izquierda. length = ancho o alto disponible
// segun corresponda.
static void RenderStylesHubDivider(bool vertical, float length)
{
    ImVec2      p      = ImGui::GetCursorScreenPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImU32       colEdge = IM_COL32(60, 80, 160,  0);
    ImU32       colMid  = IM_COL32(60, 80, 160, 80);

    if (vertical) {
        float midY = p.y + length * 0.5f;
        dl->AddRectFilledMultiColor(p, { p.x + 1.f, midY }, colEdge, colEdge, colMid, colMid);
        dl->AddRectFilledMultiColor({ p.x, midY }, { p.x + 1.f, p.y + length }, colMid, colMid, colEdge, colEdge);
        ImGui::Dummy(ImVec2(1.0f, length));
    } else {
        float midX = p.x + length * 0.5f;
        dl->AddRectFilledMultiColor(p, { midX, p.y + 1.f }, colEdge, colMid, colMid, colEdge);
        dl->AddRectFilledMultiColor({ midX, p.y }, { p.x + length, p.y + 1.f }, colMid, colEdge, colEdge, colMid);
        ImGui::Dummy(ImVec2(length, 1.0f));
    }
}

void StylesHubPanel::Render()
{
    // Alt Gr + 2: si Diseño esta colapsado (o pasando el punto medio de la
    // animacion), no dibujar la ventana ni sus tabs internas.
    if (m_UIManager && m_UIManager->IsPanelCollapsedForRender(GetName()))
        return;

    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, 0, ImVec2(0.0f, 0.0f))
        : ImGui::Begin(GetName().c_str());

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    // Cuando el panel queda mas alto que ancho (columna angosta -- ej.
    // "Diseño" apilado junto a "Vista en Vivo" en Ajustes > Apariencia >
    // Entorno de trabajo > Simple), el rail de iconos pasa a vertical en el
    // lateral izquierdo: una fila horizontal de 6 iconos + el boton de
    // transicion no entra en una columna angosta sin amontonarse/cortarse.
    const bool vertical = totalH > totalW * 1.15f;

    // Transiciones tiene pestaña propia (catalogo con nombre, ver
    // LayersTransitionsTab) junto a Estilos, para gestionar/crear
    // transiciones guardadas. El icono chico + popup rapido
    // (RenderTransitionRailButton) se mantiene ademas, como atajo para
    // tocar tipo/duracion de la transicion activa sin abrir la pestaña.
    static const IconRailItem kItems[] = {
        { (int)StylesSection::Backgrounds,   AppIcons::DrawIcon_Layers,    "Fondos"      },
        { (int)StylesSection::Styles,        AppIcons::DrawIcon_TextAa,    "Estilos"     },
        { (int)StylesSection::Shaders,       AppIcons::DrawIcon_Shader,    "Shaders"     },
        { (int)StylesSection::Transitions,   AppIcons::DrawIcon_Swap,      "Transiciones"},
        { (int)StylesSection::Announcements, HomeIcons::DrawIcon_Megaphone,"Anuncios"    },
        { (int)StylesSection::Capture,       HomeIcons::DrawIcon_Camera,   "Captura"     },
    };
    const auto& hubSettings  = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
    int         currentIndex = (int)m_CurrentSection;

    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    if (vertical)
    {
        const float railW = IconRailThickness(true);

        // ── Rail de iconos a la izquierda ───────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##stylesRail", ImVec2(railW, totalH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        RenderIconRail(kItems, 6, currentIndex, IconRailOrientation::Vertical, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;

        // Boton de transicion -- abajo del todo de la columna, no hay
        // "derecha" a la que pegarlo como en el rail horizontal.
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), totalH - 40.0f));
        ImGui::SetCursorPosX(std::max(0.0f, (railW - 32.0f) * 0.5f));
        RenderTransitionRailButton();

        ImGui::EndChild();
        ImGui::SameLine();
        RenderStylesHubDivider(true, totalH);
        ImGui::SameLine();
    }
    else
    {
        const float railH = IconRailThickness(false);

        // ── Rail de iconos arriba ────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##stylesRail", ImVec2(totalW, railH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        RenderIconRail(kItems, 6, currentIndex, IconRailOrientation::Horizontal, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;

        ImGui::SetCursorPosY((railH - 32.0f) * 0.5f);
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), totalW - 44.0f));
        RenderTransitionRailButton();

        ImGui::EndChild();
        RenderStylesHubDivider(false, totalW);
    }

    // ── Contenido de la seccion activa ───────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##stylesContent", ImVec2(0.f, 0.f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case StylesSection::Backgrounds: m_Backgrounds.RenderContent(); break;
        case StylesSection::Styles:
            m_Styles.RenderContent();
            break;
        case StylesSection::Shaders:     m_Shaders.RenderContent();    break;
        case StylesSection::Transitions:
            m_TransitionsTab.RenderContent();
            break;
        case StylesSection::Announcements:
            if (m_UIManager) m_Announcements.Render(m_UIManager->GetGlassRenderer());
            break;
        case StylesSection::Capture:
            m_Capture.RenderContent();
            break;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

} // namespace ProyecThor::UI
