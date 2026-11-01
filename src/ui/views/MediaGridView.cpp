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

void MediaGridView::Render(Core::CollectionMediaType type, Core::VLCBasePlayer* previewPlayer)
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

    // ── Search Bar ──────────────────────────────────────────────────────────
    float searchW = std::clamp(availW * 0.28f, 160.0f, 260.0f);
    float rightControlsW = 280.0f;
    float searchX = ImGui::GetWindowWidth() - searchW - rightControlsW - 30.0f;
    if (searchX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(searchX);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##mediaSearch", "Buscar archivo...", m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 8.0f);

    // ── Import Button ───────────────────────────────────────────────────────
    if (DS::GlassButton("+ Importar", ImVec2(100.0f, DS::ButtonHeight))) {
        Core::MediaImportManager::Get().OpenFilePickerForImport(Core::MediaImportDest::Multimedia);
    }

    ImGui::SameLine(0.0f, 8.0f);

    // ── Queue Play / Stop Button ────────────────────────────────────────────
    auto& colMgr = Core::MediaCollectionManager::Get();
    bool isPlayingThisQueue = colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type;

    if (isPlayingThisQueue) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(239, 68, 68, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(248, 113, 113, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(220, 38, 38, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button("⏹ Detener Cola", ImVec2(120.0f, DS::ButtonHeight))) {
            colMgr.StopQueue();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 6.0f);
        if (ImGui::Button("⏮", ImVec2(32.0f, DS::ButtonHeight))) colMgr.Previous();
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("⏭", ImVec2(32.0f, DS::ButtonHeight))) colMgr.Next();

        bool isLoop = colMgr.GetLoop(type, m_ActiveCollection);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, isLoop ? IM_COL32(56, 189, 248, 80) : IM_COL32(255, 255, 255, 12));
        ImGui::PushStyleColor(ImGuiCol_Border, isLoop ? IM_COL32(56, 189, 248, 200) : IM_COL32(255, 255, 255, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::Button(isLoop ? "🔁 Bucle: ON" : "🔁 Bucle: OFF", ImVec2(100.0f, DS::ButtonHeight))) {
            colMgr.SetLoop(type, m_ActiveCollection, !isLoop);
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "▶ [%d / %d] %s",
                           colMgr.GetQueueCurrentIndex() + 1, colMgr.GetQueueTotalItems(),
                           colMgr.GetQueueCurrentItem().c_str());

        if (type == Core::CollectionMediaType::Image) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextDisabled("(%.1fs / %.1fs)", colMgr.GetSlideTimer(), colMgr.GetCurrentSlideDuration());
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(16, 185, 129, 210));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 211, 153, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(5, 150, 105, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button("▶ Reproducir en Cola", ImVec2(160.0f, DS::ButtonHeight))) {
            // Build fallback list if in "Todas"
            std::vector<std::string> allFiles;
            if (type == Core::CollectionMediaType::Video) {
                for (const auto& it : Library::GetVideoList()) allFiles.push_back(it.filename);
            } else if (type == Core::CollectionMediaType::Image) {
                for (const auto& it : Library::GetImageList()) allFiles.push_back(it.filename);
            } else {
                for (const auto& it : Library::GetAudioList()) allFiles.push_back(it.filename);
            }
            colMgr.StartQueue(type, m_ActiveCollection, allFiles, 0);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (type == Core::CollectionMediaType::Audio) {
            auto* ap = UI::AudioPanel::GetActiveInstance();
            if (ap && ap->GetIsPlaying()) {
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(239, 68, 68, 210));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(248, 113, 113, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(220, 38, 38, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
                if (ImGui::Button("⏹ Detener Audio", ImVec2(140.0f, DS::ButtonHeight))) {
                    ap->Stop();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            }
        }
    }

    // ── Extra controls: Slideshow Duration (Images only) ────────────────────
    if (type == Core::CollectionMediaType::Image) {
        ImGui::SameLine(0.0f, 14.0f);
        float duration = colMgr.GetSlideDuration(m_ActiveCollection);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("⏱ Slide:");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::DragFloat("##slideDur", &duration, 0.5f, 1.0f, 60.0f, "%.1fs")) {
            colMgr.SetSlideDuration(m_ActiveCollection, duration);
        }
    }
}

void MediaGridView::RenderCollectionTabs(Core::CollectionMediaType type, int totalFiles)
{
    auto& colMgr = Core::MediaCollectionManager::Get();
    auto colNames = colMgr.GetCollectionNames(type);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

    // Tab 1: "Todas"
    bool isAllActive = m_ActiveCollection.empty();
    if (isAllActive) {
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

    std::string allLbl = "📁 Todos (" + std::to_string(totalFiles) + ")";
    if (ImGui::Button(allLbl.c_str())) {
        m_ActiveCollection = "";
        colMgr.SetActiveCollection(type, "");
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

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

        // Drag & Drop Target: Dropping item on collection tab adds it!
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MEDIA_ITEM_FILENAME")) {
                const char* droppedFilename = (const char*)payload->Data;
                colMgr.AddItemToCollection(type, name, droppedFilename);
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
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // If a custom collection is active, show quick actions (Rename / Delete)
    if (!m_ActiveCollection.empty()) {
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

void MediaGridView::RenderCardGrid(Core::CollectionMediaType type, Core::VLCBasePlayer* previewPlayer)
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
            if (!searchLower.empty()) {
                std::string lo = it.filename;
                std::transform(lo.begin(), lo.end(), lo.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
                if (lo.find(searchLower) == std::string::npos) continue;
            }
            filtered.push_back(it);
        }
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

    // Iniciar el área con scroll antes de obtener el draw list para que el recorte (clip rect)
    // pertenezca estrictamente al child y el contenido no se solape con las toolbars superiores.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f));
    ImGui::BeginChild("##mediaCardGridScroll", ImVec2(0.0f, 0.0f), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Grid Dimensions (calculadas dentro del child para considerar el scrollbar) ──
    float availW = ImGui::GetContentRegionAvail().x;
    const float minCardW = 230.0f;
    const float gap      = 12.0f;
    int   cols           = std::max(1, (int)((availW + gap) / (minCardW + gap)));
    float cardW          = std::max(minCardW, (availW - (cols - 1) * gap) / cols);
    float cardH          = cardW * 0.72f;

    int currentCol = 0;
    int itemIdx = 0;

    for (const auto& item : filtered) {
        ImGui::PushID(itemIdx);

        std::string fullPath = Library::GetMMItemFullPath(item);
        bool isLive = false;
        if (type == Core::CollectionMediaType::Audio) {
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

        // ── Card Background & Border (Estilo Fondos) ─────────────────────────
        bool hovRaw = ImGui::IsMouseHoveringRect(cardMin, cardMax);
        float tHov = UI::LPHoverLerp(ImGui::GetID(("##hov_mcard_" + item.filename).c_str()), hovRaw);
        float inset = 2.0f * tHov;
        ImVec2 p0 = { cardMin.x - inset, cardMin.y - inset };
        ImVec2 p1 = { cardMax.x + inset, cardMax.y + inset };

        bool isActiveItem = isLive || isQueueActive;
        ImVec4 borderTarget = isActiveItem ? LP::Green : (isPreview ? LP::Accent : (hovRaw ? LP::BorderHov : LP::Border));
        float borderThickness = isActiveItem ? 2.2f : (isPreview ? 1.8f : (1.0f + 0.8f * tHov));

        dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), DS::RadiusCard);
        dl->AddRect(p0, p1, LPU32(borderTarget), DS::RadiusCard, 0, borderThickness);

        // ── Thumbnail Rendering ──────────────────────────────────────────────
        float thumbH = cardH * 0.60f;
        ImVec2 thumbMin = cardMin;
        ImVec2 thumbMax = { cardMin.x + cardW, cardMin.y + thumbH };

        ImTextureID thumbTex = 0;
        if (type == Core::CollectionMediaType::Video)
            thumbTex = Library::GetMMVideoThumbnail(fullPath);
        else if (type == Core::CollectionMediaType::Image)
            thumbTex = Library::GetMMImageThumbnail(fullPath);
        else
            thumbTex = Library::GetMMAudioThumbnail(fullPath);

        dl->PushClipRect(cardMin, cardMax, true);

        if (thumbTex) {
            dl->AddImage(thumbTex, thumbMin, thumbMax, ImVec2(0, 0), ImVec2(1, 1));
        } else {
            // Placeholder background with subtle gradient
            dl->AddRectFilledMultiColor(thumbMin, thumbMax,
                                        IM_COL32(30, 34, 45, 255), IM_COL32(20, 22, 30, 255),
                                        IM_COL32(15, 17, 24, 255), IM_COL32(25, 28, 38, 255));
            float iconSz = 32.0f;
            ImVec2 iconPos = { thumbMin.x + (cardW - iconSz) * 0.5f, thumbMin.y + (thumbH - iconSz) * 0.5f };
            if (type == Core::CollectionMediaType::Video)
                Library::DrawIcon_Video(dl, iconPos, iconSz, IM_COL32(255, 255, 255, 60));
            else if (type == Core::CollectionMediaType::Image)
                Library::DrawIcon_Image(dl, iconPos, iconSz, IM_COL32(255, 255, 255, 60));
            else
                Library::DrawIcon_Audio(dl, iconPos, iconSz, IM_COL32(255, 255, 255, 60));
        }

        // Overlay badges
        // Format extension (top-left)
        std::string ext = fs::path(item.filename).extension().string();
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

        ImVec2 badgeMin = { thumbMin.x + 6.0f, thumbMin.y + 6.0f };
        ImVec2 extSz    = ImGui::CalcTextSize(ext.c_str());
        dl->AddRectFilled(badgeMin, { badgeMin.x + extSz.x + 8.0f, badgeMin.y + extSz.y + 4.0f },
                          IM_COL32(0, 0, 0, 180), 3.0f);
        dl->AddText({ badgeMin.x + 4.0f, badgeMin.y + 2.0f }, IM_COL32(220, 220, 220, 255), ext.c_str());

        // Live / Queue / Preview Status badge (top-right)
        if (isLive || isQueueActive) {
            const char* tag = isLive ? ((type == Core::CollectionMediaType::Audio) ? "▶ REPRODUCIENDO" : "● EN VIVO") : "▶ EN COLA";
            ImVec2 tagSz = ImGui::CalcTextSize(tag);
            ImVec2 tagMin = { thumbMax.x - tagSz.x - 12.0f, thumbMin.y + 6.0f };
            dl->AddRectFilled(tagMin, { tagMin.x + tagSz.x + 8.0f, tagMin.y + tagSz.y + 4.0f },
                              IM_COL32(34, 197, 94, 220), 3.0f);
            dl->AddText({ tagMin.x + 4.0f, tagMin.y + 2.0f }, IM_COL32(255, 255, 255, 255), tag);
        } else if (isPreview) {
            const char* tag = "👁 PREVIEW";
            ImVec2 tagSz = ImGui::CalcTextSize(tag);
            ImVec2 tagMin = { thumbMax.x - tagSz.x - 12.0f, thumbMin.y + 6.0f };
            dl->AddRectFilled(tagMin, { tagMin.x + tagSz.x + 8.0f, tagMin.y + tagSz.y + 4.0f },
                              IM_COL32(56, 189, 248, 200), 3.0f);
            dl->AddText({ tagMin.x + 4.0f, tagMin.y + 2.0f }, IM_COL32(255, 255, 255, 255), tag);
        }

        // ── Bottom Metadata & Buttons ────────────────────────────────────────
        float infoY = thumbMax.y + 6.0f;
        std::string dispName = item.filename;
        if (dispName.size() > 28) dispName = dispName.substr(0, 25) + "...";

        dl->AddText({ cardMin.x + 8.0f, infoY }, DS::TextPrimary, dispName.c_str());

        auto StartQueueFromHere = [&]() {
            std::vector<std::string> allFiles;
            if (!m_ActiveCollection.empty()) {
                const auto* c = colMgr.GetCollection(type, m_ActiveCollection);
                if (c) allFiles = c->itemFilenames;
            }
            if (allFiles.empty()) {
                if (type == Core::CollectionMediaType::Video) {
                    for (const auto& it : Library::GetVideoList()) allFiles.push_back(it.filename);
                } else if (type == Core::CollectionMediaType::Image) {
                    for (const auto& it : Library::GetImageList()) allFiles.push_back(it.filename);
                } else {
                    for (const auto& it : Library::GetAudioList()) allFiles.push_back(it.filename);
                }
            }
            int startIdx = 0;
            for (int q = 0; q < (int)allFiles.size(); ++q) {
                if (allFiles[q] == item.filename) { startIdx = q; break; }
            }
            colMgr.StartQueue(type, m_ActiveCollection, allFiles, startIdx);
        };

        auto StopActivePlayback = [&]() {
            if (colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type) {
                colMgr.StopQueue();
            }
            if (type == Core::CollectionMediaType::Audio) {
                if (auto* ap = UI::AudioPanel::GetActiveInstance()) ap->Stop();
            } else {
                core.StopBackgroundMedia();
            }
        };

        auto PlayItemLiveOrQueue = [&]() {
            if (type == Core::CollectionMediaType::Audio) {
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

        // Action Buttons Row at Bottom
        float btnY = cardMax.y - 30.0f;
        float btnH = 22.0f;

        bool isThisActive = isLive || isQueueActive;
        const char* pBtnLabel = "Proyectar";
        float projBtnW = 72.0f;
        ImVec4 pBtnCol = LP::GreenDim;
        ImVec4 pBtnHov = LP::Green;
        ImVec4 pBtnAct = LP::Green;

        if (isThisActive) {
            pBtnLabel = (type == Core::CollectionMediaType::Image) ? "⏹ Quitar" : "⏹ Detener";
            projBtnW = 76.0f;
            pBtnCol = LP::RedDim;
            pBtnHov = LP::Red;
            pBtnAct = LP::Red;
        } else if (type == Core::CollectionMediaType::Audio) {
            pBtnLabel = "▶ Reproducir";
            projBtnW = 82.0f;
        }

        // Button 1: Proyectar / Reproducir / Detener
        ImVec2 pBtnPos = { cardMin.x + 8.0f, btnY };
        ImGui::SetCursorScreenPos(pBtnPos);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertFloat4ToU32(pBtnCol));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertFloat4ToU32(pBtnHov));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertFloat4ToU32(pBtnAct));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button(pBtnLabel, ImVec2(projBtnW, btnH))) {
            if (isThisActive) {
                StopActivePlayback();
            } else {
                PlayItemLiveOrQueue();
            }
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
            sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video :
                        (type == Core::CollectionMediaType::Image) ? Core::ItemType::Image :
                                                                     Core::ItemType::Audio;
            core.SetSelection(sel);

            if (type == Core::CollectionMediaType::Video) {
                core.RequestPreviewLoad(fullPath, false, true);
            } else if (type == Core::CollectionMediaType::Audio) {
                core.RequestPreviewLoad(fullPath, false, false);
            }

            if (m_UIManagerRef) {
                m_UIManagerRef->SetShowMediaPreviewWindow(true);
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Button 3: More options (...)
        ImVec2 moreBtnPos = { cardMax.x - 30.0f, btnY };
        ImGui::SetCursorScreenPos(moreBtnPos);
        if (ImGui::Button("...", ImVec2(22.0f, btnH))) {
            ImGui::OpenPopup("##cardCtxPopup");
        }

        // Context Menu
        if (ImGui::BeginPopup("##cardCtxPopup")) {
            if (isThisActive) {
                if (ImGui::MenuItem("⏹ Detener Reproducción")) {
                    StopActivePlayback();
                }
            } else {
                const char* playMenuLbl = (type == Core::CollectionMediaType::Audio)
                    ? "▶ Reproducir (Iniciar Cola)" : "Proyectar a Público";
                if (ImGui::MenuItem(playMenuLbl)) {
                    PlayItemLiveOrQueue();
                }
            }
            if (ImGui::MenuItem("▶ Iniciar cola desde aquí")) {
                StartQueueFromHere();
            }
            if (ImGui::MenuItem("Ver en Vista Previa")) {
                Core::LibrarySelection sel;
                sel.title = item.filename;
                sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video :
                            (type == Core::CollectionMediaType::Image) ? Core::ItemType::Image :
                                                                         Core::ItemType::Audio;
                core.SetSelection(sel);
                if (type == Core::CollectionMediaType::Video) core.RequestPreviewLoad(fullPath, false, true);
                if (m_UIManagerRef) m_UIManagerRef->SetShowMediaPreviewWindow(true);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Mover a Colección...")) {
                auto allCols = colMgr.GetCollectionNames(type);
                for (const auto& cName : allCols) {
                    if (ImGui::MenuItem(cName.c_str())) {
                        colMgr.AddItemToCollection(type, cName, item.filename);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("+ Nueva Colección...")) {
                    m_ShowNewCollectionModal = true;
                    m_NewCollectionNameBuf[0] = '\0';
                }
                ImGui::EndMenu();
            }

            if (!m_ActiveCollection.empty()) {
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

        dl->PopClipRect();

        // ── Card Invisible Button for Hover / Click / Drag ───────────────────
        ImGui::SetCursorScreenPos(cardMin);
        std::string invBtnId = "##invCardBtn_" + std::to_string(itemIdx);
        bool cardClicked = ImGui::InvisibleButton(invBtnId.c_str(), ImVec2(cardW, cardH));
        bool cardHovered = ImGui::IsItemHovered();

        // Border highlighting on hover
        if (cardHovered && !isActiveItem && !isPreview) {
            dl->AddRect(p0, p1, LPU32(LP::BorderHov), DS::RadiusCard, 0, borderThickness);
        }

        // Click / Double Click
        if (cardClicked) {
            Core::LibrarySelection sel;
            sel.title = item.filename;
            sel.type  = (type == Core::CollectionMediaType::Video) ? Core::ItemType::Video :
                        (type == Core::CollectionMediaType::Image) ? Core::ItemType::Image :
                                                                     Core::ItemType::Audio;
            core.SetSelection(sel);

            if (type == Core::CollectionMediaType::Video) {
                core.RequestPreviewLoad(fullPath, false, true);
            } else if (type == Core::CollectionMediaType::Audio) {
                core.RequestPreviewLoad(fullPath, false, false);
            }
        }

        if (cardHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (isThisActive) {
                StopActivePlayback();
            } else {
                PlayItemLiveOrQueue();
            }
        }

        // Drag source to allow dropping into collection tabs
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MEDIA_ITEM_FILENAME", item.filename.c_str(), item.filename.size() + 1);
            ImGui::Text("Mover '%s'", item.filename.c_str());
            ImGui::EndDragDropSource();
        }

        if (cardHovered && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s\nRuta: %s\n(Doble clic para proyectar en vivo)", item.filename.c_str(), fullPath.c_str());
        }

        ImGui::PopID();
        itemIdx++;
        currentCol++;

        if (currentCol < cols) {
            ImGui::SameLine(0.0f, gap);
        } else {
            currentCol = 0;
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
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##newColName", m_NewCollectionNameBuf, sizeof(m_NewCollectionNameBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();

        if (ImGui::Button("Crear", ImVec2(100.0f, 0)) || enterPressed) {
            if (m_NewCollectionNameBuf[0] != '\0') {
                colMgr.CreateCollection(type, m_NewCollectionNameBuf);
                m_ActiveCollection = m_NewCollectionNameBuf;
                colMgr.SetActiveCollection(type, m_NewCollectionNameBuf);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, 0))) {
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

        if (ImGui::Button("Guardar", ImVec2(100.0f, 0)) || enterPressed) {
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
        if (ImGui::Button("Cancelar", ImVec2(100.0f, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace ProyecThor::UI
