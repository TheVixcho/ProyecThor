#pragma once
#include "AIWebViewPanel.h"
#include <string>
#include <vector>
#include <cstdint>

namespace ProyecThor::UI {

struct VisitedWebItem {
    std::string url;
    std::string title;
    std::string visitedAt;
    bool        isFavorite = false;
};

struct ImportedHtmlItem {
    std::string id;
    std::string title;
    std::string filePath;
    std::string importedAt;
    uintmax_t   fileSize = 0;
    bool        isFavorite = false;
};

class WebBrowserPanel {
public:
    WebBrowserPanel();
    ~WebBrowserPanel() = default;

    void Render();
    void Hide();

private:
    void Go(const std::string& customUrl = "");
    void NavigateToPathOrUrl(const std::string& pathOrUrl);
    void StartSendToPublic();
    void StopSendToPublic();
    void UpdateSendToPublicBounds();

    void LoadLibraryFromDisk();
    void SaveLibraryToDisk();

    void ImportHtmlDialog();
    bool ImportHtmlFile(const std::string& srcFilePath);
    void DeleteImportedHtml(size_t index);
    void DeleteVisitedWeb(size_t index);
    void ClearHistory();

    void RenderHeader(float availW);
    void RenderNavigationControls(float availW);
    void RenderActionButtons(float availW);
    void RenderViewModeBar(float availW);
    void RenderWebArea(float availW, float h);
    void RenderLibrarySection(float availW, float h);

    char           m_UrlBuf[1024] = "https://";
    char           m_SearchFilter[128] = "";
    bool           m_Navigated   = false;
    AIWebViewPanel m_WebView;
    bool           m_SendingToPublic = false;

    bool           m_FullWebView = false;
    int            m_ActiveTab = 0;

    std::vector<VisitedWebItem>   m_VisitedWebs;
    std::vector<ImportedHtmlItem> m_ImportedHtmls;

    std::string    m_CurrentLoadedTitle;
};

} // namespace ProyecThor::UI


