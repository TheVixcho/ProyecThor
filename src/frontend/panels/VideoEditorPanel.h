#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;

// ── VideoEditorPanel ─────────────────────────────────────────────────────────
// Ventana dockeable del preset "Video" (ver Settings::WorkspaceLayoutPreset::
// Video / UIManager::BuildWorkspaceLayoutVideo) -- por ahora solo un
// placeholder ("Proximamente"), pensado para mas adelante crecer a un editor
// de video real. Ver AudioEditorPanel/ImageEditorPanel, mismo patron.
class VideoEditorPanel : public IPanel {
public:
    void Render() override;
    std::string GetName() const override { return "VideoEditor"; }

    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }

private:
    UIManager* m_UIManagerRef = nullptr;
};

} // namespace ProyecThor::UI
