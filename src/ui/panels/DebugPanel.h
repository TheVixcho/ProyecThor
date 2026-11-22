#pragma once

#include <imgui.h>
#include <string>
#include <vector>

namespace ProyecThor::UI {

class DebugPanel {
public:
    DebugPanel();
    void Render(bool* isOpen);
    void SetCategory(int idx) { m_SelectedCategory = idx; }
    int  GetSelectedCategory() const { return m_SelectedCategory; }

private:
    int         m_SelectedCategory = 0;
    bool        m_WasOpenLastFrame = false;

    // Categoría 0: Logs de la App
    char        m_LogSearchBuffer[128] = "";
    int         m_LogLevelFilter = -1; // -1 = ALL, 0 = Debug, 1 = Info, 2 = Warning, 3 = Error
    bool        m_LogAutoScroll = true;

    float       m_FeedbackTimer = 0.0f;
    std::string m_FeedbackMsg;

    void RenderSidebar();
    void RenderContent();

    void RenderCategoryLogs();
    void RenderCategoryDiagnostics();
};

} // namespace ProyecThor::UI
