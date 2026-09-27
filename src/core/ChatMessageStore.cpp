#include "ChatMessageStore.h"
#include <chrono>

namespace ProyecThor::Core {

namespace {
    constexpr size_t kMaxMessages     = 300;   // se descartan los mas viejos al superar esto
    constexpr size_t kMaxNicknameLen  = 24;
    constexpr size_t kMaxTextLen      = 500;

    std::string Trim(const std::string& s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    // Corta por bytes: suficiente aca (evita mensajes gigantes rompiendo el
    // layout del log), no nos preocupamos por cortar un caracter UTF-8 a la
    // mitad — mismo criterio "simple y suficiente" que el resto del server.
    std::string ClampLen(const std::string& s, size_t maxLen)
    {
        return s.size() > maxLen ? s.substr(0, maxLen) : s;
    }
}

void ChatMessageStore::PostChatMessage(const std::string& nickname, const std::string& text)
{
    std::string nick = Trim(nickname);
    std::string body = Trim(text);
    if (nick.empty()) nick = "Anonimo";
    if (body.empty()) return;

    nick = ClampLen(nick, kMaxNicknameLen);
    body = ClampLen(body, kMaxTextLen);

    ChatMessage msg;
    msg.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lk(m_Mutex);
    msg.id       = m_NextId++;
    msg.nickname = std::move(nick);
    msg.text     = std::move(body);
    m_Messages.push_back(std::move(msg));
    while (m_Messages.size() > kMaxMessages)
        m_Messages.pop_front();
}

std::vector<ChatMessage> ChatMessageStore::GetMessagesSince(uint64_t sinceId) const
{
    std::vector<ChatMessage> out;
    std::lock_guard<std::mutex> lk(m_Mutex);
    for (const auto& m : m_Messages)
        if (m.id > sinceId) out.push_back(m);
    return out;
}

uint64_t ChatMessageStore::GetLatestId() const
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    return m_Messages.empty() ? 0 : m_Messages.back().id;
}

} // namespace ProyecThor::Core
