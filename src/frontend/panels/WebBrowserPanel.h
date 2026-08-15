#pragma once
#include "AIWebViewPanel.h"
#include <string>

namespace ProyecThor::UI {

// ── WebBrowserPanel ─────────────────────────────────────────────────────────
// Navegador embebido generico (Biblioteca > Web, ver LibrarySideMode::Web) --
// barra de direccion + navegador real (AIWebViewPanel) + "Enviar a Público",
// que reparenta esa MISMA ventana nativa directo sobre la salida real al
// publico (ver PresentationCore::GetProjectorNativeWindow), asi el operador
// puede mostrar cualquier pagina web en pantalla, no solo contenido de la
// Biblioteca -- ej. una letra que solo esta en un sitio, una transmision
// embebida de otra plataforma, etc.
class WebBrowserPanel {
public:
    // Asume que ya hay una ventana/child abierta (se llama desde
    // LibraryPanel::Render() cuando LibrarySideMode::Web esta activo).
    void Render();

private:
    void Go();
    void StartSendToPublic();
    void StopSendToPublic();
    void UpdateSendToPublicBounds();

    char           m_UrlBuf[512] = "https://";
    bool           m_Navigated   = false;
    AIWebViewPanel m_WebView;
    bool           m_SendingToPublic = false;
};

} // namespace ProyecThor::UI
