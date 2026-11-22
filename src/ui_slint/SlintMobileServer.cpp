#include "SlintMobileServer.h"

#include "qrcodegen.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::SlintUI {

slint::Image GenerateQrImage(const std::string& text, int scale, int border) {
    using qrcodegen::QrCode;
    if (text.empty()) return {};

    QrCode qr = QrCode::encodeText(text.c_str(), QrCode::Ecc::MEDIUM);
    int size = qr.getSize();
    int totalModules = size + border * 2;
    int pxSize = totalModules * scale;

    slint::SharedPixelBuffer<slint::Rgba8Pixel> buf(
        static_cast<uint32_t>(pxSize), static_cast<uint32_t>(pxSize));
    auto* pixels = buf.begin();

    for (int y = 0; y < pxSize; ++y) {
        int my = (y / scale) - border;
        for (int x = 0; x < pxSize; ++x) {
            int mx = (x / scale) - border;
            bool dark = (mx >= 0 && mx < size && my >= 0 && my < size) && qr.getModule(mx, my);
            uint8_t c = dark ? 14 : 250;
            pixels[y * pxSize + x] = { c, c, c, 255 };
        }
    }
    return slint::Image(buf);
}

std::string DetectLanIPv4() {
#ifdef _WIN32
    char host[256] = {};
    if (gethostname(host, sizeof(host)) == 0) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res) {
            for (addrinfo* p = res; p; p = p->ai_next) {
                auto* sin = reinterpret_cast<sockaddr_in*>(p->ai_addr);
                char ip[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
                if (std::strncmp(ip, "127.", 4) != 0) {
                    freeaddrinfo(res);
                    return ip;
                }
            }
            freeaddrinfo(res);
        }
    }
    return "127.0.0.1";
#else
    ifaddrs* ifaddr = nullptr;
    std::string fallback = "127.0.0.1";
    if (getifaddrs(&ifaddr) == 0 && ifaddr) {
        for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
            if (std::strncmp(ip, "127.", 4) == 0) continue;
            if (std::strncmp(ip, "192.168.", 8) == 0 || std::strncmp(ip, "10.", 3) == 0) {
                freeifaddrs(ifaddr);
                return ip;
            }
            fallback = ip;
        }
        freeifaddrs(ifaddr);
    }
    return fallback;
#endif
}

SlintMobileServer::SlintMobileServer() = default;

SlintMobileServer::~SlintMobileServer() {
    Stop();
}

void SlintMobileServer::SetRootDir(const std::string& dir) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_RootDir = dir;
}

void SlintMobileServer::SetPairingPin(const std::string& pin) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Pin = pin;
}

std::string SlintMobileServer::GetPairingPin() const {
    std::lock_guard<std::mutex> lk(m_Mutex);
    return m_Pin;
}

void SlintMobileServer::SetCallbacks(RemoteCallbacks cb) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Callbacks = std::move(cb);
}

std::string SlintMobileServer::GetPairingURI() const {
    std::string ip = DetectLanIPv4();
    return "proyecthor://sync?host=" + ip + "&port=" + std::to_string(m_Port) + "&pin=" + GetPairingPin();
}

bool SlintMobileServer::Start(int port) {
    if (m_Running.load()) return true;
    m_Port = port;
    m_BaseURL = "http://" + DetectLanIPv4() + ":" + std::to_string(port);
    m_Running.store(true);

    m_HttpThread      = std::thread(&SlintMobileServer::HttpThreadFunc, this, port);
    m_DiscoveryThread = std::thread(&SlintMobileServer::DiscoveryThreadFunc, this);
    return true;
}

void SlintMobileServer::Stop() {
    if (!m_Running.exchange(false)) return;

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        if (m_ServerPtr) {
            static_cast<httplib::Server*>(m_ServerPtr)->stop();
        }
    }
    if (m_HttpThread.joinable())      m_HttpThread.join();
    if (m_DiscoveryThread.joinable()) m_DiscoveryThread.join();
}

void SlintMobileServer::HttpThreadFunc(int port) {
    httplib::Server svr;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_ServerPtr = &svr;
    }

    auto checkToken = [this](const httplib::Request& req, httplib::Response& res) -> bool {
        std::string expected = GetPairingPin();
        std::string provided = req.get_header_value("X-Sync-Token");
        if (provided.empty() && req.has_param("pin")) {
            provided = req.get_param_value("pin");
        }
        if (!expected.empty() && provided != expected) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid pin\"}", "application/json");
            return false;
        }
        return true;
    };

    // Mobile web remote landing page (works directly from any phone browser!)
    svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        std::string html = R"HTML(<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ProyecThor Remote</title>
