#pragma once
#include "AIWebViewPanel.h"
#include "frontend/ui/GlassRenderer.h"
#include "backend/core/ClaudeClient.h"
#include <optional>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  AIAssistantPanel — ventana flotante "Asistente IA", con dos modos:
//
//   - Basica: primero pregunta que IA usar (Claude/ChatGPT/Gemini) y despues
//     muestra el navegador embebido (AIWebViewPanel) apuntando al sitio de
//     esa IA, donde el usuario inicia sesion como en cualquier navegador
//     normal. Si WebView2 no esta disponible (Linux, o Windows sin el
//     Runtime instalado) cae a abrir el navegador externo del sistema.
//
//   - Avanzada: el usuario pone su propia API key de Claude (Ajustes >
//     Asistente de IA / Settings::AISettings) y la IA puede revisar, crear y
//     editar canciones de la Biblioteca de forma directa (ver
//     backend/core/AITools.h), con confirmacion explicita antes de cualquier
//     accion que escriba algo en disco (create_song/edit_song).
// ─────────────────────────────────────────────────────────────────────────────
class AIAssistantPanel {
public:
    ~AIAssistantPanel();

    // Llamar SIEMPRE, una vez por frame, sin importar si *pShow es true o
    // false -- cuando esta oculto igual hace falta avisarle al WebView2
    // embebido que se esconda (ver AIWebViewPanel::UpdateBounds). El cierre
    // nativo (boton X) escribe directo en *pShow, igual que Notas.
    void Render(bool* pShow, GlassRenderer& glass);

private:
    enum class Provider { None, Claude, ChatGPT, Gemini };
    enum class Mode      { Basic, Advanced };

    void RenderModeTabs();
    void RenderProviderPicker();
    void RenderBrowserArea();
    void SelectProvider(Provider p);

    Provider       m_Provider = Provider::None;
    AIWebViewPanel m_WebView;
    Mode           m_Mode = Mode::Basic;

    // ── Modo Avanzada (chat con tool-use, ver AITools.h) ─────────────────
    struct ChatLine { bool fromUser; std::string text; };

    void RenderAdvancedMode();
    void RenderApiKeySetup();
    void RenderChat();
    void StartTurn();
    void ProcessBatch();
    void ApproveOrRejectPending(bool approve);

    char m_ApiKeyBuf[256] = {};
    bool m_ApiKeyBufInit  = false;

    std::vector<ChatLine>                m_ChatLog;
    std::vector<Core::ClaudeMessage>     m_History;
    char                                 m_InputBuf[4096] = {};

    std::thread                          m_Worker;
    std::mutex                           m_WorkerMutex;
    bool                                 m_WorkerBusy = false;
    std::optional<Core::ClaudeResponse>  m_WorkerResult;   // protegido por m_WorkerMutex

    // Cola de tool_use del turno en curso -- se procesan en orden; las de
    // solo lectura se auto-ejecutan, las que escriben algo pausan en
    // m_HasPendingConfirm hasta que el operador aprueba/rechaza (ver
    // ProcessBatch/ApproveOrRejectPending).
    std::vector<Core::ClaudeToolCall>    m_PendingBatch;
    size_t                               m_PendingBatchIdx = 0;
    nlohmann::json                       m_PendingBatchResults = nlohmann::json::array();

    bool                  m_HasPendingConfirm = false;
    Core::ClaudeToolCall  m_PendingConfirmCall;
};

} // namespace ProyecThor::UI
