#pragma once
#include "AIWebViewPanel.h"
#include "frontend/ui/GlassRenderer.h"
#include <string>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  AIAssistantPanel — ventana flotante "Asistente IA":
//  Permite chatear con Claude, ChatGPT o Gemini directamente mediante el
//  navegador embebido (AIWebViewPanel) con cookies y sesión propia, o
//  navegador externo en sistemas sin WebView2.
// ─────────────────────────────────────────────────────────────────────────────
class AIAssistantPanel {
public:
    ~AIAssistantPanel();

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
