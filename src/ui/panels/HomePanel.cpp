#include "HomePanel.h"
#include "StylesHubPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "ui/framework/UIStrings.h"
#include "ui/panels/LibraryPanel.h"
#include "core/PresentationCore.h"
#include "core/MediaCollections.h"
#include "media/player/VLCBasePlayer.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/layers/LayersTheme.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

HomePanel::HomePanel() {
}

HomePanel::~HomePanel() {
}

void HomePanel::RenderFloatingPreview(Core::VLCBasePlayer* player, float w, float h)
{
    m_MonitorView.RenderFloatingPreview(player, w, h);
}

void HomePanel::RenderHomeContent()
{
    auto selection = Core::PresentationCore::Get().PeekSelection();
    Core::VLCBasePlayer* previewPlayer = Core::PresentationCore::Get().GetPreviewPlayer();

    LibraryCategory libCat = LibraryCategory::Songs;
    LibrarySideMode sideMode = LibrarySideMode::Categories;
    if (m_UIManagerRef && m_UIManagerRef->GetLibraryPanelRef())
    {
        libCat   = m_UIManagerRef->GetLibraryPanelRef()->GetCategory();
        sideMode = m_UIManagerRef->GetLibraryPanelRef()->GetSideMode();
    }

    if (sideMode == LibrarySideMode::Categories)
    {
        if (libCat == LibraryCategory::Videos)
        {
            m_MediaGridView.Render(Core::CollectionMediaType::Video, previewPlayer);
            return;
        }
        else if (libCat == LibraryCategory::Images)
        {
            m_MediaGridView.Render(Core::CollectionMediaType::Image, previewPlayer);
            return;
        }
        else if (libCat == LibraryCategory::Audio)
        {
            m_MediaGridView.Render(Core::CollectionMediaType::Audio, previewPlayer);
            return;
        }
        else if (libCat == LibraryCategory::Bibles)
        {
            m_BibleView.Render();
            return;
        }
        else if (libCat == LibraryCategory::Documents)
        {
            m_DocumentView.Render(selection.title, selection.contentData);
            return;
        }
    }

    if (selection.type == Core::ItemType::Bible)
    {
        m_BibleView.Render();
    }
    else if (selection.type == Core::ItemType::Documents)
    {
        m_DocumentView.Render(selection.title, selection.contentData);
    }
    else if (selection.type == Core::ItemType::Audio)
    {
        m_MediaGridView.Render(Core::CollectionMediaType::Audio, previewPlayer);
    }
    else if (selection.type == Core::ItemType::Image)
    {
        m_MediaGridView.Render(Core::CollectionMediaType::Image, previewPlayer);
    }
    else if (selection.type == Core::ItemType::Video)
    {
        m_MediaGridView.Render(Core::CollectionMediaType::Video, previewPlayer);
    }
    else
    {
        m_SongView.Render();
    }
}

