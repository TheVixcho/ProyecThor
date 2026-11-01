#pragma once

#include <string>
#include <vector>
#include "core/MediaCollections.h"
#include "media/player/VLCBasePlayer.h"

namespace ProyecThor::UI {

class UIManager;

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

    UIManager*  m_UIManagerRef = nullptr;
    char        m_SearchBuffer[128] = {};
    std::string m_ActiveCollection = ""; // "" = Todas (All items)

    // Modals state
    bool        m_ShowNewCollectionModal = false;
    char        m_NewCollectionNameBuf[64] = {};

    bool        m_ShowRenameCollectionModal = false;
    std::string m_RenameTargetCol;
    char        m_RenameCollectionNameBuf[64] = {};

    std::string m_ItemToMoveFilename;
    bool        m_ShowMoveToCollectionModal = false;
};

} // namespace ProyecThor::UI
