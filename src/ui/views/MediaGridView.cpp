#include "MediaGridView.h"
#include "ui/framework/UIManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/biblio/LibraryIcons.h"
#include "ui/panels/biblio/LibraryStyles.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/monitor/MonitorTheme.h"
#include "ui/panels/layers/LayersTheme.h"
#include "core/PresentationCore.h"
#include "core/MediaImportManager.h"
#include "core/FileDeletionManager.h"
#include "ui/views/Audio.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;
namespace MT = ProyecThor::UI::MonitorTheme;

namespace ProyecThor::UI {

MediaGridView::MediaGridView()
{
}

MediaGridView::~MediaGridView()
{
}

void MediaGridView::Render(Core::CollectionMediaType type, Core::MPVBasePlayer* previewPlayer)
{
    auto& colMgr = Core::MediaCollectionManager::Get();
    m_ActiveCollection = colMgr.GetActiveCollection(type);

    // Retrieve media item count
    int totalCount = 0;
    if (type == Core::CollectionMediaType::Video)
        totalCount = (int)Library::GetVideoList().size();
    else if (type == Core::CollectionMediaType::Image)
        totalCount = (int)Library::GetImageList().size();
    else if (type == Core::CollectionMediaType::Audio)
        totalCount = (int)Library::GetAudioList().size();

    RenderHeader(type, totalCount);
    ImGui::Spacing();
    RenderCollectionTabs(type, totalCount);
    ImGui::Spacing();
    RenderCardGrid(type, previewPlayer);
    RenderModals(type);
}

void MediaGridView::RenderHeader(Core::CollectionMediaType type, int totalItems)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float availW   = ImGui::GetContentRegionAvail().x;

    const char* typeName = (type == Core::CollectionMediaType::Video) ? "Videos" :
                           (type == Core::CollectionMediaType::Image) ? "Imágenes" : "Música";

    ImU32 accentCol = (type == Core::CollectionMediaType::Video) ? IM_COL32(56, 189, 248, 255) :
                      (type == Core::CollectionMediaType::Image) ? IM_COL32(168, 85, 247, 255) :
                                                                   IM_COL32(34, 197, 94, 255);

