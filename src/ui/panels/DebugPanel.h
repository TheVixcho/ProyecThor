#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include "core/CloudflareUpdater.h"

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

    // Categoría 1: Cloudflare & Actualizador
    char        m_CloudflareUrlBuffer[512] = "";
    char        m_ProbeVersionBuffer[64] = "";
    bool        m_IsProbing = false;
    ProyecThor::Core::CloudflareProbeResult m_LastProbeResult;
    bool        m_HasProbeResult = false;

    // Escaneo de múltiples versiones en Cloudflare
    bool        m_IsScanning = false;
    bool        m_HasScanResult = false;
    std::string m_ScanHighestVer;
    std::string m_ScanDownloadUrl;
    float       m_ScanSizeMB = 0.0f;
    std::vector<std::string> m_ScanAllFound;

    float       m_FeedbackTimer = 0.0f;
    std::string m_FeedbackMsg;

    // Métodos de renderizado
    void RenderSidebar();
    void RenderContent();

    void RenderCategoryLogs();
    void RenderCategoryCloudflare();
    void RenderCategoryDiagnostics();
};

} // namespace ProyecThor::UI
