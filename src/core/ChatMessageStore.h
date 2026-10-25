#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace ProyecThor::Core {

// ── ChatMessage ────────────────────────────────────────────────────────────
struct ChatMessage {
    uint64_t    id          = 0;
    std::string nickname;
    std::string text;
    int64_t     timestampMs = 0;
};

// ── ChatMessageStore ───────────────────────────────────────────────────────
// Feed de chat de equipo en memoria — SIN puerto ni hilo propio. Las rutas
// HTTP (/chat, /chat/messages, /chat/send) viven en NetworkStreamServer
// (ver NetworkStreamServer::SetChatStore): el chat reusa el MISMO
// servidor/puerto que ya usa la Transmision en Red, en vez de abrir uno
// propio. Asi nunca hace falta un permiso de firewall nuevo para el chat —
// si el operador ya tiene la transmision LAN andando (o la deja arrancar
// el chat primero), ambas features comparten el mismo puerto ya permitido.
class ChatMessageStore {
public:
    void PostChatMessage(const std::string& nickname, const std::string& text);

    // Mensajes con id > sinceId, en orden cronologico.
    std::vector<ChatMessage> GetMessagesSince(uint64_t sinceId) const;
    uint64_t                 GetLatestId() const;

private:
    mutable std::mutex      m_Mutex;
    std::deque<ChatMessage> m_Messages;      // tope kMaxMessages, se descartan los mas viejos
    uint64_t                m_NextId { 1 };
};

} // namespace ProyecThor::Core