<style>
body{background:#09090b;color:#f4f4f5;font-family:system-ui,sans-serif;margin:0;padding:18px;text-align:center}
h2{margin:6px 0 4px;font-size:20px}p{color:#a1a1aa;font-size:13px;margin:0 0 16px}
.box{background:#141417;border:1px solid #27272a;border-radius:14px;padding:16px;margin-bottom:14px}
.row{display:flex;gap:10px;margin-top:10px}
button{flex:1;padding:15px 10px;border-radius:12px;border:1px solid #3f3f46;background:#202024;color:#f4f4f5;font-size:15px;font-weight:700;cursor:pointer}
button.pri{background:#d4d4d8;color:#09090b;border-color:#f4f4f5}
button.red{background:#3f1515;color:#fca5a5;border-color:#ef4444}
input{width:140px;padding:9px;border-radius:8px;border:1px solid #3f3f46;background:#0c0c0e;color:#f4f4f5;text-align:center;font-size:16px}
#live{font-size:18px;font-weight:700;min-height:60px;white-space:pre-wrap}
</style></head><body>
<h2>ProyecThor Mobile Remote</h2>
<p>Control remoto LAN en vivo</p>
<div class="box"><input id="pin" placeholder="PIN 6 dígitos"><div id="live" style="margin-top:12px">Conectando...</div></div>
<div class="row"><button onclick="act('/remote/prev')">◀ Anterior</button><button class="pri" onclick="act('/remote/next')">Siguiente ▶</button></div>
<div class="row"><button onclick="act('/remote/audience')">Público ON/OFF</button><button class="red" onclick="act('/remote/clear')">Limpiar Texto</button></div>
<script>
const p=new URLSearchParams(location.search).get('pin'); if(p) document.getElementById('pin').value=p;
function hdr(){return {'X-Sync-Token':document.getElementById('pin').value.trim()};}
async function act(u){await fetch(u,{method:'POST',headers:hdr()});poll();}
async function poll(){try{const r=await fetch('/remote/state',{headers:hdr()});if(r.ok){const j=await r.json();document.getElementById('live').textContent=j.text||'(Sin texto en vivo)';}}catch(e){}}
setInterval(poll,900);poll();
</script></body></html>)HTML";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/sync/hello", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        nlohmann::json j = {
            {"app", "ProyecThor"},
            {"ui", "Slint"},
            {"status", "ok"}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/sync/manifest", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::string root;
        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            root = m_RootDir;
        }
        nlohmann::json arr = nlohmann::json::array();
        std::error_code ec;
        if (!root.empty() && fs::exists(root, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                auto rel = fs::relative(entry.path(), root, ec).generic_string();
                arr.push_back({
                    {"path", rel},
                    {"size", entry.file_size(ec)}
                });
            }
        }
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get("/remote/state", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::function<std::string()> fn;
        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            fn = m_Callbacks.getLiveStateJson;
        }
        res.set_content(fn ? fn() : "{}", "application/json");
    });

    svr.Post("/remote/next", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::function<void()> fn;
        { std::lock_guard<std::mutex> lk(m_Mutex); fn = m_Callbacks.onNextSlide; }
        if (fn) fn();
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/remote/prev", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::function<void()> fn;
        { std::lock_guard<std::mutex> lk(m_Mutex); fn = m_Callbacks.onPrevSlide; }
        if (fn) fn();
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/remote/clear", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::function<void()> fn;
        { std::lock_guard<std::mutex> lk(m_Mutex); fn = m_Callbacks.onClearText; }
        if (fn) fn();
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/remote/audience", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::function<void()> fn;
        { std::lock_guard<std::mutex> lk(m_Mutex); fn = m_Callbacks.onToggleAudience; }
        if (fn) fn();
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.listen("0.0.0.0", port);

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_ServerPtr = nullptr;
    }
    m_Running.store(false);
}

void SlintMobileServer::DiscoveryThreadFunc() {
#ifndef _WIN32
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(47990);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        char buf[256];
        while (m_Running.load()) {
            sockaddr_in from{};
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                 reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (n > 0) {
                buf[n] = '\0';
                if (std::strstr(buf, "PROYECTHOR_DISCOVER") != nullptr) {
                    nlohmann::json reply = {
                        {"app", "ProyecThor"},
                        {"port", m_Port},
                        {"host", DetectLanIPv4()}
                    };
                    std::string s = reply.dump();
                    sendto(sock, s.data(), s.size(), 0,
                           reinterpret_cast<sockaddr*>(&from), fromLen);
                }
            }
        }
    }
    close(sock);
#endif
}

} // namespace ProyecThor::SlintUI
