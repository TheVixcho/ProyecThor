// NetworkStreamServer.cpp
//
// Dependencias:
//   - cpp-httplib  (header-only)  https://github.com/yhirose/cpp-httplib
//   - stb_image_write (header-only, para JPEG sin libJPEG externa)
//     Coloca stb_image_write.h en sdk/ y en UN .cpp define:
//       #define STB_IMAGE_WRITE_IMPLEMENTATION
//     antes del include.  Si ya usas stb en otro lugar, quita la define de aquí.

#include "NetworkStreamServer.h"
#include "httplib.h"

#include "stb_image_write.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <thread>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#   pragma comment(lib, "iphlpapi.lib")
#   pragma comment(lib, "ws2_32.lib")
#else
#   include <ifaddrs.h>
#   include <arpa/inet.h>
#   include <netinet/in.h>
#endif

namespace ProyecThor::Core {

// ── Constructor / Destructor ──────────────────────────────────────────────────
NetworkStreamServer::NetworkStreamServer()  = default;
NetworkStreamServer::~NetworkStreamServer() { Stop(); }

// ── SetSnapshotProvider / SetFrameProvider / SetFontPathProvider ─────────────
void NetworkStreamServer::SetSnapshotProvider(SnapshotProvider p)
{
    std::lock_guard<std::mutex> lk(m_ProviderMutex);
    m_SnapshotProvider = std::move(p);
}

void NetworkStreamServer::SetFrameProvider(FrameProvider p)
{
    std::lock_guard<std::mutex> lk(m_ProviderMutex);
    m_FrameProvider = std::move(p);
}

void NetworkStreamServer::SetFontPathProvider(FontPathProvider p)
{
    std::lock_guard<std::mutex> lk(m_ProviderMutex);
    m_FontPathProvider = std::move(p);
}

void NetworkStreamServer::SetChatStore(ChatMessageStore* store)
{
    std::lock_guard<std::mutex> lk(m_ProviderMutex);
    m_ChatStore = store;
}

// ── SetConfig / GetConfig ─────────────────────────────────────────────────────
void NetworkStreamServer::SetConfig(const StreamConfig& cfg)
{
    std::lock_guard<std::mutex> lk(m_ConfigMutex);
    m_Config = cfg;
}

StreamConfig NetworkStreamServer::GetConfig() const
{
    std::lock_guard<std::mutex> lk(m_ConfigMutex);
    return m_Config;
}

// ── Start ─────────────────────────────────────────────────────────────────────
bool NetworkStreamServer::Start(int port)
{
    if (m_Running.load()) return true;

    m_Port    = port;
    m_BaseURL = "http://" + DetectLocalIP() + ":" + std::to_string(port);
    std::cout << "[NetworkStream] URL generada: " << m_BaseURL << "\n";

    std::promise<bool> startedPromise;
    std::future<bool>  startedFuture = startedPromise.get_future();

    m_Running.store(true);
    m_Thread = std::thread([this, port, p = std::move(startedPromise)]() mutable {
        ServerThreadFunc(port, std::move(p));
    });

    bool ok = startedFuture.wait_for(std::chrono::seconds(3)) ==
              std::future_status::ready && startedFuture.get();

    if (!ok) {
        m_Running.store(false);
        if (m_Thread.joinable()) m_Thread.join();
        std::cerr << "[NetworkStream] No se pudo escuchar en puerto " << port << "\n";
        return false;
    }

    std::cout << "[NetworkStream] Servidor iniciado en " << m_BaseURL << "\n";
    return true;
}

// ── Stop ──────────────────────────────────────────────────────────────────────
void NetworkStreamServer::Stop()
{
    if (!m_Running.load()) return;
    m_Running.store(false);
    if (m_Thread.joinable()) m_Thread.join();
    std::cout << "[NetworkStream] Servidor detenido.\n";
}

// ── ServerThreadFunc ──────────────────────────────────────────────────────────
void NetworkStreamServer::ServerThreadFunc(int port, std::promise<bool> startedPromise)
{
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(32); };

