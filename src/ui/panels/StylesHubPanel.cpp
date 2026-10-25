#include "StylesHubPanel.h"
#include "TransitionPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "ui/framework/IconRail.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/home/HomeIcons.h"
#include "ui/panels/biblio/LibraryIcons.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/panels/biblio/LibraryContext.h"
#include "ui/panels/LibraryPanel.h"
#include "ui/panels/WebBrowserPanel.h"
#include "ui/panels/model3d/Model3DPanel.h"
#include "ui/panels/lab/LabPanel.h"
#include "ui/panels/PluginsPanel.h"
#include "core/settings/SettingsManager.h"
#include "core/PresentationCore.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <vector>

namespace ProyecThor::UI {

StylesHubPanel::StylesHubPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
    , m_OverlayTab(std::make_unique<OverlayLibraryTab>(uiManager))
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

    ImGui::SetCursorScreenPos(bMin);
    bool clicked = ImGui::InvisibleButton("##transRailBtn", size);
    bool hovered = ImGui::IsItemHovered();
    if (clicked) {
        ImGui::OpenPopup("##transQuickPopup");
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr float rounding = 6.0f;

    if (hasTrans) {
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);
        ac.w = 0.18f;
        dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        ac.w = 0.35f;
        dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
    } else if (hovered) {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 20), rounding);
    } else {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 8), rounding);
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

void StylesHubPanel::SelectSection(StylesSection section)
{
    m_CurrentSection = section;
    if (m_UIManager) {
        m_UIManager->CloseAllToolWindows();
        m_UIManager->SetPanelCollapsed(1, false);
        m_UIManager->SetPanelCollapsed(0, true);
    }
}

