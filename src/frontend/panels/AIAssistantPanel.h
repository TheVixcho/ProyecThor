#pragma once
#include "AIWebViewPanel.h"
#include "frontend/ui/GlassRenderer.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  AIAssistantPanel — ventana flotante "Asistente IA": primero pregunta que IA
//  usar (Claude/ChatGPT/Gemini) y despues muestra el navegador embebido
//  (AIWebViewPanel) apuntando al sitio de esa IA, donde el usuario inicia
//  sesion como en cualquier navegador normal. Si WebView2 no esta disponible
//  (Linux, o Windows sin el Runtime instalado) cae a abrir el navegador
//  externo del sistema en su lugar (ver OpenURL.cpp).
// ─────────────────────────────────────────────────────────────────────────────
class AIAssistantPanel {
public:
    // Llamar SIEMPRE, una vez por frame, sin importar si *pShow es true o
    // false -- cuando esta oculto igual hace falta avisarle al WebView2
    // embebido que se esconda (ver AIWebViewPanel::UpdateBounds). El cierre
    // nativo (boton X) escribe directo en *pShow, igual que Notas.
    void Render(bool* pShow, GlassRenderer& glass);

private:
    enum class Provider { None, Claude, ChatGPT, Gemini };

    void RenderProviderPicker();
    void RenderBrowserArea();
    void SelectProvider(Provider p);

    Provider       m_Provider = Provider::None;
    AIWebViewPanel m_WebView;
};

} // namespace ProyecThor::UI
