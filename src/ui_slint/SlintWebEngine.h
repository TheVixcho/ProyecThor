#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ProyecThor::SlintUI {

class SlintWebEngine {
public:
    // Callback invoked from worker thread whenever a decoded RGBA8 viewport frame is ready.
    using FrameCallback  = std::function<void(std::vector<uint8_t>&& rgba, int width, int height)>;
    using StatusCallback = std::function<void(const std::string& url, const std::string& title, const std::string& statusMsg, bool loading)>;

    SlintWebEngine() = default;
    ~SlintWebEngine();

    SlintWebEngine(const SlintWebEngine&) = delete;
    SlintWebEngine& operator=(const SlintWebEngine&) = delete;

    void SetProfileDir(const std::string& dir);
    void SetViewportSize(int width, int height);
    void SetFrameCallback(FrameCallback cb);
    void SetStatusCallback(StatusCallback cb);

    bool StartIfNeeded(std::string& outError);
    void Stop();

    bool Navigate(const std::string& urlOrFilePath);
    void Reload();
    void GoBack();
    void GoForward();
    void ScrollBy(int deltaX, int deltaY);
    void ClickNormalized(float normX, float normY);
    void SendNavKey(const std::string& keyName);
    void SetZoomFactor(double zoom);
    void CaptureSnapshotNow();

    bool IsRunning() const { return m_Connected.load(std::memory_order_acquire); }
    std::string GetBrowserName() const;
    std::string GetCurrentUrl() const;
    std::string GetCurrentTitle() const;

    static std::string NormalizeInputToUrl(const std::string& input);
    static std::string LocalFileToFileUrl(const std::string& utf8Path);

private:
    bool FindAndLaunchBrowser(int port, std::string& outError);
    bool FetchPageWebSocketUrl(int port, std::string& outWsPath);
    bool ConnectWebSocket(int port, const std::string& wsPath);
    void CloseConnection();
    void TerminateBrowserProcess();

    bool SendWsTextFrame(const std::string& text);
    bool SendCdpCommand(const std::string& method, const std::string& paramsJson = "{}");
    void ReaderLoop();
    void HandleCdpMessage(const std::string& jsonText);
    void DecodeAndEmitImageBytes(const std::string& b64Data);

    std::string m_ProfileDir;
    std::string m_BrowserPath;
    std::string m_BrowserLabel;
    int         m_ViewportW = 1920;
    int         m_ViewportH = 1080;
    double      m_ZoomFactor = 1.0;

    mutable std::mutex m_StateMutex;
    std::string m_CurrentUrl;
    std::string m_CurrentTitle;
    std::string m_PendingUrl;

    std::mutex     m_CbMutex;
    FrameCallback  m_OnFrame;
    StatusCallback m_OnStatus;

    std::mutex     m_SendMutex;
    std::atomic<bool> m_StopRequested{false};
    std::atomic<bool> m_Connected{false};
    std::atomic<int>  m_NextCmdId{1};
    std::atomic<int64_t> m_LastFrameEmitMs{0};

    intptr_t   m_Socket = -1;
    std::thread m_ReaderThread;

#ifdef _WIN32
    void* m_ProcessHandle = nullptr;
#else
    int   m_ChildPid = -1;
#endif
};

} // namespace ProyecThor::SlintUI
