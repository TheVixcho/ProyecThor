#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;

// ── ImageEditorPanel ─────────────────────────────────────────────────────────
// Ventana dockeable del preset "Imagen" (ver Settings::WorkspaceLayoutPreset::
// Image / UIManager::BuildWorkspaceLayoutImage) -- por ahora solo un
// placeholder ("Proximamente"), pensado para mas adelante crecer a un editor
// de imagen real. Ver AudioEditorPanel/VideoEditorPanel, mismo patron.
class ImageEditorPanel : public IPanel {
public:
    void Render() override;
    std::string GetName() const override { return "ImageEditor"; }

    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }

private:
    UIManager* m_UIManagerRef = nullptr;
};

} // namespace ProyecThor::UI