    // ── Title & Item count ──────────────────────────────────────────────────
    ImGui::BeginGroup();
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float iconSz = 22.0f;
        if (type == Core::CollectionMediaType::Video)
            Library::DrawIcon_Video(dl, p, iconSz, accentCol);
        else if (type == Core::CollectionMediaType::Image)
            Library::DrawIcon_Image(dl, p, iconSz, accentCol);
        else
            Library::DrawIcon_Audio(dl, p, iconSz, accentCol);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconSz + 8.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", typeName);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d archivos)", totalItems);
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    // ── Search Bar & Actions layout ──────────────────────────────────────────
    auto& colMgr = Core::MediaCollectionManager::Get();
    bool isPlayingThisQueue = colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type;

    auto* ap = (type == Core::CollectionMediaType::Audio) ? UI::AudioPanel::GetActiveInstance() : nullptr;
    bool isAudioPlaying = (ap && ap->GetIsPlaying());

    float sortW = 120.0f;
    float importW = 78.0f;
    float queueControlsW = 0.0f;
    bool showQueueBadge = false;
    float queueBadgeW = 55.0f;

    if (isPlayingThisQueue) {
        queueControlsW = 76.0f + 4.0f + 26.0f + 4.0f + 26.0f + 4.0f + 28.0f; // Stop + Prev + Next + Loop
        showQueueBadge = true;
    } else {
        queueControlsW = 105.0f; // Play
        if (isAudioPlaying) {
            queueControlsW += 6.0f + 76.0f; // + Stop Audio
        }
    }
    if (type == Core::CollectionMediaType::Image) {
        queueControlsW += 10.0f + 110.0f; // Slide dur
    }

    float leftGroupEnd = ImGui::GetCursorPosX();
    // Margen de seguridad de 14px respecto al borde derecho para que nunca se corte ningún botón
    float safeMaxRight = ImGui::GetWindowContentRegionMax().x - 14.0f;

    float fixedBtnsTotalW = sortW + 6.0f + importW + 6.0f + queueControlsW + (showQueueBadge ? (8.0f + queueBadgeW) : 0.0f);
    float remainingSpace = safeMaxRight - leftGroupEnd - fixedBtnsTotalW - 10.0f;
    float searchW = std::clamp(remainingSpace, 65.0f, 170.0f);
    float totalClusterW = searchW + 6.0f + fixedBtnsTotalW;

    float clusterStartX = safeMaxRight - totalClusterW;
    if (clusterStartX > leftGroupEnd + 6.0f) {
        ImGui::SetCursorPosX(clusterStartX);
    } else {
        ImGui::SameLine(0.0f, 6.0f);
    }

    // Buscador
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.0f, 5.0f));
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##mediaSearch", "Buscar...", m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 6.0f);

    // Selector de Ordenamiento (Sort Dropdown)
    ImGui::SetNextItemWidth(sortW);
    static const char* kSortNames[] = {
        "Nombre (A - Z)",
        "Nombre (Z - A)",
        "Más recientes",
        "Más antiguos",
        "Tamaño (mayor)"
    };
    int currentSort = static_cast<int>(m_SortMode);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(6.0f, 5.0f));
    if (ImGui::Combo("##mediaSort", &currentSort, kSortNames, IM_ARRAYSIZE(kSortNames))) {
        m_SortMode = static_cast<MediaSortMode>(currentSort);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ordenar elementos por nombre, fecha o tamaño");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 6.0f);

    // Botón Importar
    if (DS::GlassButton("+ Importar", ImVec2(importW, DS::ButtonHeight))) {
        Core::MediaImportManager::Get().OpenFilePickerForImport(Core::MediaImportDest::Multimedia);
    }

    ImGui::SameLine(0.0f, 6.0f);

    // Controles de Cola / Reproducción
    if (isPlayingThisQueue) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(239, 68, 68, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(248, 113, 113, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(220, 38, 38, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button("⏹ Detener", ImVec2(76.0f, DS::ButtonHeight))) {
            colMgr.StopQueue();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Detener cola en reproducción:\n%s", colMgr.GetQueueCurrentItem().c_str());
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("⏮", ImVec2(26.0f, DS::ButtonHeight))) colMgr.Previous();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Archivo anterior");

        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("⏭", ImVec2(26.0f, DS::ButtonHeight))) colMgr.Next();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Siguiente archivo");

        bool isLoop = colMgr.GetLoop(type, m_ActiveCollection);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, isLoop ? IM_COL32(56, 189, 248, 90) : IM_COL32(255, 255, 255, 12));
        ImGui::PushStyleColor(ImGuiCol_Border, isLoop ? IM_COL32(56, 189, 248, 220) : IM_COL32(255, 255, 255, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::Button("🔁", ImVec2(28.0f, DS::ButtonHeight))) {
            colMgr.SetLoop(type, m_ActiveCollection, !isLoop);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Bucle de cola: %s", isLoop ? "ACTIVADO" : "DESACTIVADO");
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        if (showQueueBadge) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "[%d/%d]",
                               colMgr.GetQueueCurrentIndex() + 1, colMgr.GetQueueTotalItems());
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(16, 185, 129, 210));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 211, 153, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(5, 150, 105, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button("▶ Reproducir", ImVec2(105.0f, DS::ButtonHeight))) {
            std::vector<std::string> allFiles;
            if (!m_ActiveCollection.empty() && m_ActiveCollection.rfind("__root__", 0) != 0) {
                const auto* c = colMgr.GetCollection(type, m_ActiveCollection);
                if (c) allFiles = c->itemFilenames;
            } else {
                std::string targetRoot = (m_ActiveCollection.rfind("__root__", 0) == 0)
                    ? m_ActiveCollection.substr(8) : "ProyecThor";
                std::vector<Library::MMItem> src;
                if (type == Core::CollectionMediaType::Video) src = Library::GetVideoList();
                else if (type == Core::CollectionMediaType::Image) src = Library::GetImageList();
                else src = Library::GetAudioList();
                for (const auto& it : src) {
                    std::string r = it.rootLabel.empty() ? "ProyecThor" : it.rootLabel;
                    if (r == targetRoot) allFiles.push_back(it.filename);
                }
            }
            colMgr.StartQueue(type, m_ActiveCollection, allFiles, 0);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reproducir archivos de esta carpeta en cola");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (isAudioPlaying) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(239, 68, 68, 210));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(248, 113, 113, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(220, 38, 38, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
            if (ImGui::Button("⏹ Detener", ImVec2(76.0f, DS::ButtonHeight))) {
                ap->Stop();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Detener audio en reproducción");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
    }

    if (type == Core::CollectionMediaType::Image) {
        ImGui::SameLine(0.0f, 10.0f);
        float duration = colMgr.GetSlideDuration(m_ActiveCollection);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("⏱ Slide:");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(65.0f);
        if (ImGui::DragFloat("##slideDur", &duration, 0.5f, 1.0f, 60.0f, "%.1fs")) {
            colMgr.SetSlideDuration(m_ActiveCollection, duration);
        }
    }
}

void MediaGridView::RenderCollectionTabs(Core::CollectionMediaType type, int totalFiles)
{
    auto& colMgr = Core::MediaCollectionManager::Get();
    auto colNames = colMgr.GetCollectionNames(type);

    std::vector<Library::MMItem> sourceItems;
    if (type == Core::CollectionMediaType::Video)
        sourceItems = Library::GetVideoList();
    else if (type == Core::CollectionMediaType::Image)
        sourceItems = Library::GetImageList();
    else
        sourceItems = Library::GetAudioList();

    std::unordered_map<std::string, int> rootCounts;
    for (const auto& item : sourceItems) {
        std::string rl = item.rootLabel.empty() ? "ProyecThor" : item.rootLabel;
        rootCounts[rl]++;
    }

    std::vector<std::string> roots;
    roots.push_back("ProyecThor");
    if (true) {
        auto extraRoots = Library::GetAvailableRootLabels(Core::ItemType::Video);
        for (const auto& r : extraRoots) {
            if (r != "ProyecThor" && std::find(roots.begin(), roots.end(), r) == roots.end()) {
                roots.push_back(r);
            }
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

    // Tab 1: "ProyecThor" (antes "Todos")
    bool isProyecThorActive = m_ActiveCollection.empty() || m_ActiveCollection == "__root__ProyecThor";
    if (isProyecThorActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(56, 189, 248, 60));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 189, 248, 80));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(56, 189, 248, 100));
        ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(56, 189, 248, 180));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(255, 255, 255, 12));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 24));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(255, 255, 255, 36));
        ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(255, 255, 255, 20));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    int proCount = rootCounts["ProyecThor"];
    std::string proLbl = "📁 ProyecThor (" + std::to_string(proCount) + ")";
    if (ImGui::Button(proLbl.c_str())) {
        m_ActiveCollection = "";
        colMgr.SetActiveCollection(type, "");
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // External roots tabs (solo para usuarios PRO con carpetas externas agregadas)
    if (true) {
        for (size_t ri = 1; ri < roots.size(); ++ri) {
            const std::string& rName = roots[ri];
            std::string rootKey = "__root__" + rName;
            bool isRootActive = (m_ActiveCollection == rootKey);

            ImGui::SameLine();
            if (isRootActive) {
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(56, 189, 248, 60));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 189, 248, 80));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(56, 189, 248, 100));
                ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(56, 189, 248, 180));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(255, 255, 255, 12));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 24));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(255, 255, 255, 36));
                ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(255, 255, 255, 20));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            int rCount = rootCounts[rName];
            std::string rLbl = "📁 " + rName + " (" + std::to_string(rCount) + ")";
            if (ImGui::Button(rLbl.c_str())) {
                m_ActiveCollection = rootKey;
                colMgr.SetActiveCollection(type, rootKey);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
    }

    // User Collections tabs
    for (const auto& name : colNames) {
        ImGui::SameLine();
        const auto* col = colMgr.GetCollection(type, name);
        int count = col ? (int)col->itemFilenames.size() : 0;
        bool isActive = (m_ActiveCollection == name);

        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(56, 189, 248, 60));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 189, 248, 80));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(56, 189, 248, 100));
            ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(56, 189, 248, 180));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(255, 255, 255, 12));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 24));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(255, 255, 255, 36));
            ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(255, 255, 255, 20));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        std::string tabLbl = "📁 " + name + " (" + std::to_string(count) + ")";
        if (ImGui::Button(tabLbl.c_str())) {
            m_ActiveCollection = name;
            colMgr.SetActiveCollection(type, name);
        }

        // Drag & Drop Target: Dropping item on collection tab asks Mover o Duplicar
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MEDIA_ITEM_FILENAME")) {
                m_PendingMoveOrCopyFilename = (const char*)payload->Data;
                m_PendingMoveOrCopyTargetCol = name;
                m_ShowMoveOrCopyModal = true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    // Tab "+ Nueva Carpeta / Colección"
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(255, 255, 255, 8));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 20));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(255, 255, 255, 32));
    ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(255, 255, 255, 25));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (ImGui::Button("+ Nueva Colección")) {
        m_ShowNewCollectionModal = true;
        m_NewCollectionNameBuf[0] = '\0';
        m_NewCollectionTargetRootIdx = 0;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // If a custom collection is active (not a root), show quick actions (Rename / Delete)
    if (!m_ActiveCollection.empty() && m_ActiveCollection.rfind("__root__", 0) != 0) {
        ImGui::SameLine(0.0f, 16.0f);
        if (ImGui::SmallButton("✏ Renombrar")) {
            m_ShowRenameCollectionModal = true;
            m_RenameTargetCol = m_ActiveCollection;
            strncpy_s(m_RenameCollectionNameBuf, m_ActiveCollection.c_str(), sizeof(m_RenameCollectionNameBuf) - 1);
        }
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(239, 68, 68, 255));
        if (ImGui::SmallButton("🗑 Eliminar Colección")) {
            colMgr.DeleteCollection(type, m_ActiveCollection);
            m_ActiveCollection = "";
            colMgr.SetActiveCollection(type, "");
        }
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar(2);
}

