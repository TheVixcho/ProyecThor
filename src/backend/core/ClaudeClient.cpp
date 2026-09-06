#include "ClaudeClient.h"

// httplib::Client con esquema "https://..." usa SSLClient por debajo
// automaticamente cuando CPPHTTPLIB_OPENSSL_SUPPORT esta definido (ver
// CMakeLists.txt) -- si no lo esta, el include igual compila (SSLClient
// queda excluido a nivel de #ifdef adentro del propio httplib.h) pero
// Post() a un host https:// falla en runtime, por eso SendMessage() chequea
// el resultado en vez de asumir que la conexion sirvio.
#include "httplib.h"

namespace ProyecThor::Core {

using json = nlohmann::json;

ClaudeMessage MakeUserTextMessage(const std::string& text) {
    return ClaudeMessage{ "user", json(text) };
}

ClaudeMessage MakeAssistantMessage(const json& content) {
    return ClaudeMessage{ "assistant", content };
}

ClaudeMessage MakeToolResultMessage(const std::string& toolUseId, const std::string& resultText, bool isError) {
    json block = {
        { "type", "tool_result" },
        { "tool_use_id", toolUseId },
        { "content", resultText }
    };
    if (isError) block["is_error"] = true;
    return ClaudeMessage{ "user", json::array({ block }) };
}

ClaudeResponse ClaudeClient::SendRequest(const std::string& apiKey, const std::string& model,
                                          const std::string& systemPrompt,
                                          const std::vector<ClaudeMessage>& history,
                                          const std::vector<ClaudeToolDef>& tools) {
    ClaudeResponse out;

    if (apiKey.empty()) {
        out.error = "Falta la API key de Claude (Ajustes > Asistente de IA).";
        return out;
    }
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
    out.error = "Este build no tiene soporte HTTPS (falta OpenSSL al compilar) -- el asistente no puede conectarse.";
    return out;
#endif

    json body;
    body["model"]      = model.empty() ? "claude-sonnet-5" : model;
    body["max_tokens"] = 4096;
    if (!systemPrompt.empty()) body["system"] = systemPrompt;

    json messages = json::array();
    for (const auto& m : history)
        messages.push_back({ { "role", m.role }, { "content", m.content } });
    body["messages"] = messages;

    if (!tools.empty()) {
        json toolsJson = json::array();
        for (const auto& t : tools) {
            toolsJson.push_back({
                { "name", t.name },
                { "description", t.description },
                { "input_schema", t.inputSchema }
            });
        }
        body["tools"] = toolsJson;
    }

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(20, 0);
    cli.set_read_timeout(90, 0);
    cli.set_write_timeout(20, 0);

    httplib::Headers headers = {
        { "x-api-key", apiKey },
        { "anthropic-version", "2023-06-01" },
    };

    auto res = cli.Post("/v1/messages", headers, body.dump(), "application/json");
    if (!res) {
        out.error = "No se pudo conectar con api.anthropic.com (revisa tu conexion a internet).";
        return out;
    }

    if (res->status != 200) {
        std::string apiMsg;
        try {
            json errJson = json::parse(res->body);
            if (errJson.contains("error") && errJson["error"].contains("message"))
                apiMsg = errJson["error"]["message"].get<std::string>();
        } catch (...) {}
        out.error = "Error de la API (" + std::to_string(res->status) + "): " +
                    (apiMsg.empty() ? res->body : apiMsg);
        return out;
    }

    json resJson;
    try {
        resJson = json::parse(res->body);
    } catch (...) {
        out.error = "Respuesta invalida de la API (no era JSON valido).";
        return out;
    }

    out.assistantContent = resJson.value("content", json::array());
    out.stopReason        = resJson.value("stop_reason", "");

    for (const auto& block : out.assistantContent) {
        std::string type = block.value("type", "");
        if (type == "text") {
            out.text += block.value("text", "");
        } else if (type == "tool_use") {
            ClaudeToolCall call;
            call.id    = block.value("id", "");
            call.name  = block.value("name", "");
            call.input = block.value("input", json::object());
            out.toolCalls.push_back(call);
        }
    }

    out.ok = true;
    return out;
}

} // namespace ProyecThor::Core
