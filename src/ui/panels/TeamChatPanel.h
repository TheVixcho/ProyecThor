#pragma once
#include "core/ChatMessageStore.h"
#include <string>
#include <vector>
#include <cstdint>
#include <imgui.h>

namespace ProyecThor::UI {

// Contenido del popup "Chat" de Vista en Vivo (ver ViewPanel::RenderChatPopup)
// y de Yggdrasil — mismo rol que StreamingPanel: no es un IPanel propio.
//
// El operador puede escribir desde aca mismo (PostChatMessage directo al
// ChatMessageStore, sin pasar por HTTP) mientras cualquier dispositivo de
// la LAN se suma escaneando el QR / abriendo la URL (.../chat) y poniendo
// un nickname. El chat vive en el MISMO servidor/puerto que Transmision en
// Red (ver ChatMessageStore.h) — arrancar cualquiera de los dos alcanza.
class TeamChatPanel {
public:
    TeamChatPanel()  = default;
    ~TeamChatPanel() = default;

    // Llamar UNA VEZ POR FRAME sin importar que seccion este activa, para
    // que el log siga recibiendo mensajes aunque el operador este mirando
    // otra pestana — mismo criterio que StreamingPanel::Update().
    void Update();

    void RenderContent();

    std::string GetName() const { return "Chat"; }

private:
    void RenderServerControl();
    void RenderURLSection();              // tarjeta fija de QR/URL, fuera del scroll del log
    void RenderChatLog(float height);     // scroll propio, solo mensajes
    void RenderMessages();
    void RenderComposer();

    void RebuildQRTexture(const std::string& url);
    void DrawQR(ImDrawList* dl, ImVec2 origin, float size);

    // Solo se usa si NINGUN server esta corriendo todavia y el chat es el
    // primero en arrancarlo — si Streaming ya esta activo, el chat se suma
    // a ESE puerto (ver TeamChatPanel::RenderServerControl).
    int  m_Port = 8080;

    char m_NicknameBuf[32] = "Operador";
    char m_MessageBuf[512] = "";

    uint64_t                          m_LastSeenId = 0;
    std::vector<Core::ChatMessage>    m_CachedMessages;

    // QR
    std::string           m_QRCachedURL;
    std::vector<uint8_t>  m_QRModules;
    int                   m_QRSize = 0;
};

} // namespace ProyecThor::UI