void HomePanel::Render()
{
    if (m_UIManagerRef) {
        m_MonitorView.SetUIManager(m_UIManagerRef);
        m_MediaGridView.SetUIManager(m_UIManagerRef);
    }

    m_MonitorView.Update();

    if (m_UIManagerRef && m_UIManagerRef->IsPanelCollapsedForRender(GetName()))
        return;

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

    auto* stylesHub = m_UIManagerRef ? m_UIManagerRef->GetStylesHubPanelRef() : nullptr;

    // Si se seleccionó un nuevo contenido desde la Biblioteca, volver automáticamente a la vista de Diapositivas
    auto currentSelection = Core::PresentationCore::Get().PeekSelection();
    if (!currentSelection.title.empty() && currentSelection.title != m_LastSelectionTitle) {
        m_LastSelectionTitle = currentSelection.title;
        if (stylesHub && stylesHub->IsInDesignMode()) {
            stylesHub->SwitchToPresentation();
        }
    }

    // Si el modo diseño estaba en Fondos, lo desactivamos para mostrar Fondos en la sección inferior
    if (stylesHub && stylesHub->IsInDesignMode() && stylesHub->GetCurrentSection() == StylesSection::Backgrounds) {
        stylesHub->SwitchToPresentation();
        m_FondosCollapsed = false;
    }

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    const float kHeaderBarH = 28.0f;
    const float kSplitterH  = 5.0f;
    const float kMinHomeH   = 80.0f;
    const float kMinFondosH = 110.0f;
    const float kMaxFondosH = std::max(kMinFondosH, totalH - kSplitterH - kMinHomeH);

    m_FondosHeight = std::clamp(m_FondosHeight, kMinFondosH, kMaxFondosH);

    const float bottomTotalH = m_FondosCollapsed ? kHeaderBarH : (kSplitterH + m_FondosHeight);
    const float homeH        = std::max(kMinHomeH, totalH - bottomTotalH);

    // ── Área de contenido principal (Home / Diapositivas) ──
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##homeContent", ImVec2(0.0f, homeH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    if (stylesHub && stylesHub->IsInDesignMode() && stylesHub->GetCurrentSection() != StylesSection::Backgrounds)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 0.25f));
        if (ImGui::Button("◀ Volver a Presentación", ImVec2(180.0f, 24.0f))) {
            stylesHub->SwitchToPresentation();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("| Modo de edición activo");
        ImGui::Separator();
        stylesHub->RenderCurrentSectionContent();
    }
    else
    {
        RenderHomeContent();
    }

    ImGui::EndChild();

    if (m_FondosCollapsed)
    {
        // ── Barra colapsada compacta al pie de Main ──
        ImGui::PushStyleColor(ImGuiCol_ChildBg, LP::Surface0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 3.0f));
        ImGui::BeginChild("##fondosCollapsedBar", ImVec2(totalW, kHeaderBarH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImDrawList* dlBar = ImGui::GetWindowDrawList();
        ImVec2 barPos = ImGui::GetCursorScreenPos();

        // Icono de capas
        float iconSz = 16.0f;
        ImVec2 iconPos = ImVec2(barPos.x + 4.0f, barPos.y + (kHeaderBarH - iconSz) * 0.5f);
        AppIcons::DrawIcon_Layers(dlBar, iconPos, iconSz, LPU32(LP::Accent));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconSz + 10.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Text);
        ImGui::TextUnformatted("Fondos");
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::TextUnformatted("(colapsado)");
        ImGui::PopStyleColor();

        float btnW = 110.0f;
        float btnH = 22.0f;
        float rightMargin = 8.0f;
        float btnX = std::max(ImGui::GetCursorPosX(), totalW - btnW - rightMargin);

        ImGui::SameLine(btnX);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface3);
        ImGui::PushStyleColor(ImGuiCol_Border, LP::Border);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Text);

        if (ImGui::Button("\xE2\x96\xB2 Expandir", ImVec2(btnW, btnH)))
        {
            m_FondosCollapsed = false;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        // Permitir clic en cualquier parte vacía de la barra para alternar
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_FondosCollapsed = false;
        }

        ImGui::EndChild();
    }
    else
    {
        // ── Splitter interactivo para redimensionar Fondos ──
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, LP::Accent);
        ImGui::Button("##fondosSplitter", ImVec2(totalW, kSplitterH));
        if (ImGui::IsItemActive())
        {
            float deltaY = ImGui::GetIO().MouseDelta.y;
            m_FondosHeight -= deltaY;
            m_FondosHeight = std::clamp(m_FondosHeight, kMinFondosH, kMaxFondosH);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_FondosCollapsed = true;
            }
        }
        ImGui::PopStyleColor(3);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        float midY = (pMin.y + pMax.y) * 0.5f;
        dl->AddLine(ImVec2(pMin.x, midY), ImVec2(pMax.x, midY), LPU32(LP::Border), 1.0f);
        float midX = (pMin.x + pMax.x) * 0.5f;
        dl->AddLine(ImVec2(midX - 18.0f, midY), ImVec2(midX + 18.0f, midY), LPU32(LP::Accent), 2.0f);

        // ── Galería de Fondos ──
        ImGui::PushStyleColor(ImGuiCol_ChildBg, LP::Base);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        ImGui::BeginChild("##fondosContentArea", ImVec2(totalW, m_FondosHeight), false,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (stylesHub)
        {
            stylesHub->SetBackgroundsOnCollapse([this]() {
                m_FondosCollapsed = true;
            });
            stylesHub->RenderBackgrounds();
        }

        ImGui::EndChild();
    }

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
