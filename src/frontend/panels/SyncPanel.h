#pragma once
#include "backend/core/SyncServer.h"
#include <string>

namespace ProyecThor::UI {

// ── SyncPanel ──────────────────────────────────────────────────────────────
// Controles para el SyncServer (ver backend/core/SyncServer.h): habilitar/
// deshabilitar, elegir puerto, ver/regenerar el PIN de emparejamiento que la
// app movil necesita para conectarse. Vive dentro del rail de Yggdrasil (ver
// YggdrasilPanel::Section::Sync), mismo criterio que Red/Chat/Broadcast --
// no es un IPanel propio.
//
// A diferencia de Red/Chat (que comparten UN NetworkStreamServer via
// PresentationCore, ver StreamingPanel/TeamChatPanel), Sync es un servidor
// completamente independiente (otro puerto, sin proveedores de frame/estado
// que dependan del ciclo de render) -- por eso esta clase es dueña directa
// de su propia instancia de SyncServer, sin pasar por PresentationCore.
class SyncPanel {
public:
    SyncPanel();
    ~SyncPanel();

    // Llamar una vez por frame: si SyncSettings::enabled quedo guardado como
    // true de una sesion anterior, levanta el servidor solo en el primer
    // frame (mismo criterio de "auto-arranque" que otros paneles de Home).
    void Update();

    void RenderContent();

    std::string GetName() const { return "Sincronización"; }

private:
    void RenderServerControl();
    void RenderPairingSection();
    static std::string GenerateRandomPin();

    Core::SyncServer m_Server;
    int               m_Port = 8090;
    bool              m_AutoStartTried = false;
};

} // namespace ProyecThor::UI
