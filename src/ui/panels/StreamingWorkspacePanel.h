#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;
class BroadcastPanel;

// ── StreamingWorkspacePanel ──────────────────────────────────────────────────
// Ventana dockeable con el contenido de Ajustes > Conexiones > Streaming
// (Captura/Capa/Iniciar, ver BroadcastPanel) pero embebido directo en el
// workspace, en vez de enterrado en Ajustes -- pedido explicito para el
// preset "Transmisión" (ver Settings::WorkspaceLayoutPreset::Broadcast /
// UIManager::BuildWorkspaceLayoutBroadcast), que reemplaza a "Vista en Vivo"
// ahi. No tiene estado propio: renderiza las mismas 3 secciones del MISMO
// BroadcastPanel que ya usa Ajustes, asi que activar/mirar la transmision
// desde cualquiera de los dos lados queda sincronizado solo.
class StreamingWorkspacePanel : public IPanel {
public:
    explicit StreamingWorkspacePanel(BroadcastPanel* broadcastRef) : m_BroadcastRef(broadcastRef) {}

    void Render() override;
    std::string GetName() const override { return "Transmisión"; }

    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }

private:
    BroadcastPanel* m_BroadcastRef  = nullptr;
    UIManager*      m_UIManagerRef  = nullptr;
};

} // namespace ProyecThor::UI