    // ── Logger ────────────────────────────────────────────────────────────────
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[NetworkStream] " << req.method << " " << req.path
                  << " -> " << res.status
                  << " (" << req.remote_addr << ")\n";
    });

    // ── GET / ─────────────────────────────────────────────────────────────────
    svr.Get("/", [this](const httplib::Request& req, httplib::Response& res)
    {
        res.set_content(BuildHTMLPage(), "text/html; charset=utf-8");
    });

    // ── GET /chat  (HTML del chat de equipo — 404 si no hay store conectado) ──
    svr.Get("/chat", [this](const httplib::Request&, httplib::Response& res)
    {
        bool hasChat;
        { std::lock_guard<std::mutex> lk(m_ProviderMutex); hasChat = (m_ChatStore != nullptr); }
        if (!hasChat) { res.status = 404; return; }
        res.set_content(BuildChatHTMLPage(), "text/html; charset=utf-8");
    });

    // ── GET /chat/messages  (long-poll JSON) ───────────────────────────────────
    svr.Get("/chat/messages", [this](const httplib::Request& req, httplib::Response& res)
    {
        ChatMessageStore* store;
        { std::lock_guard<std::mutex> lk(m_ProviderMutex); store = m_ChatStore; }
        if (!store) { res.status = 404; return; }

        uint64_t clientSince = 0;
        if (req.has_param("since"))
            try { clientSince = std::stoull(req.get_param_value("since")); } catch (...) {}

        const int maxWaitMs      = 15000;
        const int pollIntervalMs = 200;
        int       waited         = 0;
        std::vector<ChatMessage> fresh;

        while (m_Running.load())
        {
            fresh = store->GetMessagesSince(clientSince);
            if (!fresh.empty()) break;
            if (waited >= maxWaitMs) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
            waited += pollIntervalMs;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
        res.set_content(ChatMessagesToJSON(fresh), "application/json; charset=utf-8");
    });

    // ── POST /chat/send ─────────────────────────────────────────────────────────
    svr.Post("/chat/send", [this](const httplib::Request& req, httplib::Response& res)
    {
        ChatMessageStore* store;
        { std::lock_guard<std::mutex> lk(m_ProviderMutex); store = m_ChatStore; }
        if (!store) { res.status = 404; return; }

        std::string nick = req.has_param("nickname") ? req.get_param_value("nickname") : "";
        std::string text = req.has_param("text")     ? req.get_param_value("text")     : "";
        store->PostChatMessage(nick, text);

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /font  (sirve el .ttf/.otf activo tal cual, para @font-face) ──────
    svr.Get("/font", [this](const httplib::Request& req, httplib::Response& res)
    {
        std::string path;
        {
            std::lock_guard<std::mutex> lk(m_ProviderMutex);
            if (m_FontPathProvider) path = m_FontPathProvider();
        }

        if (path.empty()) {
            res.status = 404;
            res.set_content(R"({"error":"no_custom_font"})", "application/json");
            return;
        }

        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            res.status = 404;
            res.set_content(R"({"error":"font_not_found"})", "application/json");
            return;
        }

        std::ostringstream ss;
        ss << f.rdbuf();
        std::string data = ss.str();

        std::string ext;
        auto dot = path.find_last_of('.');
        if (dot != std::string::npos) ext = path.substr(dot + 1);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string mime = (ext == "otf") ? "font/otf" : "font/ttf";

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Cache-Control", "public, max-age=3600");
        res.set_content(data, mime.c_str());
    });

    // ── GET /state  (long-poll JSON) ──────────────────────────────────────────
    svr.Get("/state", [this](const httplib::Request& req, httplib::Response& res)
    {
        uint64_t clientVersion = 0;
        if (req.has_param("since"))
            try { clientVersion = std::stoull(req.get_param_value("since")); } catch (...) {}

        StreamSnapshot snap;
        const int maxWaitMs      = 10000;
        const int pollIntervalMs = 200;
        int       waited         = 0;
        bool      changed        = false;

        while (m_Running.load())
        {
            {
                std::lock_guard<std::mutex> lk(m_ProviderMutex);
                if (m_SnapshotProvider) snap = m_SnapshotProvider();
            }
            if (snap.version != clientVersion) { changed = true; break; }
            if (waited >= maxWaitMs) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
            waited += pollIntervalMs;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
        res.set_content(SnapshotToJSON(snap), "application/json; charset=utf-8");
    });

    // ── GET /frame  (JPEG único — modo LowLatency) ────────────────────────────
    svr.Get("/frame", [this](const httplib::Request& req, httplib::Response& res)
    {
        std::vector<uint8_t> jpegData;
        {
            std::lock_guard<std::mutex> lk(m_ProviderMutex);
            if (m_FrameProvider) jpegData = m_FrameProvider();
        }

        if (jpegData.empty()) {
            // Frame vacío: devolvemos un JPEG negro 1x1
            static const uint8_t kBlack1x1[] = {
                0xFF,0xD8,0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46,0x00,0x01,
                0x01,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0xFF,0xDB,0x00,0x43,
                0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,
                0x09,0x08,0x0A,0x0C,0x14,0x0D,0x0C,0x0B,0x0B,0x0C,0x19,0x12,
                0x13,0x0F,0x14,0x1D,0x1A,0x1F,0x1E,0x1D,0x1A,0x1C,0x1C,0x20,
                0x24,0x2E,0x27,0x20,0x22,0x2C,0x23,0x1C,0x1C,0x28,0x37,0x29,
                0x2C,0x30,0x31,0x34,0x34,0x34,0x1F,0x27,0x39,0x3D,0x38,0x32,
                0x3C,0x2E,0x33,0x34,0x32,0xFF,0xC0,0x00,0x0B,0x08,0x00,0x01,
                0x00,0x01,0x01,0x01,0x11,0x00,0xFF,0xC4,0x00,0x1F,0x00,0x00,
                0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                0x09,0x0A,0x0B,0xFF,0xC4,0x00,0xB5,0x10,0x00,0x02,0x01,0x03,
                0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,
                0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,
                0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,
                0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,
                0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
                0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,
                0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
                0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,
                0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
                0x8A,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,
                0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
                0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,
                0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,0xE3,
                0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,
                0xF6,0xF7,0xF8,0xF9,0xFA,0xFF,0xDA,0x00,0x08,0x01,0x01,0x00,
                0x00,0x3F,0x00,0xFB,0xD7,0xFF,0xD9
            };
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Cache-Control", "no-cache");
            res.set_content(
                std::string(reinterpret_cast<const char*>(kBlack1x1), sizeof(kBlack1x1)),
                "image/jpeg");
            return;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Cache-Control", "no-cache, no-store");
        res.set_content(
            std::string(reinterpret_cast<const char*>(jpegData.data()), jpegData.size()),
            "image/jpeg");
    });

    // ── GET /stream  (MJPEG — modo HighQuality) ───────────────────────────────
    svr.Get("/stream", [this](const httplib::Request& req, httplib::Response& res)
{
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");

    const std::string boundary = "PTframe";

    res.set_chunked_content_provider(
        "multipart/x-mixed-replace; boundary=" + boundary,
        [this, boundary](size_t /*offset*/, httplib::DataSink& sink)
        {
            using Clock = std::chrono::steady_clock;
            auto nextFrameDeadline = Clock::now();

            while (m_Running.load())
            {
                StreamConfig cfg = GetConfig();

                // Frecuencia objetivo segun el modo activo. UltraStable
                // respeta el FPS elegido por el usuario (30 o 60); el resto
                // se queda en ~30fps como hasta ahora.
                int fps = 30;
                if (cfg.videoMode == StreamConfig::VideoMode::UltraStable)
                    fps = std::clamp(cfg.targetFPS, 24, 60);

                const auto frameInterval =
                    std::chrono::microseconds(1000000 / std::max(1, fps));

                std::vector<uint8_t> jpegData;
                {
                    std::lock_guard<std::mutex> lk(m_ProviderMutex);
                    if (m_FrameProvider) jpegData = m_FrameProvider();
                }

                if (!jpegData.empty())
                {
                    std::ostringstream hdr;
                    hdr << "--" << boundary << "\r\n"
                        << "Content-Type: image/jpeg\r\n"
                        << "Content-Length: " << jpegData.size() << "\r\n\r\n";
                    std::string hdrStr = hdr.str();

                    if (!sink.write(hdrStr.data(), hdrStr.size())) return false;
                    if (!sink.write(reinterpret_cast<const char*>(jpegData.data()),
                                    jpegData.size()))           return false;

                    std::string tail = "\r\n";
                    if (!sink.write(tail.data(), tail.size())) return false;
                }

                // Pacing anti-drift: avanzamos el deadline en pasos fijos de
                // frameInterval. Si nos atrasamos (frame de red lenta, JPEG
                // grande, etc.) NO intentamos mandar rafagas para "ponernos
                // al dia" — eso es lo que rompe la fluidez percibida. En vez
                // de eso, resincronizamos el deadline al momento actual y
                // seguimos desde ahi, priorizando timing estable sobre
                // recuperar frames perdidos.
                nextFrameDeadline += frameInterval;
                auto now = Clock::now();
                if (nextFrameDeadline > now) {
                    std::this_thread::sleep_for(nextFrameDeadline - now);
                } else {
                    nextFrameDeadline = now;
                }
            }
            return false;
        }
    );
});
    // ── Error handler ─────────────────────────────────────────────────────────
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        res.status = 404;
        res.set_content(R"({"error":"not_found"})", "application/json");
    });

    // ── Watcher ───────────────────────────────────────────────────────────────
    std::thread watcher([this, &svr]()
    {
        while (m_Running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "[NetworkStream] Watcher deteniendo servidor...\n";
        svr.stop();
    });

    // ── Bind → señalizar → listen ─────────────────────────────────────────────
    int boundPort = svr.bind_to_port("0.0.0.0", port);
    if (boundPort <= 0) {
        startedPromise.set_value(false);
        m_Running.store(false);
        if (watcher.joinable()) watcher.join();
        return;
    }

    startedPromise.set_value(true);
    std::cout << "[NetworkStream] Ejecutando listen en 0.0.0.0:" << port << "\n";
    svr.listen_after_bind();

    if (watcher.joinable()) watcher.join();
}

// ── SnapshotToJSON ────────────────────────────────────────────────────────────
std::string NetworkStreamServer::SnapshotToJSON(const StreamSnapshot& s) const
{
    StreamConfig cfg = GetConfig();

    auto escapeJSON = [](const std::string& in) -> std::string {
        std::string out; out.reserve(in.size() + 8);
        for (unsigned char c : in) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf,8,"\\u%04x",c); out+=buf; }
                    else out += static_cast<char>(c);
            }
        }
        return out;
    };
    auto B = [](bool v) -> const char* { return v ? "true" : "false"; };

  bool hiQ = (cfg.videoMode != StreamConfig::VideoMode::LowLatency);

    std::ostringstream j;
    j << "{"
      << "\"isProjecting\":"  << B(s.isProjecting)            << ","
      << "\"showText\":"      << B(s.showText && cfg.sendText)<< ","
      << "\"version\":"       << s.version                    << ","
      << "\"fontVersion\":"   << s.fontVersion                << ","
      << "\"transitionTrigger\":"  << s.transitionTrigger  << ","
  << "\"transitionType\":"     << s.transitionType     << ","
  << "\"transitionDuration\":" << s.transitionDuration << ","
      << "\"textAlignment\":" << s.textAlignment              << ","
      << "\"vAlignment\":"    << s.vAlignment                 << ","
      << "\"textSize\":"      << s.textSize                   << ","
      << "\"autoScale\":"     << B(s.autoScale)                << ","
      << "\"margins\":["      << s.margins[0] << "," << s.margins[1] << ","
                               << s.margins[2] << "," << s.margins[3] << "],"
      << "\"fontFamily\":\""  << escapeJSON(s.fontFamily)     << "\","
      << "\"refW\":"          << s.refW                       << ","
      << "\"refH\":"          << s.refH                       << ","
      << "\"isBgVideo\":"     << B(s.isBgVideo)                << ","
      << "\"hasFrame\":"      << B(s.hasFrame && cfg.sendBackground) << ","
      << "\"highQuality\":"   << B(hiQ)                        << ","
      << "\"currentText\":\"" << escapeJSON(s.currentText)     << "\","
      << "\"textColor\":["
            << s.textColor[0] << "," << s.textColor[1] << ","
            << s.textColor[2] << "," << s.textColor[3]
      << "],"
      << "\"bgColor\":["
            << s.bgColor[0] << "," << s.bgColor[1] << "," << s.bgColor[2]
      << "]"
      << "}";
    return j.str();
}

