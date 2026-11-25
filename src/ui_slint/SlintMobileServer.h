#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <slint.h>

namespace ProyecThor::SlintUI {

// Generates a crisp black-on-white QR code image for any URL or pairing URI.
slint::Image GenerateQrImage(const std::string& text, int scale = 6, int border = 2);

// Detects local LAN IPv4 address (e.g. 192.168.x.x).
std::string DetectLanIPv4();

// Lightweight HTTP + UDP Discovery server for ProyecThor Mobile (port 8090 + UDP 47990).
class SlintMobileServer {
public:
    struct RemoteCallbacks {
        std::function<void()> onNextSlide;
        std::function<void()> onPrevSlide;
        std::function<void()> onClearText;
        std::function<void()> onClearAll;
        std::function<void()> onToggleAudience;
        std::function<std::string()> getLiveStateJson;
    };

    SlintMobileServer();
    ~SlintMobileServer();

    void SetRootDir(const std::string& dir);
    void SetPairingPin(const std::string& pin);
    std::string GetPairingPin() const;
    void SetCallbacks(RemoteCallbacks cb);

    bool Start(int port = 8090);
    void Stop();

    bool        IsRunning() const { return m_Running.load(); }
    int         GetPort()   const { return m_Port; }
    std::string GetBaseURL() const { return m_BaseURL; }
    std::string GetPairingURI() const;

private:
    void HttpThreadFunc(int port);
    void DiscoveryThreadFunc();

    std::atomic<bool> m_Running{false};
    int               m_Port = 8090;
    std::string       m_BaseURL;

    mutable std::mutex m_Mutex;
    std::string        m_RootDir;
    std::string        m_Pin;
    RemoteCallbacks    m_Callbacks;

    std::thread m_HttpThread;
    std::thread m_DiscoveryThread;
    void*       m_ServerPtr = nullptr; // httplib::Server*
};

} // namespace ProyecThor::SlintUI
