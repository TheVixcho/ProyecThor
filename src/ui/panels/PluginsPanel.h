#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <imgui.h>

namespace ProyecThor::UI { class UIManager; }
namespace ProyecThor::Core { struct PluginInfo; }

namespace ProyecThor::UI {

class PluginsPanel {
public:
    PluginsPanel();
    ~PluginsPanel();

    void SetUIManager(UIManager* uiManager) { m_UIManager = uiManager; }
    void Render();

    // Invocar actualización o refresco desde el exterior
    void RefreshPlugins();

private:
    void RenderSidebar(float sidebarW, float contentH);
    void RenderPluginDetails(float contentW, float contentH);
    void RenderYtDlpSpecificUI();
    void RenderGenericPluginUI();
    void RenderTerminalConsole(float w, float h);

    UIManager* m_UIManager = nullptr;
    std::string m_SelectedPluginId;

    // Búsqueda y filtros
    char m_SearchFilter[128] = "";

    // Estado para yt-dlp plugin específico
    char m_YtDlpStreamUrlInput[512] = "";
    char m_YtDlpLyricsUrlInput[512] = "";
    std::string m_ResolvedStreamUrl;
    std::string m_DownloadedLyricsTitle;
    std::string m_DownloadedLyricsContent;
    std::string m_YtDlpStatusText;
    bool m_YtDlpStatusChecked = false;
    bool m_YtDlpInstalled = false;

    // Parámetros para plugins genéricos
    char m_GenericArgsInput[512] = "";

    // Consola
    bool m_AutoScrollConsole = true;

    // Mensaje de estado temporal
    std::string m_FeedbackMessage;
    float m_FeedbackTimer = 0.0f;
};

} // namespace ProyecThor::UI