// ── ChatMessagesToJSON ────────────────────────────────────────────────────────
std::string NetworkStreamServer::ChatMessagesToJSON(const std::vector<ChatMessage>& msgs) const
{
    auto escapeJSON = [](const std::string& in) -> std::string {
        std::string out; out.reserve(in.size() + 8);
        for (unsigned char c : in) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf,8,"\\u%04x",c); out+=buf; }
                    else out += static_cast<char>(c);
            }
        }
        return out;
    };

    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < msgs.size(); i++) {
        const auto& m = msgs[i];
        if (i) j << ",";
        j << "{"
          << "\"id\":"        << m.id                  << ","
          << "\"nickname\":\""<< escapeJSON(m.nickname) << "\","
          << "\"text\":\""    << escapeJSON(m.text)     << "\","
          << "\"ts\":"        << m.timestampMs
          << "}";
    }
    j << "]";
    return j.str();
}

// ── DetectLocalIP ─────────────────────────────────────────────────────────────
std::string NetworkStreamServer::DetectLocalIP()
{
#ifdef _WIN32
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    addrinfo hints = {}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) == 0 && result) {
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, ip, sizeof(ip));
        freeaddrinfo(result);
        std::string out(ip);
        if (!out.empty() && out != "127.0.0.1") return out;
    }
