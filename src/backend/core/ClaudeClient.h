#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  ClaudeClient — cliente minimo para la Anthropic Messages API
//  (https://api.anthropic.com/v1/messages). Sincrono/bloqueante a proposito:
//  se llama desde un hilo de trabajo dedicado (ver AIAssistantPanel), nunca
//  desde el hilo de render/UI. Requiere CPPHTTPLIB_OPENSSL_SUPPORT (ver
//  CMakeLists.txt) -- sin OpenSSL disponible en el build, SendMessage()
//  devuelve ok=false con un error explicando por que.
// ─────────────────────────────────────────────────────────────────────────────

// Un pedido de "tool use" del modelo -- el asistente decidio que hace falta
// ejecutar una accion (ver AITools.h) para responder. NO se ejecuta solo:
// el llamador decide si mostrarlo al usuario para confirmar antes de
// correrlo de verdad (ver AIAssistantPanel).
struct ClaudeToolCall {
    std::string    id;
    std::string    name;
    nlohmann::json input;
};

struct ClaudeResponse {
    bool                        ok = false;
    std::string                 error;
    std::string                 text;        // texto de la respuesta (puede ser vacio si solo hay tool_use)
    std::vector<ClaudeToolCall> toolCalls;
    std::string                 stopReason;  // "end_turn", "tool_use", "max_tokens", etc.

    // El array "content" tal cual vino en la respuesta -- se reinyecta sin
    // modificar como mensaje "assistant" en el historial de la proxima
    // llamada (la API lo exige asi para mantener el hilo de tool_use).
    nlohmann::json assistantContent;
};

// Un mensaje ya armado para el array "messages" de la API.
struct ClaudeMessage {
    std::string    role;    // "user" | "assistant"
    nlohmann::json content; // string simple (texto) o array de bloques (tool_use/tool_result)
};

struct ClaudeToolDef {
    std::string    name;
    std::string    description;
    nlohmann::json inputSchema; // JSON Schema del objeto "input" que el modelo debe mandar
};

ClaudeMessage MakeUserTextMessage(const std::string& text);
ClaudeMessage MakeAssistantMessage(const nlohmann::json& content);
ClaudeMessage MakeToolResultMessage(const std::string& toolUseId, const std::string& resultText, bool isError);

class ClaudeClient {
public:
    // Nombrado "SendRequest" y no "SendMessage" a proposito: windows.h
    // define SendMessage como macro (SendMessageA/W, la API real de Win32
    // para mandarle mensajes a un HWND) -- un metodo con ese nombre exacto
    // se reescribe en tiempo de preprocesado y el linker tira "no
    // declaration matches ...::SendMessageA(...)".
    ClaudeResponse SendRequest(const std::string& apiKey, const std::string& model,
                                const std::string& systemPrompt,
                                const std::vector<ClaudeMessage>& history,
                                const std::vector<ClaudeToolDef>& tools);
};

} // namespace ProyecThor::Core
