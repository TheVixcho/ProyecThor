#pragma once

#include "ui/framework/GlassRenderer.h"
#include "core/ProAIManager.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

class ProAIChatPanel {
public:
    ProAIChatPanel() = default;
    ~ProAIChatPanel() = default;

    void Render(bool* pOpen, GlassRenderer& glass);

private:
    std::vector<ProyecThor::Core::ProChatMessage> m_Messages;
    char m_InputBuf[1024] = "";
    bool m_ScrollToBottom = false;
    std::string m_StatusFeedback = "";
};

} // namespace ProyecThor::UI