#else
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
            std::string out(ip);
            if (out != "127.0.0.1" && out.substr(0,3) != "169") { freeifaddrs(ifaddr); return out; }
        }
        freeifaddrs(ifaddr);
    }
#endif
    return "127.0.0.1";
}

// ── BuildHTMLPage ─────────────────────────────────────────────────────────────
std::string NetworkStreamServer::BuildHTMLPage()
{
    return R"html(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<title>ProyecThor — Vista en Vivo</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    width: 100%; height: 100%;
    background: #000;
    overflow: hidden;
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    -webkit-font-smoothing: antialiased;
  }
  #screen {
    position: relative;
    width: 100vw; height: 100vh;
    display: flex;
    align-items: center; justify-content: center;
    background: #000;
    overflow: hidden;
  }

  /* Contenedor con las MISMAS proporciones que el proyector real.
     Su tamaño en px se calcula en JS replicando "lo justo y necesario"
     (idéntico al algoritmo de ViewPanel::RenderContent en el cliente C++). */
  #viewport {
    position: relative;
    background: #000;
    overflow: hidden;
    transition: background-color 0.4s ease;
  }

  /* Fondo — imagen/video capturado */
  #bg-frame {
    position: absolute;
    inset: 0;
    width: 100%; height: 100%;
    object-fit: cover;
    opacity: 0;
    transition: opacity 0.3s ease;
    pointer-events: none;
  }
  #bg-frame.visible { opacity: 1; }

  /* Texto overlay — posición/tamaño se fijan en px vía JS (no CSS fijo),
     para reproducir exactamente los mismos márgenes y escala que el
     proyector real, en vez de un padding fijo en vw. */
  #text-container {
    position: absolute;
    display: flex;
    opacity: 0;
    transition: opacity 0.35s ease;
    pointer-events: none;
    z-index: 10;
    overflow: hidden;
  }
  #text-container.visible { opacity: 1; }
  #text-container.h-left   { justify-content: flex-start; text-align: left; }
  #text-container.h-center { justify-content: center;     text-align: center; }
  #text-container.h-right  { justify-content: flex-end;   text-align: right; }
  #text-container.v-top    { align-items: flex-start; }
  #text-container.v-center { align-items: center; }
  #text-container.v-bottom { align-items: flex-end; }
  #main-text {
    white-space: pre-wrap;
    word-break: break-word;
    line-height: 1.25;
    text-shadow: 2px 3px 12px rgba(0,0,0,0.92), 0 0 30px rgba(0,0,0,0.6);
    max-width: 100%;
    font-family: 'UserFont', 'Segoe UI', system-ui, -apple-system, sans-serif;
  }

  /* Idle overlay */
  #idle-overlay {
    position: absolute; inset: 0;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    gap: 16px;
    opacity: 1; transition: opacity 0.4s ease;
    pointer-events: none; z-index: 20;
  }
  #idle-overlay.hidden { opacity: 0; pointer-events: none; }

  /* Status bar */
  #status-bar {
    position: fixed; bottom: 12px; right: 14px;
    font-size: 11px; color: rgba(255,255,255,0.18);
    letter-spacing: 0.05em; user-select: none; pointer-events: none;
    z-index: 100;
  }
  #status-bar .dot {
    display: inline-block; width: 7px; height: 7px;
    border-radius: 50%; background: rgba(80,80,80,0.6);
    margin-right: 5px; vertical-align: middle; transition: background 0.3s;
  }
  #status-bar.connected .dot { background: rgba(60,200,100,0.7); }
  #status-bar.error     .dot { background: rgba(220,60,60,0.7);  }
