#pragma once

#include <string>
#include <vector>
#include "core/MediaCollections.h"
#include "media/player/VLCBasePlayer.h"

namespace ProyecThor::UI {

class UIManager;

enum class MediaSortMode {
    NameAsc = 0,    // Nombre (A - Z)
    NameDesc = 1,   // Nombre (Z - A)
    DateNewest = 2, // Más recientes
    DateOldest = 3, // Más antiguos
    SizeDesc = 4    // Tamaño (mayor)
};

class MediaGridView {
public:
    MediaGridView();
    ~MediaGridView();

    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }
    void Render(Core::CollectionMediaType type, Core::VLCBasePlayer* previewPlayer);

private:
    void RenderHeader(Core::CollectionMediaType type, int totalItems);
    void RenderCollectionTabs(Core::CollectionMediaType type, int totalFiles);
    void RenderCardGrid(Core::CollectionMediaType type, Core::VLCBasePlayer* previewPlayer);
    void RenderModals(Core::CollectionMediaType type);

    UIManager*    m_UIManagerRef = nullptr;
    char          m_SearchBuffer[128] = {};
    std::string   m_ActiveCollection = ""; // "" = Todas (All items)
    MediaSortMode m_SortMode = MediaSortMode::NameAsc;

    // Modals state
    bool        m_ShowNewCollectionModal = false;
    char        m_NewCollectionNameBuf[64] = {};
    int         m_NewCollectionTargetRootIdx = 0;

    bool        m_ShowRenameCollectionModal = false;
    std::string m_RenameTargetCol;
    char        m_RenameCollectionNameBuf[64] = {};

    std::string m_ItemToMoveFilename;
    bool        m_ShowMoveToCollectionModal = false;

    bool        m_ShowMoveOrCopyModal = false;
    std::string m_PendingMoveOrCopyTargetCol;
    std::string m_PendingMoveOrCopyFilename;
};

} // namespace ProyecThor::UI
