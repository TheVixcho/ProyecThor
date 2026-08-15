#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;

// ── AudioEditorPanel ─────────────────────────────────────────────────────────
// Ventana dockeable del preset "Audio" (ver Settings::WorkspaceLayoutPreset::
// Audio / UIManager::BuildWorkspaceLayoutAudio) -- por ahora solo un
// placeholder ("Proximamente"), pensado para mas adelante crecer a un editor
// de audio real (recortar/normalizar pistas de la Biblioteca, etc).
class AudioEditorPanel : public IPanel {
public:
    void Render() override;
    std::string GetName() const override { return "AudioEditor"; }

    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }

private:
    UIManager* m_UIManagerRef = nullptr;
};

} // namespace ProyecThor::UI