</style>
</head>
<body>
<div id="screen">
  <div id="viewport">
    <!-- Fondo capturado (JPEG polling o MJPEG) -->
    <img id="bg-frame" src="" alt="" aria-hidden="true">

    <!-- Overlay de texto -->
    <div id="text-container">
      <div id="main-text"></div>
    </div>

    <!-- Placeholder de "esperando contenido" — ahora independiente,
         no tapa el fondo ni el texto cuando hay algo proyectándose -->
    <div id="idle-overlay"></div>
  </div>
</div>

<div id="status-bar">
  <span class="dot"></span><span id="status-text">conectando…</span>
</div>

<script>
'use strict';

const screenEl      = document.getElementById('screen');
const viewportEl    = document.getElementById('viewport');
const bgFrame       = document.getElementById('bg-frame');
const textContainer = document.getElementById('text-container');
const mainText      = document.getElementById('main-text');
const idleOverlay   = document.getElementById('idle-overlay');
const statusBar     = document.getElementById('status-bar');
const statusText    = document.getElementById('status-text');

let currentVersion     = 0;
let currentFontVersion = -1;
let retryDelay         = 1000;
let highQuality        = false;
let mjpegActive        = false;
let lastRefW            = 1920;
let lastRefH             = 1080;

// ── Carga dinámica de la fuente real del usuario vía FontFace API ───────────
// Si el servidor no tiene fuente custom activa ("Predeterminada"), /font
// devuelve 404 y simplemente seguimos con el fallback sans-serif del CSS.
async function ensureFontLoaded(fontVersion) {
  if (fontVersion === currentFontVersion) return;
  currentFontVersion = fontVersion;
  try {
    const face = new FontFace('UserFont', `url(/font?v=${fontVersion})`);
    const loaded = await face.load();
    document.fonts.add(loaded);
  } catch (e) {
    // Sin fuente custom disponible: se mantiene el fallback del sistema.
  }
}