void StylesHubPanel::Render()
{
    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, ImVec2(0.0f, 0.0f))
        : ImGui::Begin(GetName().c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    const bool vertical = totalH > totalW * 1.15f;

    const auto& hubSettings  = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
    const auto& libSettings  = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;
    int         currentIndex = (int)m_CurrentSection;

    std::vector<IconRailItem> railItems = {
        { (int)StylesSection::Backgrounds,   AppIcons::DrawIcon_Layers,          "Fondos"      },
        { (int)StylesSection::Multimedia,    Library::DrawIcon_Multimedia,       "Multimedia"  },
        { (int)StylesSection::Overlays,      AppIcons::DrawIcon_Overlay,         "Overlays"    },
        { (int)StylesSection::Styles,        AppIcons::DrawIcon_TextAa,          "Estilos"     },
        { (int)StylesSection::Shaders,       AppIcons::DrawIcon_Shader,          "Shaders"     },
        { (int)StylesSection::Transitions,   AppIcons::DrawIcon_Swap,            "Transiciones"},
        { (int)StylesSection::Announcements, HomeIcons::DrawIcon_Megaphone,      "Anuncios"    },
        { (int)StylesSection::Capture,       HomeIcons::DrawIcon_Camera,         "Captura"     },
    };

    std::vector<std::array<float, 4>> railColors = {
        { hubSettings.categoryColor[0][0], hubSettings.categoryColor[0][1], hubSettings.categoryColor[0][2], 1.0f }, // Fondos
        { 0.30f, 0.72f, 0.95f, 1.0f }, // Multimedia
        { 0.95f, 0.58f, 0.22f, 1.0f }, // Overlays
        { hubSettings.categoryColor[1][0], hubSettings.categoryColor[1][1], hubSettings.categoryColor[1][2], 1.0f }, // Estilos
        { hubSettings.categoryColor[2][0], hubSettings.categoryColor[2][1], hubSettings.categoryColor[2][2], 1.0f }, // Shaders
        { hubSettings.categoryColor[3][0], hubSettings.categoryColor[3][1], hubSettings.categoryColor[3][2], 1.0f }, // Transiciones
        { hubSettings.categoryColor[4][0], hubSettings.categoryColor[4][1], hubSettings.categoryColor[4][2], 1.0f }, // Anuncios
        { hubSettings.categoryColor[5][0], hubSettings.categoryColor[5][1], hubSettings.categoryColor[5][2], 1.0f }, // Captura
    };

    if (libSettings.showRender || m_CurrentSection == StylesSection::Render) {
        railItems.push_back({ (int)StylesSection::Render, AppIcons::DrawIcon_Swap, "Render" });
        railColors.push_back({ 0.93f, 0.28f, 0.60f, 1.0f });
    }
    if (libSettings.showWeb || m_CurrentSection == StylesSection::Web) {
        railItems.push_back({ (int)StylesSection::Web, AppIcons::DrawIcon_Globe, "Web" });
        railColors.push_back({ 0.39f, 0.40f, 0.95f, 1.0f });
    }
    if (libSettings.showModel3D || m_CurrentSection == StylesSection::Model3D) {
        railItems.push_back({ (int)StylesSection::Model3D, AppIcons::DrawIcon_Cube3D, "3D" });
        railColors.push_back({ 0.55f, 0.36f, 0.96f, 1.0f });
    }
    if (libSettings.showLab || m_CurrentSection == StylesSection::Lab) {
        railItems.push_back({ (int)StylesSection::Lab, AppIcons::DrawIcon_Formula, "Lab" });
        railColors.push_back({ 0.66f, 0.33f, 0.97f, 1.0f });
    }
    if (libSettings.showPlugins || m_CurrentSection == StylesSection::Plugins) {
        railItems.push_back({ (int)StylesSection::Plugins, AppIcons::DrawIcon_Plugin, "Plugins" });
        railColors.push_back({ 0.96f, 0.62f, 0.04f, 1.0f });
    }

    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    const bool isBottomActive = (m_UIManager && !m_UIManager->IsPanelCollapsed(1) && !m_UIManager->IsAnyToolWindowOpen());
    int activeRailIndex = isBottomActive ? currentIndex : -1;

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

        bool clicked = RenderIconRail(railItems.data(), (int)railItems.size(), activeRailIndex, IconRailOrientation::Vertical, (const float(*)[4])railColors.data());
        if (clicked) {
            SelectSection((StylesSection)activeRailIndex);
        }

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

        bool clicked = RenderIconRail(railItems.data(), (int)railItems.size(), activeRailIndex, IconRailOrientation::Horizontal, (const float(*)[4])railColors.data(), 44.0f);
        if (clicked) {
            SelectSection((StylesSection)activeRailIndex);
        }

        ImGui::SetCursorPos(ImVec2(std::max(0.0f, totalW - 38.0f), std::max(0.0f, (railH - 32.0f) * 0.5f)));
        RenderTransitionRailButton();

        ImGui::EndChild();
        RenderStylesHubDivider(false, totalW);
    }

    // Si la barra inferior está colapsada ("dejando solo los botones para dar espacio"),
    // no dibujamos el contenido (galerías, pestañas) para dejar libre el espacio vertical a Home y Live.
    const bool isCollapsed = (m_UIManager && m_UIManager->IsPanelCollapsed(1)) ||
                             (vertical ? (totalW <= IconRailThickness(true) + 8.0f)
                                       : (totalH <= IconRailThickness(false) + 8.0f));
    if (isCollapsed)
    {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    // ── Contenido de la seccion activa ───────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##stylesContent", ImVec2(0.f, 0.f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case StylesSection::Backgrounds:
            m_Backgrounds.RenderContent();
            break;
        case StylesSection::Multimedia:
            RenderMultimedia();
            break;
        case StylesSection::Overlays:
            if (m_OverlayTab) m_OverlayTab->Render();
            break;
        case StylesSection::Styles:
            m_Styles.RenderContent();
            break;
        case StylesSection::Shaders:
            m_Shaders.RenderContent();
            break;
        case StylesSection::Transitions:
            m_TransitionsTab.RenderContent();
            break;
        case StylesSection::Announcements:
            if (m_UIManager) m_Announcements.Render(m_UIManager->GetGlassRenderer());
            break;
        case StylesSection::Capture:
            m_Capture.RenderContent();
            break;
        case StylesSection::Render:
            if (m_UIManager && m_UIManager->GetLibraryPanelRef())
                m_UIManager->GetLibraryPanelRef()->RenderConverterSection();
            break;
        case StylesSection::Web:
            if (m_UIManager && m_UIManager->GetLibraryPanelRef() && m_UIManager->GetLibraryPanelRef()->GetWebBrowserPanel())
                m_UIManager->GetLibraryPanelRef()->GetWebBrowserPanel()->Render();
            break;
        case StylesSection::Model3D:
            if (m_UIManager && m_UIManager->GetLibraryPanelRef() && m_UIManager->GetLibraryPanelRef()->GetModel3DPanel())
                m_UIManager->GetLibraryPanelRef()->GetModel3DPanel()->Render();
            break;
        case StylesSection::Lab:
            if (m_UIManager && m_UIManager->GetLibraryPanelRef() && m_UIManager->GetLibraryPanelRef()->GetLabPanel())
                m_UIManager->GetLibraryPanelRef()->GetLabPanel()->Render();
            break;
        case StylesSection::Plugins:
            if (m_UIManager && m_UIManager->GetLibraryPanelRef() && m_UIManager->GetLibraryPanelRef()->GetPluginsPanel())
                m_UIManager->GetLibraryPanelRef()->GetPluginsPanel()->Render();
            break;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

void StylesHubPanel::RenderMultimedia()
{
    static Library::MultimediaFilter mmFilter = Library::MultimediaFilter::All;
    Library::RenderMultimediaSection(m_MultimediaSearch, (int)sizeof(m_MultimediaSearch), mmFilter, []() {
        Library::ImportMultimediaFile();
    });
}

} // namespace ProyecThor::UI