void MediaGridView::RenderCardGrid(Core::CollectionMediaType type, Core::MPVBasePlayer* previewPlayer)
{
    std::vector<Library::MMItem> sourceItems;
    if (type == Core::CollectionMediaType::Video)
        sourceItems = Library::GetVideoList();
    else if (type == Core::CollectionMediaType::Image)
        sourceItems = Library::GetImageList();
    else
        sourceItems = Library::GetAudioList();

    // Filter by Active Collection
    auto& colMgr = Core::MediaCollectionManager::Get();
    std::vector<Library::MMItem> filtered;
    filtered.reserve(sourceItems.size());

    const auto* col = m_ActiveCollection.empty() ? nullptr : colMgr.GetCollection(type, m_ActiveCollection);

    std::string searchLower = m_SearchBuffer;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    std::string rootFilter = "";
    if (m_ActiveCollection.rfind("__root__", 0) == 0) {
        rootFilter = m_ActiveCollection.substr(8);
    } else if (m_ActiveCollection.empty()) {
        rootFilter = "ProyecThor";
    }

    if (col) {
        // En una colección personalizada, preservar el orden exacto de la colección
        for (const auto& fn : col->itemFilenames) {
            auto it = std::find_if(sourceItems.begin(), sourceItems.end(),
                                   [&](const Library::MMItem& item) { return item.filename == fn; });
            if (it != sourceItems.end()) {
                if (!searchLower.empty()) {
                    std::string lo = it->filename;
                    std::transform(lo.begin(), lo.end(), lo.begin(),
                                   [](unsigned char c) { return (char)std::tolower(c); });
                    if (lo.find(searchLower) == std::string::npos) continue;
                }
                filtered.push_back(*it);
            }
        }
    } else {
        for (const auto& it : sourceItems) {
            std::string itemRoot = it.rootLabel.empty() ? "ProyecThor" : it.rootLabel;
            if (itemRoot != rootFilter) continue;

            if (!searchLower.empty()) {
                std::string lo = it.filename;
                std::transform(lo.begin(), lo.end(), lo.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
                if (lo.find(searchLower) == std::string::npos) continue;
            }
            filtered.push_back(it);
        }
    }

    // ── Apply Sorting ────────────────────────────────────────────────────────
    if (m_SortMode == MediaSortMode::NameAsc) {
        std::sort(filtered.begin(), filtered.end(), [](const Library::MMItem& a, const Library::MMItem& b) {
            std::string la = a.filename;
            std::string lb = b.filename;
            std::transform(la.begin(), la.end(), la.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            std::transform(lb.begin(), lb.end(), lb.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return la < lb;
        });
    } else if (m_SortMode == MediaSortMode::NameDesc) {
        std::sort(filtered.begin(), filtered.end(), [](const Library::MMItem& a, const Library::MMItem& b) {
            std::string la = a.filename;
            std::string lb = b.filename;
            std::transform(la.begin(), la.end(), la.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            std::transform(lb.begin(), lb.end(), lb.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return la > lb;
        });
    } else if (m_SortMode == MediaSortMode::DateNewest || m_SortMode == MediaSortMode::DateOldest) {
        static std::unordered_map<std::string, std::filesystem::file_time_type> s_TimeCache;
        auto getFileTime = [](const Library::MMItem& it) -> std::filesystem::file_time_type {
            std::string path = Library::GetMMItemFullPath(it);
            auto fit = s_TimeCache.find(path);
            if (fit != s_TimeCache.end()) return fit->second;
            std::error_code ec;
#ifdef _WIN32
            auto ftime = std::filesystem::last_write_time(ProyecThor::Library::Utf8ToWide(path), ec);
#else
            auto ftime = std::filesystem::last_write_time(path, ec);
#endif
            s_TimeCache[path] = ftime;
            return ftime;
        };

        if (m_SortMode == MediaSortMode::DateNewest) {
            std::sort(filtered.begin(), filtered.end(), [&](const Library::MMItem& a, const Library::MMItem& b) {
                return getFileTime(a) > getFileTime(b);
            });
        } else {
            std::sort(filtered.begin(), filtered.end(), [&](const Library::MMItem& a, const Library::MMItem& b) {
                return getFileTime(a) < getFileTime(b);
            });
        }
    } else if (m_SortMode == MediaSortMode::SizeDesc) {
        static std::unordered_map<std::string, uintmax_t> s_SizeCache;
        auto getFileSize = [](const Library::MMItem& it) -> uintmax_t {
            std::string path = Library::GetMMItemFullPath(it);
            auto fit = s_SizeCache.find(path);
            if (fit != s_SizeCache.end()) return fit->second;
            std::error_code ec;
#ifdef _WIN32
            uintmax_t sz = std::filesystem::file_size(ProyecThor::Library::Utf8ToWide(path), ec);
#else
            uintmax_t sz = std::filesystem::file_size(path, ec);
#endif
            s_SizeCache[path] = sz;
            return sz;
        };
        std::sort(filtered.begin(), filtered.end(), [&](const Library::MMItem& a, const Library::MMItem& b) {
            return getFileSize(a) > getFileSize(b);
        });
    }

    if (filtered.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        ImGui::TextDisabled("No hay elementos para mostrar en esta vista.");
        if (!m_ActiveCollection.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Arrastra elementos hacia la pestaña '%s' para agregarlos a esta colección.", m_ActiveCollection.c_str());
        }
        return;
    }

    auto& core = Core::PresentationCore::Get();
    std::string currentBgPath = core.GetState().bgPath;
    auto currentSelection = core.PeekSelection();

    bool isQueueRunningThisType = colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type;
    std::string queueCurrentItem = isQueueRunningThisType ? colMgr.GetQueueCurrentItem() : "";
    bool isAudio = (type == Core::CollectionMediaType::Audio);

    // Iniciar el área con scroll
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f));
    ImGui::BeginChild("##mediaCardGridScroll", ImVec2(0.0f, 0.0f), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Grid Dimensions ──────────────────────────────────────────────────────
    float availW = ImGui::GetContentRegionAvail().x;
    float gap = isAudio ? 10.0f : 12.0f;
    float minCardW = isAudio ? 110.0f : 200.0f;

    int cols = std::max(1, (int)((availW + gap) / (minCardW + gap)));
    // En pantallas pequeñas / medianas, para música asegurar al menos 6 columnas estilo Spotify
    if (isAudio && availW >= 660.0f && cols < 6) {
        cols = 6;
    }
    float cardW = std::max(minCardW, (availW - (cols - 1) * gap) / cols);
    // Portadas de música cuadradas + franja de nombre inferior; Video/Imagen formato 16:9
    float cardH = isAudio ? (cardW + 48.0f) : (cardW * 0.72f);
    float rowH  = cardH + gap;

    // Aceleración de scroll de rueda para desplazamiento suave y ágil
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float curScroll = ImGui::GetScrollY();
            ImGui::SetScrollY(curScroll - wheel * (rowH * 0.75f));
        }
    }

    int totalItems = static_cast<int>(filtered.size());
    int totalRows  = (totalItems + cols - 1) / cols;

    // Virtualización por filas (ImGuiListClipper): rendimiento extremo y cero lag al scrollear
    ImGuiListClipper clipper;
    clipper.Begin(totalRows, rowH);

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            int startIdx = row * cols;
            int endIdx   = std::min(totalItems, startIdx + cols);

            for (int itemIdx = startIdx; itemIdx < endIdx; ++itemIdx) {
                const auto& item = filtered[itemIdx];
                ImGui::PushID(itemIdx);

                std::string fullPath = Library::GetMMItemFullPath(item);
                bool isLive = false;
                if (isAudio) {
                    if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                        isLive = ap->IsPlayingFile(item.filename);
                    }
                } else {
                    isLive = (core.IsProjecting() && currentBgPath == fullPath);
                }
                bool isQueueActive = (isQueueRunningThisType && queueCurrentItem == item.filename);
                bool isPreview = (currentSelection.title == item.filename);

                ImVec2 cardMin = ImGui::GetCursorScreenPos();
                ImVec2 cardMax = { cardMin.x + cardW, cardMin.y + cardH };

                bool hovRaw = ImGui::IsMouseHoveringRect(cardMin, cardMax);
                float tHov = UI::LPHoverLerp(ImGui::GetID(("##hov_mcard_" + item.filename).c_str()), hovRaw);
                float inset = 2.0f * tHov;
                ImVec2 p0 = { cardMin.x - inset, cardMin.y - inset };
                ImVec2 p1 = { cardMax.x + inset, cardMax.y + inset };

                bool isActiveItem = isLive || isQueueActive;
                ImVec4 borderTarget = isActiveItem ? LP::Green : (isPreview ? LP::Accent : (hovRaw ? LP::BorderHov : LP::Border));
                float borderThickness = isActiveItem ? 2.2f : (isPreview ? 1.8f : (1.0f + 0.8f * tHov));

                ImU32 cardBg = isAudio
                    ? (hovRaw ? IM_COL32(36, 40, 52, 255) : LPU32(LP::Surface1))
                    : LPU32(LP::Surface1);

                dl->AddRectFilled(p0, p1, cardBg, DS::RadiusCard);
                dl->AddRect(p0, p1, LPU32(borderTarget), DS::RadiusCard, 0, borderThickness);

                // Closures de reproducción y colas
                auto StartQueueFromHere = [&]() {
                    std::vector<std::string> allFiles;
                    if (!m_ActiveCollection.empty() && m_ActiveCollection.rfind("__root__", 0) != 0) {
                        const auto* c = colMgr.GetCollection(type, m_ActiveCollection);
                        if (c) allFiles = c->itemFilenames;
                    }
                    if (allFiles.empty()) {
                        for (const auto& it : filtered) allFiles.push_back(it.filename);
                    }
                    int startQ = 0;
                    for (int q = 0; q < (int)allFiles.size(); ++q) {
                        if (allFiles[q] == item.filename) { startQ = q; break; }
                    }
                    colMgr.StartQueue(type, m_ActiveCollection, allFiles, startQ);
                };

                auto StopActivePlayback = [&]() {
                    if (colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type) {
                        colMgr.StopQueue();
                    }
                    if (isAudio) {
                        if (auto* ap = UI::AudioPanel::GetActiveInstance()) ap->Stop();
                    } else {
                        core.StopBackgroundMedia();
                    }
                };

                auto PlayItemLiveOrQueue = [&]() {
                    if (isAudio) {
                        if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                            ap->StopMusicOnly();
                        }
                        StartQueueFromHere();
                        return;
                    }
                    if (colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type) {
                        for (int qIdx = 0; qIdx < colMgr.GetQueueTotalItems(); ++qIdx) {
                            if (colMgr.GetQueueItem(qIdx) == item.filename) {
                                colMgr.PlayQueueIndex(qIdx);
                                return;
                            }
                        }
                    }
                    core.SetProjecting(true);
                    if (type == Core::CollectionMediaType::Video) {
                        core.SetBackgroundMedia(fullPath, true, true);
                    } else if (type == Core::CollectionMediaType::Image) {
                        core.SetBackgroundMedia(fullPath, false, false);
                    }
                };

                if (isAudio) {
                    // ═════════════════════════════════════════════════════════════
                    // AUDIO / CANCIÓN: PORTADA CUADRADA 1:1 TIPO SPOTIFY + HOVER PLAY
                    // ═════════════════════════════════════════════════════════════
                    float pad = 7.0f;
                    float thumbSz = cardW - pad * 2.0f;
                    ImVec2 thumbMin = { cardMin.x + pad, cardMin.y + pad };
                    ImVec2 thumbMax = { thumbMin.x + thumbSz, thumbMin.y + thumbSz };

                    ImTextureID thumbTex = Library::GetMMAudioThumbnail(fullPath);
                    if (thumbTex) {
                        dl->AddImageRounded(thumbTex, thumbMin, thumbMax, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, DS::RadiusMedium);
                    } else {
                        dl->AddRectFilledMultiColor(thumbMin, thumbMax,
                            IM_COL32(22, 27, 36, 255), IM_COL32(32, 38, 50, 255),
                            IM_COL32(18, 22, 30, 255), IM_COL32(15, 18, 24, 255));
                        dl->AddRect(thumbMin, thumbMax, IM_COL32(255, 255, 255, 15), DS::RadiusMedium);
                        float iconSz = std::min(thumbSz * 0.35f, 28.0f);
                        ImVec2 iconPos = { thumbMin.x + (thumbSz - iconSz) * 0.5f, thumbMin.y + (thumbSz - iconSz) * 0.5f };
                        Library::DrawIcon_Audio(dl, iconPos, iconSz, IM_COL32(34, 197, 94, 130));
                    }

                    // Badge de formato de archivo (MP3, FLAC, WAV, etc.)
                    std::string ext = fs::path(item.filename).extension().string();
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
                    ImVec2 bMin = { thumbMin.x + 4.0f, thumbMin.y + 4.0f };
                    ImVec2 extSz = ImGui::CalcTextSize(ext.c_str());
                    dl->AddRectFilled(bMin, { bMin.x + extSz.x + 6.0f, bMin.y + extSz.y + 3.0f }, IM_COL32(0, 0, 0, 190), 3.0f);
                    dl->AddText({ bMin.x + 3.0f, bMin.y + 1.0f }, IM_COL32(210, 210, 210, 255), ext.c_str());

                    // Botón flotante circular tipo Spotify al pasar el mouse o reproducir
                    float playBtnR = std::clamp(thumbSz * 0.18f, 14.0f, 20.0f);
                    ImVec2 playBtnC = { thumbMax.x - playBtnR - 5.0f, thumbMax.y - playBtnR - 5.0f };
                    bool playBtnHov = hovRaw && (std::hypot(ImGui::GetIO().MousePos.x - playBtnC.x, ImGui::GetIO().MousePos.y - playBtnC.y) <= playBtnR);

                    if (hovRaw || isLive) {
                        dl->AddCircleFilled({ playBtnC.x, playBtnC.y + 2.0f }, playBtnR + 1.0f, IM_COL32(0, 0, 0, 110));
                        ImU32 circleCol = isLive
                            ? (playBtnHov ? IM_COL32(248, 113, 113, 255) : IM_COL32(239, 68, 68, 255))
                            : (playBtnHov ? IM_COL32(52, 211, 153, 255) : IM_COL32(34, 197, 94, 255));
                        dl->AddCircleFilled(playBtnC, playBtnR, circleCol);

                        if (isLive) {
                            float sq = playBtnR * 0.38f;
                            dl->AddRectFilled({ playBtnC.x - sq, playBtnC.y - sq }, { playBtnC.x + sq, playBtnC.y + sq }, IM_COL32_WHITE, 1.5f);
                        } else {
                            float tri = playBtnR * 0.44f;
                            ImVec2 pA = { playBtnC.x - tri * 0.6f, playBtnC.y - tri };
                            ImVec2 pB = { playBtnC.x - tri * 0.6f, playBtnC.y + tri };
                            ImVec2 pC = { playBtnC.x + tri * 0.9f, playBtnC.y };
                            dl->AddTriangleFilled(pA, pB, pC, IM_COL32_WHITE);
                        }
                    }

                    // Metadatos (Título y Subtítulo)
                    float infoY = thumbMax.y + 5.0f;
                    std::string dispName = item.filename;
                    if (dispName.size() > ext.size() + 1) dispName = dispName.substr(0, dispName.size() - ext.size() - 1);
                    float maxTextW = cardW - pad * 2.0f;
                    if (ImGui::CalcTextSize(dispName.c_str()).x > maxTextW) {
                        while (dispName.size() > 3 && ImGui::CalcTextSize((dispName + "...").c_str()).x > maxTextW) {
                            dispName.pop_back();
                        }
                        dispName += "...";
                    }
                    dl->AddText({ cardMin.x + pad, infoY }, isLive ? IM_COL32(34, 197, 94, 255) : DS::TextPrimary, dispName.c_str());

                    std::string rootLabel = item.rootLabel.empty() ? "ProyecThor" : item.rootLabel;
                    std::string subText = ext + " • " + rootLabel;
                    if (ImGui::CalcTextSize(subText.c_str()).x > maxTextW) subText = ext;
                    dl->AddText({ cardMin.x + pad, infoY + 16.0f }, IM_COL32(148, 163, 184, 220), subText.c_str());

                    // Clic / Interacción: Clic en la tarjeta o botón flotante reproduce/detiene en el reproductor
                    ImGui::SetCursorScreenPos(cardMin);
                    std::string invBtnId = "##invCardBtn_" + std::to_string(itemIdx);
                    if (ImGui::InvisibleButton(invBtnId.c_str(), ImVec2(cardW, cardH))) {
                        if (isLive) {
                            StopActivePlayback();
                        } else {
                            if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                                ap->PlayFile(fullPath, /*live=*/false);
                            }
                        }
                    }
                } else {
                    // ═════════════════════════════════════════════════════════════
                    // VIDEO / IMAGEN: RATIO 16:9 + BOTONES PROYECTAR / PREVIEW
                    // ═════════════════════════════════════════════════════════════
                    float thumbH = cardH * 0.60f;
                    ImVec2 thumbMin = cardMin;
                    ImVec2 thumbMax = { cardMin.x + cardW, cardMin.y + thumbH };

                    ImTextureID thumbTex = (type == Core::CollectionMediaType::Video)
                        ? Library::GetMMVideoThumbnail(fullPath)
                        : Library::GetMMImageThumbnail(fullPath);

                    dl->PushClipRect(cardMin, cardMax, true);

                    if (thumbTex) {
                        dl->AddImage(thumbTex, thumbMin, thumbMax, ImVec2(0, 0), ImVec2(1, 1));
                    } else {
                        dl->AddRectFilledMultiColor(thumbMin, thumbMax,
                                                    IM_COL32(20, 22, 30, 255), IM_COL32(28, 32, 44, 255),
                                                    IM_COL32(15, 17, 24, 255), IM_COL32(25, 28, 38, 255));
                        float iconSz = 32.0f;
                        ImVec2 iconPos = { thumbMin.x + (cardW - iconSz) * 0.5f, thumbMin.y + (thumbH - iconSz) * 0.5f };
                        if (type == Core::CollectionMediaType::Video)
                            Library::DrawIcon_Video(dl, iconPos, iconSz, IM_COL32(255, 255, 255, 60));
                        else
                            Library::DrawIcon_Image(dl, iconPos, iconSz, IM_COL32(255, 255, 255, 60));
                    }

                    // Format extension badge
                    std::string ext = fs::path(item.filename).extension().string();
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
                    ImVec2 badgeMin = { thumbMin.x + 6.0f, thumbMin.y + 6.0f };
                    ImVec2 extSz = ImGui::CalcTextSize(ext.c_str());
                    dl->AddRectFilled(badgeMin, { badgeMin.x + extSz.x + 8.0f, badgeMin.y + extSz.y + 4.0f }, IM_COL32(0, 0, 0, 180), 3.0f);
                    dl->AddText({ badgeMin.x + 4.0f, badgeMin.y + 2.0f }, IM_COL32(220, 220, 220, 255), ext.c_str());

                    // Status badge
                    if (isLive || isQueueActive) {
                        const char* tag = isLive ? "● EN VIVO" : "▶ EN COLA";
                        ImVec2 tagSz = ImGui::CalcTextSize(tag);
                        ImVec2 tagMin = { thumbMax.x - tagSz.x - 12.0f, thumbMin.y + 6.0f };
                        dl->AddRectFilled(tagMin, { tagMin.x + tagSz.x + 8.0f, tagMin.y + tagSz.y + 4.0f }, IM_COL32(34, 197, 94, 220), 3.0f);
                        dl->AddText({ tagMin.x + 4.0f, tagMin.y + 2.0f }, IM_COL32(255, 255, 255, 255), tag);
                    } else if (isPreview) {
                        const char* tag = "👁 PREVIEW";
                        ImVec2 tagSz = ImGui::CalcTextSize(tag);
                        ImVec2 tagMin = { thumbMax.x - tagSz.x - 12.0f, thumbMin.y + 6.0f };
                        dl->AddRectFilled(tagMin, { tagMin.x + tagSz.x + 8.0f, tagMin.y + tagSz.y + 4.0f }, IM_COL32(56, 189, 248, 200), 3.0f);
                        dl->AddText({ tagMin.x + 4.0f, tagMin.y + 2.0f }, IM_COL32(255, 255, 255, 255), tag);
                    }

                    // Metadata
                    float infoY = thumbMax.y + 6.0f;
                    std::string dispName = item.filename;
                    if (dispName.size() > 28) dispName = dispName.substr(0, 25) + "...";
                    dl->AddText({ cardMin.x + 8.0f, infoY }, DS::TextPrimary, dispName.c_str());

                    // Buttons
                    float btnY = cardMax.y - 30.0f;
                    float btnH = 22.0f;
                    bool isThisActive = isLive || isQueueActive;
                    const char* pBtnLabel = isThisActive ? ((type == Core::CollectionMediaType::Image) ? "⏹ Quitar" : "⏹ Detener") : "Proyectar";
                    float projBtnW = isThisActive ? 76.0f : 72.0f;
                    ImVec4 pBtnCol = isThisActive ? LP::RedDim : LP::GreenDim;
                    ImVec4 pBtnHov = isThisActive ? LP::Red : LP::Green;
                    ImVec4 pBtnAct = isThisActive ? LP::Red : LP::Green;

                    // Button 1: Proyectar / Detener
                    ImVec2 pBtnPos = { cardMin.x + 8.0f, btnY };
                    ImGui::SetCursorScreenPos(pBtnPos);
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertFloat4ToU32(pBtnCol));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertFloat4ToU32(pBtnHov));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertFloat4ToU32(pBtnAct));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
                    if (ImGui::Button(pBtnLabel, ImVec2(projBtnW, btnH))) {
                        if (isThisActive) StopActivePlayback();
                        else PlayItemLiveOrQueue();
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);

                    // Button 2: Preview
                    float prvBtnW = 62.0f;
                    ImVec2 prvBtnPos = { pBtnPos.x + projBtnW + 6.0f, btnY };
                    ImGui::SetCursorScreenPos(prvBtnPos);
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertFloat4ToU32(LP::AccentDim));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertFloat4ToU32(LP::AccentHov));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertFloat4ToU32(LP::AccentActive));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
                    if (ImGui::Button("Preview", ImVec2(prvBtnW, btnH))) {
                        Core::LibrarySelection sel;
                        sel.title = item.filename;
                        sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video : Core::ItemType::Image;
                        core.SetSelection(sel);
                        if (type == Core::CollectionMediaType::Video) core.RequestPreviewLoad(fullPath, false, true);
                        if (m_UIManagerRef) m_UIManagerRef->SetShowMediaPreviewWindow(true);
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);

                    // Invisible card click button
                    ImGui::SetCursorScreenPos(cardMin);
                    std::string invBtnId = "##invCardBtn_" + std::to_string(itemIdx);
                    if (ImGui::InvisibleButton(invBtnId.c_str(), ImVec2(cardW, cardH - 32.0f))) {
                        Core::LibrarySelection sel;
                        sel.title = item.filename;
                        sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video : Core::ItemType::Image;
                        core.SetSelection(sel);
                        if (type == Core::CollectionMediaType::Video) core.RequestPreviewLoad(fullPath, false, true);
                    }

                    if (hovRaw && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        core.RequestPreviewLoad(fullPath, /*loop=*/false, /*startMuted=*/false);
                    }

                    dl->PopClipRect();
                }

                // ── Drag Source & Tooltip ─────────────────────────────────────
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MEDIA_ITEM_FILENAME", item.filename.c_str(), item.filename.size() + 1);
                    ImGui::Text("Mover '%s'", item.filename.c_str());
                    ImGui::EndDragDropSource();
                }

                if (hovRaw && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetTooltip("%s\nRuta: %s\n(Doble clic para reproducir)", item.filename.c_str(), fullPath.c_str());
                }

                // ── Menú Contextual (Clic derecho) ───────────────────────────
                if (ImGui::BeginPopupContextItem("##cardCtxPopup")) {
                    if (isLive || isQueueActive) {
                        if (ImGui::MenuItem("⏹ Detener Reproducción")) StopActivePlayback();
                    } else {
                        const char* playLbl = isAudio ? "▶ Reproducir (Local)" : "Proyectar a Público";
                        if (ImGui::MenuItem(playLbl)) PlayItemLiveOrQueue();
                    }
                    if (ImGui::MenuItem("▶ Iniciar cola desde aquí")) StartQueueFromHere();
                    if (isAudio) {
                        if (ImGui::MenuItem("Enviar en vivo al proyector")) {
                            if (auto* ap = UI::AudioPanel::GetActiveInstance()) ap->PlayFileLive(fullPath);
                        }
                    } else {
                        if (ImGui::MenuItem("Ver en Vista Previa")) {
                            Core::LibrarySelection sel;
                            sel.title = item.filename;
                            sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video : Core::ItemType::Image;
                            core.SetSelection(sel);
                            if (type == Core::CollectionMediaType::Video) core.RequestPreviewLoad(fullPath, false, true);
                            if (m_UIManagerRef) m_UIManagerRef->SetShowMediaPreviewWindow(true);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::BeginMenu("Mover a Colección...")) {
                        auto allCols = colMgr.GetCollectionNames(type);
                        for (const auto& cName : allCols) {
                            if (ImGui::MenuItem(cName.c_str())) colMgr.AddItemToCollection(type, cName, item.filename);
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("+ Nueva Colección...")) {
                            m_ShowNewCollectionModal = true;
                            m_NewCollectionNameBuf[0] = '\0';
                        }
                        ImGui::EndMenu();
                    }
                    if (!m_ActiveCollection.empty() && m_ActiveCollection.rfind("__root__", 0) != 0) {
                        if (ImGui::MenuItem("Quitar de esta Colección")) {
                            colMgr.RemoveItemFromCollectionByPath(type, m_ActiveCollection, item.filename);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Copiar ruta completa")) {
                        ImGui::SetClipboardText(fullPath.c_str());
                    }
#ifdef _WIN32
                    if (ImGui::MenuItem("Abrir en Explorador de Windows")) {
                        std::string selectArg = "/select,\"" + fullPath + "\"";
                        ShellExecuteA(NULL, "open", "explorer.exe", selectArg.c_str(), NULL, SW_SHOWNORMAL);
                    }
#endif
                    ImGui::Separator();
                    if (ImGui::MenuItem("Eliminar archivo")) {
                        Core::FileDeletionManager::ForceDeleteFile(fullPath);
                        Library::RefreshMultimediaLists();
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();

                if (itemIdx < endIdx - 1) {
                    ImGui::SameLine(0.0f, gap);
                }
            }
            ImGui::Dummy(ImVec2(0.0f, gap));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void MediaGridView::RenderModals(Core::CollectionMediaType type)
{
    auto& colMgr = Core::MediaCollectionManager::Get();

    // ── Modal: Nueva Colección ──────────────────────────────────────────────
    if (m_ShowNewCollectionModal) {
        ImGui::OpenPopup("Nueva Colección##NewMediaColModal");
        m_ShowNewCollectionModal = false;
    }

    ImGuiViewport* vpCol = ImGui::GetMainViewport();
    if (vpCol) {
        ImGui::SetNextWindowViewport(vpCol->ID);
        ImGui::SetNextWindowPos(vpCol->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Nueva Colección##NewMediaColModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Ingresa el nombre para la nueva colección / carpeta:");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##newColName", m_NewCollectionNameBuf, sizeof(m_NewCollectionNameBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();

        std::vector<std::string> roots;
        roots.push_back("ProyecThor");
        if (true) {
            auto extraRoots = Library::GetAvailableRootLabels(Core::ItemType::Video);
            for (const auto& r : extraRoots) {
                if (r != "ProyecThor" && std::find(roots.begin(), roots.end(), r) == roots.end()) {
                    roots.push_back(r);
                }
            }
        }

        std::string chosenRoot = "ProyecThor";
        // Si el usuario es PRO y tiene carpetas de escaneo externas, preguntar la ubicación
        if (roots.size() > 1) {
            ImGui::Separator();
            ImGui::Text("¿Dónde deseas crear la carpeta / cola?");
            for (int ri = 0; ri < (int)roots.size(); ++ri) {
                std::string rLabel = (ri == 0) ? "📁 ProyecThor (carpeta personal)" : ("📁 " + roots[ri]);
                if (ImGui::RadioButton(rLabel.c_str(), m_NewCollectionTargetRootIdx == ri)) {
                    m_NewCollectionTargetRootIdx = ri;
                }
            }
            if (m_NewCollectionTargetRootIdx >= 0 && m_NewCollectionTargetRootIdx < (int)roots.size()) {
                chosenRoot = roots[m_NewCollectionTargetRootIdx];
            }
            ImGui::Spacing();
        }

        if (ImGui::Button("Crear", ImVec2(100.0f, DS::ButtonHeight)) || enterPressed) {
            if (m_NewCollectionNameBuf[0] != '\0') {
                colMgr.CreateCollection(type, m_NewCollectionNameBuf, chosenRoot);
                m_ActiveCollection = m_NewCollectionNameBuf;
                colMgr.SetActiveCollection(type, m_NewCollectionNameBuf);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, DS::ButtonHeight))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Modal: Renombrar Colección ──────────────────────────────────────────
    if (m_ShowRenameCollectionModal) {
        ImGui::OpenPopup("Renombrar Colección##RenameMediaColModal");
        m_ShowRenameCollectionModal = false;
    }

    if (vpCol) {
        ImGui::SetNextWindowViewport(vpCol->ID);
        ImGui::SetNextWindowPos(vpCol->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Renombrar Colección##RenameMediaColModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Nuevo nombre para '%s':", m_RenameTargetCol.c_str());
        ImGui::Spacing();
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##renColName", m_RenameCollectionNameBuf, sizeof(m_RenameCollectionNameBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();

        if (ImGui::Button("Guardar", ImVec2(100.0f, DS::ButtonHeight)) || enterPressed) {
            if (m_RenameCollectionNameBuf[0] != '\0') {
                colMgr.RenameCollection(type, m_RenameTargetCol, m_RenameCollectionNameBuf);
                if (m_ActiveCollection == m_RenameTargetCol) {
                    m_ActiveCollection = m_RenameCollectionNameBuf;
                    colMgr.SetActiveCollection(type, m_RenameCollectionNameBuf);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, DS::ButtonHeight))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Modal: Mover o Duplicar ─────────────────────────────────────────────
    if (m_ShowMoveOrCopyModal) {
        ImGui::OpenPopup("¿Mover o Duplicar?##modalMoveOrCopyGrid");
        m_ShowMoveOrCopyModal = false;
    }

    if (vpCol) {
        ImGui::SetNextWindowViewport(vpCol->ID);
        ImGui::SetNextWindowPos(vpCol->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("¿Mover o Duplicar?##modalMoveOrCopyGrid", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("¿Deseas mover o duplicar (copiar) a la carpeta '%s'?", m_PendingMoveOrCopyTargetCol.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Archivo: %s", m_PendingMoveOrCopyFilename.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("🔀 Mover a esta carpeta", ImVec2(170.0f, DS::ButtonHeight))) {
            if (!m_ActiveCollection.empty() && m_ActiveCollection.rfind("__root__", 0) != 0 &&
                m_ActiveCollection != m_PendingMoveOrCopyTargetCol) {
                colMgr.MoveItemBetweenCollections(type, m_ActiveCollection, m_PendingMoveOrCopyTargetCol, m_PendingMoveOrCopyFilename);
            } else {
                colMgr.AddItemToCollection(type, m_PendingMoveOrCopyTargetCol, m_PendingMoveOrCopyFilename);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("📋 Duplicar (Copiar)", ImVec2(160.0f, DS::ButtonHeight))) {
            colMgr.AddItemToCollection(type, m_PendingMoveOrCopyTargetCol, m_PendingMoveOrCopyFilename);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(90.0f, DS::ButtonHeight))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace ProyecThor::UI