// ── Layout: replica EXACTAMENTE "lo justo y necesario" de ViewPanel ─────────
// (ver ViewPanel::RenderContent en el cliente C++: mismo cálculo de
// relación de aspecto y mismo criterio de encaje por ancho/alto).
function layoutViewport(refW, refH) {
  lastRefW = refW;
  lastRefH = refH;

  const panelW = window.innerWidth;
  const panelH = window.innerHeight;
  const srcRatio = refW / refH;

  let drawW = panelW;
  let drawH = panelW / srcRatio;
  if (drawH > panelH) {
    drawH = panelH;
    drawW = panelH * srcRatio;
  }

  viewportEl.style.width  = drawW + 'px';
  viewportEl.style.height = drawH + 'px';

  return { drawW, drawH };
}

// ── Modo HighQuality: conectar MJPEG ────────────────────────────────────────
function startMJPEG() {
  if (mjpegActive) return;
  mjpegActive = true;
  bgFrame.src = '/stream?_t=' + Date.now();
  bgFrame.classList.add('visible');
  bgFrame.onerror = () => {
    mjpegActive = false;
    bgFrame.classList.remove('visible');
    setTimeout(startMJPEG, 2000);
  };
}

// ── Modo LowLatency: polling de /frame ──────────────────────────────────────
let framePollTimer = null;
function pollFrame() {
  if (highQuality) return;
  bgFrame.onload = () => {
    if (!highQuality) framePollTimer = setTimeout(pollFrame, 150);
  };
  bgFrame.onerror = () => {
    if (!highQuality) framePollTimer = setTimeout(pollFrame, 500);
  };
  bgFrame.src = '/frame?_t=' + Date.now();
}

function startFramePoll() {
  if (framePollTimer) clearTimeout(framePollTimer);
  pollFrame();
}

// ── Aplicar estado ──────────────────────────────────────────────────────────
function applyState(s) {
  const { drawW, drawH } = layoutViewport(s.refW, s.refH);
  const scale = drawW / s.refW; // ← idéntico a "scale" en ViewPanel::RenderContent

  ensureFontLoaded(s.fontVersion);

  // Modo de video
  if (s.hasFrame) {
    if (s.highQuality && !highQuality) {
      highQuality = true;
      if (framePollTimer) { clearTimeout(framePollTimer); framePollTimer = null; }
      startMJPEG();
    } else if (!s.highQuality && highQuality) {
      highQuality = false;
      mjpegActive = false;
      bgFrame.src = '';
      startFramePoll();
    } else if (!highQuality && !framePollTimer) {
      startFramePoll();
    }
    bgFrame.classList.add('visible');
    viewportEl.style.backgroundColor = 'transparent';
  } else {
    // Sin frame: fondo de color sólido
    bgFrame.classList.remove('visible');
    if (!s.isBgVideo) {
      const r = Math.round(s.bgColor[0]*255);
      const g = Math.round(s.bgColor[1]*255);
      const b = Math.round(s.bgColor[2]*255);
      viewportEl.style.backgroundColor = `rgb(${r},${g},${b})`;
    }
  }

  // Idle / activo
  if (!s.isProjecting || !s.showText || !s.currentText) {
    textContainer.classList.remove('visible');
    if (!s.isProjecting) idleOverlay.classList.remove('hidden');
    return;
  }
  idleOverlay.classList.add('hidden');

  // ── Márgenes reales del usuario, escalados igual que en ViewPanel ────────
  const marginL = s.margins[0] * scale;
  const marginT = s.margins[1] * scale;
  const marginR = s.margins[2] * scale;
  const marginB = s.margins[3] * scale;

  textContainer.style.left   = marginL + 'px';
  textContainer.style.top    = marginT + 'px';
  textContainer.style.width  = Math.max(10, drawW - marginL - marginR) + 'px';
  textContainer.style.height = Math.max(10, drawH - marginT - marginB) + 'px';

  // ── Tamaño de texto real, escalado — sin clamp/vw artificial ─────────────
  mainText.style.fontSize = (s.textSize * scale) + 'px';

  // Color y opacidad
  const r = Math.round(s.textColor[0]*255);
  const g = Math.round(s.textColor[1]*255);
  const b = Math.round(s.textColor[2]*255);
  const a = s.textColor[3].toFixed(3);
  mainText.style.color = `rgba(${r},${g},${b},${a})`;

  // Alineación horizontal
  textContainer.classList.remove('h-left','h-center','h-right');
  if      (s.textAlignment===0) textContainer.classList.add('h-left');
  else if (s.textAlignment===1) textContainer.classList.add('h-center');
  else                           textContainer.classList.add('h-right');

  // Alineación vertical
  textContainer.classList.remove('v-top','v-center','v-bottom');
  if      (s.vAlignment===0) textContainer.classList.add('v-top');
  else if (s.vAlignment===1) textContainer.classList.add('v-center');
  else                        textContainer.classList.add('v-bottom');

  mainText.textContent = s.currentText;
  textContainer.classList.add('visible');
}

// ── Long-poll /state ─────────────────────────────────────────────────────────
function poll() {
  const url = `/state?since=${currentVersion}&_t=${Date.now()}`;
  fetch(url, { cache: 'no-store' })
    .then(res => { if (!res.ok) throw new Error('HTTP '+res.status); return res.json(); })
    .then(data => {
      retryDelay = 1000;
      statusBar.className = 'connected';
      statusText.textContent = 'en vivo';
      if (data.version !== currentVersion) {
        currentVersion = data.version;
        applyState(data);
      }
      setTimeout(poll, 0);
    })
    .catch(err => {
      statusBar.className = 'error';
      statusText.textContent = 'reconectando…';
      retryDelay = Math.min(retryDelay*1.5, 10000);
      setTimeout(poll, retryDelay);
    });
}

// ── Reajustar el layout si la ventana/orientación del dispositivo cambia ────
window.addEventListener('resize', () => layoutViewport(lastRefW, lastRefH));

// ── Wake lock ────────────────────────────────────────────────────────────────
async function requestWakeLock() {
  if ('wakeLock' in navigator) {
    try { await navigator.wakeLock.request('screen'); } catch(_) {}
  }
}

requestWakeLock();
poll();
</script>
</body>
</html>
)html";
}

// ── BuildChatHTMLPage ─────────────────────────────────────────────────────────
std::string NetworkStreamServer::BuildChatHTMLPage()
{
    return R"html(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<title>ProyecThor — Chat de equipo</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    width: 100%; height: 100%;
    background: #0a0c12;
    color: #eef2f5;
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    -webkit-font-smoothing: antialiased;
    overscroll-behavior: none;
  }
  #nick-screen, #chat-screen {
    position: fixed; inset: 0;
    display: flex; flex-direction: column;
  }
  #nick-screen {
    align-items: center; justify-content: center;
    gap: 18px; padding: 24px;
    text-align: center;
  }
  #nick-screen h1 { font-size: 1.3rem; font-weight: 600; }
  #nick-screen p  { color: #93a4ac; font-size: 0.9rem; max-width: 30ch; }
  #nick-input {
    width: 100%; max-width: 280px;
    padding: 12px 14px;
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.15);
    background: rgba(255,255,255,0.05);
    color: #eef2f5;
    font-size: 1rem;
  }
  #nick-input:focus { outline: none; border-color: #b177e6; }
  #nick-go {
    width: 100%; max-width: 280px;
    padding: 12px 14px;
    border-radius: 10px;
    border: none;
    background: linear-gradient(135deg, #7a5cff, #b177e6);
    color: #fff; font-size: 1rem; font-weight: 600;
    cursor: pointer;
  }
  #nick-go:disabled { opacity: 0.4; }

  #chat-screen { display: none; }
  #chat-header {
    flex-shrink: 0;
    padding: 14px 16px;
    border-bottom: 1px solid rgba(255,255,255,0.08);
    display: flex; align-items: center; justify-content: space-between;
  }
  #chat-header .title { font-weight: 600; font-size: 0.95rem; }
  #chat-header .sub   { color: #93a4ac; font-size: 0.78rem; }

  #chat-log {
    flex: 1;
    overflow-y: auto;
    padding: 14px 12px;
    display: flex; flex-direction: column; gap: 10px;
    -webkit-overflow-scrolling: touch;
  }
  .msg { max-width: 82%; }
  .msg .meta {
    font-size: 0.72rem; color: #93a4ac;
    margin-bottom: 2px;
  }
  .msg .meta .nick { color: #c9a8ff; font-weight: 600; }
  .msg .bubble {
    background: rgba(255,255,255,0.06);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 12px;
    padding: 8px 12px;
    font-size: 0.92rem;
    line-height: 1.35;
    word-break: break-word;
    white-space: pre-wrap;
  }
  .msg.mine { align-self: flex-end; }
  .msg.mine .bubble { background: rgba(122,92,255,0.28); border-color: rgba(122,92,255,0.4); }
  .msg.mine .meta { text-align: right; }

  #chat-form {
    flex-shrink: 0;
    display: flex; gap: 8px;
    padding: 10px 12px;
    border-top: 1px solid rgba(255,255,255,0.08);
    background: #0a0c12;
  }
  #chat-text {
    flex: 1;
    padding: 11px 14px;
    border-radius: 20px;
    border: 1px solid rgba(255,255,255,0.15);
    background: rgba(255,255,255,0.05);
    color: #eef2f5;
    font-size: 0.95rem;
  }
  #chat-text:focus { outline: none; border-color: #b177e6; }
  #chat-send {
    padding: 0 18px;
    border-radius: 20px;
    border: none;
    background: linear-gradient(135deg, #7a5cff, #b177e6);
    color: #fff; font-weight: 600;
    cursor: pointer;
  }
  #chat-send:disabled { opacity: 0.4; }
</style>
</head>
<body>

<div id="nick-screen">
  <h1>Chat de equipo — ProyecThor</h1>
  <p>Poné un nombre para identificarte en el chat. Solo lo ven los dispositivos conectados a esta red.</p>
  <input id="nick-input" maxlength="24" placeholder="Tu nombre" autocomplete="off">
  <button id="nick-go" disabled>Entrar al chat</button>
</div>

<div id="chat-screen">
  <div id="chat-header">
    <div>
      <div class="title">Chat de equipo</div>
      <div class="sub" id="chat-sub"></div>
    </div>
  </div>
  <div id="chat-log"></div>
  <form id="chat-form">
    <input id="chat-text" placeholder="Escribí un mensaje…" autocomplete="off" maxlength="500">
    <button id="chat-send" type="submit">Enviar</button>
  </form>
</div>

<script>
(function () {
  var nickScreen = document.getElementById('nick-screen');
  var chatScreen = document.getElementById('chat-screen');
  var nickInput  = document.getElementById('nick-input');
  var nickGo     = document.getElementById('nick-go');
  var chatSub    = document.getElementById('chat-sub');
  var chatLog    = document.getElementById('chat-log');
  var chatForm   = document.getElementById('chat-form');
  var chatText   = document.getElementById('chat-text');
  var chatSend   = document.getElementById('chat-send');

  var nickname   = localStorage.getItem('pt_chat_nick') || '';
  var sinceId    = 0;
  var polling    = false;

  function escapeHTML(s) {
    var d = document.createElement('div');
    d.innerText = s;
    return d.innerHTML;
  }

  function fmtTime(ts) {
    var d = new Date(ts);
    var h = d.getHours().toString().padStart(2, '0');
    var m = d.getMinutes().toString().padStart(2, '0');
    return h + ':' + m;
  }

  function appendMessage(m) {
    var mine = m.nickname === nickname;
    var wrap = document.createElement('div');
    wrap.className = 'msg' + (mine ? ' mine' : '');
    wrap.innerHTML =
      '<div class="meta"><span class="nick">' + escapeHTML(m.nickname) +
      '</span> · ' + fmtTime(m.ts) + '</div>' +
      '<div class="bubble">' + escapeHTML(m.text) + '</div>';
    chatLog.appendChild(wrap);
    sinceId = Math.max(sinceId, m.id);
  }

  function scrollToBottom() {
    chatLog.scrollTop = chatLog.scrollHeight;
  }

  function poll() {
    if (polling) return;
    polling = true;
    fetch('/chat/messages?since=' + sinceId)
      .then(function (r) { return r.json(); })
      .then(function (msgs) {
        var wasAtBottom = chatLog.scrollTop + chatLog.clientHeight >= chatLog.scrollHeight - 40;
        msgs.forEach(appendMessage);
        if (msgs.length && wasAtBottom) scrollToBottom();
      })
      .catch(function () {})
      .finally(function () {
        polling = false;
        setTimeout(poll, 300);
      });
  }

  function enterChat(nick) {
    nickname = nick;
    localStorage.setItem('pt_chat_nick', nick);
    nickScreen.style.display = 'none';
    chatScreen.style.display = 'flex';
    chatSub.textContent = 'Conectado como ' + nick;
    chatText.focus();
    poll();
  }

  nickInput.addEventListener('input', function () {
    nickGo.disabled = nickInput.value.trim().length === 0;
  });
  nickInput.addEventListener('keydown', function (e) {
    if (e.key === 'Enter' && nickInput.value.trim().length > 0) nickGo.click();
  });
  nickGo.addEventListener('click', function () {
    var v = nickInput.value.trim();
    if (v) enterChat(v);
  });

  chatForm.addEventListener('submit', function (e) {
    e.preventDefault();
    var text = chatText.value.trim();
    if (!text) return;
    chatSend.disabled = true;
    fetch('/chat/send', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'nickname=' + encodeURIComponent(nickname) + '&text=' + encodeURIComponent(text)
    })
      .catch(function () {})
      .finally(function () { chatSend.disabled = false; });
    chatText.value = '';
  });

  if (nickname) {
    nickInput.value = nickname;
    enterChat(nickname);
  }
})();
</script>
</body>
</html>
)html";
}

} // namespace ProyecThor::Core