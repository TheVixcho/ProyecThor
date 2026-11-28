#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "SlintWebEngine.h"
#include "stb_image.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace ProyecThor::SlintUI {

namespace {

int64_t NowSteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSock = INVALID_SOCKET;

void EnsureWinSockInit() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
    });
}

void CloseNativeSocket(NativeSocket s) {
    if (s != INVALID_SOCKET) closesocket(s);
}

std::wstring Utf8ToWide(const std::string& u8) {
    if (u8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), w.data(), len);
    return w;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string u8(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), u8.data(), len, nullptr, nullptr);
    return u8;
}

fs::path Utf8ToFs(const std::string& u8) {
    return fs::path(Utf8ToWide(u8));
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSock = -1;

void CloseNativeSocket(NativeSocket s) {
    if (s >= 0) close(s);
}

fs::path Utf8ToFs(const std::string& u8) {
    return fs::path(u8);
}
#endif

int FindFreeLocalPort() {
#ifdef _WIN32
    EnsureWinSockInit();
#endif
    NativeSocket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kInvalidSock) return 9229;
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;
    int port = 9229;
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            port = ntohs(addr.sin_port);
        }
    }
    CloseNativeSocket(s);
    return port > 0 ? port : 9229;
}

NativeSocket ConnectLoopback(int port, int timeoutMs = 1500) {
#ifdef _WIN32
    EnsureWinSockInit();
#endif
    NativeSocket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kInvalidSock) return kInvalidSock;

#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        CloseNativeSocket(s);
        return kInvalidSock;
    }
    return s;
}

bool SendAll(NativeSocket s, const void* data, size_t len) {
    const auto* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {
        int n = static_cast<int>(send(s, ptr + sent, static_cast<int>(len - sent), 0));
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool RecvExact(NativeSocket s, void* data, size_t len) {
    auto* ptr = static_cast<char*>(data);
    size_t got = 0;
    while (got < len) {
        int n = static_cast<int>(recv(s, ptr + got, static_cast<int>(len - got), 0));
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

std::vector<uint8_t> DecodeBase64(const std::string& in) {
    static const int kDec[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::vector<uint8_t> out;
    out.reserve((in.size() * 3) / 4);
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (kDec[c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + kDec[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string PercentEncodePath(const std::string& p) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(p.size() * 2);
    for (unsigned char c : p) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '/' || c == ':' ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0xF]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

#ifndef _WIN32
std::string FindBinaryInPath(const std::string& name) {
    const char* envPath = std::getenv("PATH");
    if (!envPath) return {};
    std::istringstream iss(envPath);
    std::string dir;
    while (std::getline(iss, dir, ':')) {
        if (dir.empty()) continue;
        std::string full = dir + "/" + name;
        if (access(full.c_str(), X_OK) == 0) {
            return full;
        }
    }
    return {};
}
#endif

} // namespace

SlintWebEngine::~SlintWebEngine() {
    Stop();
}

void SlintWebEngine::SetProfileDir(const std::string& dir) {
    m_ProfileDir = dir;
}

void SlintWebEngine::SetViewportSize(int width, int height) {
    m_ViewportW = std::clamp(width, 640, 3840);
    m_ViewportH = std::clamp(height, 360, 2160);
    if (IsRunning()) {
        nlohmann::json params = {
            {"width", m_ViewportW},
            {"height", m_ViewportH},
            {"deviceScaleFactor", 1},
            {"mobile", false}
        };
        SendCdpCommand("Emulation.setDeviceMetricsOverride", params.dump());
        nlohmann::json sc = {
            {"format", "jpeg"},
            {"quality", 84},
            {"maxWidth", m_ViewportW},
            {"maxHeight", m_ViewportH},
            {"everyNthFrame", 1}
        };
        SendCdpCommand("Page.startScreencast", sc.dump());
        CaptureSnapshotNow();
    }
}

void SlintWebEngine::SetFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(m_CbMutex);
    m_OnFrame = std::move(cb);
}

void SlintWebEngine::SetStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lk(m_CbMutex);
    m_OnStatus = std::move(cb);
}

std::string SlintWebEngine::GetBrowserName() const {
    return m_BrowserLabel;
}

std::string SlintWebEngine::GetCurrentUrl() const {
    std::lock_guard<std::mutex> lk(m_StateMutex);
    return m_CurrentUrl;
}

std::string SlintWebEngine::GetCurrentTitle() const {
    std::lock_guard<std::mutex> lk(m_StateMutex);
    return m_CurrentTitle;
}

std::string SlintWebEngine::LocalFileToFileUrl(const std::string& utf8Path) {
    std::string p = utf8Path;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (p.rfind("file://", 0) == 0) return p;
    if (!p.empty() && p.front() == '/') {
        return "file://" + PercentEncodePath(p);
    }
    return "file:///" + PercentEncodePath(p);
}

std::string SlintWebEngine::NormalizeInputToUrl(const std::string& rawInput) {
    std::string s = rawInput;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s.empty()) return "";

    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0 ||
        lower.rfind("file://", 0) == 0 || lower.rfind("about:", 0) == 0 ||
        lower.rfind("data:", 0) == 0) {
        return s;
    }

    // Check if it looks like a local filesystem path
    std::error_code ec;
    fs::path maybePath = Utf8ToFs(s);
    if (fs::exists(maybePath, ec) && fs::is_regular_file(maybePath, ec)) {
        return LocalFileToFileUrl(s);
    }

    if (lower.rfind("localhost", 0) == 0 || lower.rfind("127.0.0.1", 0) == 0) {
        return "http://" + s;
    }
    return "https://" + s;
}

bool SlintWebEngine::FindAndLaunchBrowser(int port, std::string& outError) {
    struct Candidate {
        std::string path;
        std::string label;
    };
    std::vector<Candidate> candidates;

    if (const char* envBrowser = std::getenv("PROYECTHOR_WEB_BROWSER")) {
        if (*envBrowser) candidates.push_back({envBrowser, "Navegador personalizado"});
    }

#ifdef _WIN32
    const char* localApp = std::getenv("LOCALAPPDATA");
    std::string localStr = localApp ? localApp : "";
    candidates.push_back({"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe", "Microsoft Edge"});
    candidates.push_back({"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe", "Microsoft Edge"});
    if (!localStr.empty()) {
        candidates.push_back({localStr + "\\Microsoft\\Edge\\Application\\msedge.exe", "Microsoft Edge"});
    }
    candidates.push_back({"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", "Google Chrome"});
    candidates.push_back({"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe", "Google Chrome"});
    if (!localStr.empty()) {
        candidates.push_back({localStr + "\\Google\\Chrome\\Application\\chrome.exe", "Google Chrome"});
    }
    candidates.push_back({"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe", "Brave"});
    candidates.push_back({"C:\\Program Files\\Chromium\\Application\\chrome.exe", "Chromium"});
#elif defined(__APPLE__)
    candidates.push_back({"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", "Google Chrome"});
    candidates.push_back({"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge", "Microsoft Edge"});
    candidates.push_back({"/Applications/Brave Browser.app/Contents/MacOS/Brave Browser", "Brave"});
    candidates.push_back({"/Applications/Chromium.app/Contents/MacOS/Chromium", "Chromium"});
#else
    for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
             {"chromium", "Chromium"},
             {"chromium-browser", "Chromium"},
             {"google-chrome", "Google Chrome"},
             {"google-chrome-stable", "Google Chrome"},
             {"microsoft-edge", "Microsoft Edge"},
             {"microsoft-edge-stable", "Microsoft Edge"},
             {"brave-browser", "Brave"},
             {"brave", "Brave"}}) {
        std::string p = FindBinaryInPath(pair.first);
        if (!p.empty()) candidates.push_back({p, pair.second});
    }
    candidates.push_back({"/usr/bin/chromium", "Chromium"});
    candidates.push_back({"/usr/bin/google-chrome", "Google Chrome"});
    candidates.push_back({"/snap/bin/chromium", "Chromium"});
#endif

    m_BrowserPath.clear();
    m_BrowserLabel.clear();
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(Utf8ToFs(c.path), ec) && fs::is_regular_file(Utf8ToFs(c.path), ec)) {
            m_BrowserPath  = c.path;
            m_BrowserLabel = c.label;
            break;
        }
    }

    if (m_BrowserPath.empty()) {
        outError = "No se encontró Edge/Chrome/Chromium instalado para renderizar el Navegador.";
        return false;
    }

    std::error_code ec;
    fs::path profDir = m_ProfileDir.empty()
        ? (fs::temp_directory_path(ec) / "proyecthor_web_profile")
        : Utf8ToFs(m_ProfileDir);
    fs::create_directories(profDir, ec);
    // Limpiar locks huérfanos de sesiones previas
    fs::remove(profDir / "SingletonLock", ec);
    fs::remove(profDir / "SingletonCookie", ec);
    fs::remove(profDir / "SingletonSocket", ec);

#ifdef _WIN32
    std::ostringstream cmd;
    cmd << "\"" << m_BrowserPath << "\""
        << " --headless=new"
        << " --remote-debugging-port=" << port
        << " --remote-allow-origins=*"
        << " --user-data-dir=\"" << WideToUtf8(profDir.wstring()) << "\""
        << " --no-first-run"
        << " --no-default-browser-check"
        << " --disable-background-networking"
        << " --disable-extensions"
        << " --disable-gpu-sandbox"
        << " --autoplay-policy=no-user-gesture-required"
        << " --allow-file-access-from-files"
        << " --window-size=" << m_ViewportW << "," << m_ViewportH
        << " about:blank";

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring wCmd = Utf8ToWide(cmd.str());
    if (!CreateProcessW(nullptr, wCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        outError = "No se pudo iniciar " + m_BrowserLabel + " en modo headless.";
        return false;
    }
    CloseHandle(pi.hThread);
    m_ProcessHandle = pi.hProcess;
    return true;
#else
    std::string profStr = profDir.string();
    std::string portArg = "--remote-debugging-port=" + std::to_string(port);
    std::string profArg = "--user-data-dir=" + profStr;
    std::string sizeArg = "--window-size=" + std::to_string(m_ViewportW) + "," + std::to_string(m_ViewportH);

    pid_t pid = fork();
    if (pid < 0) {
        outError = "Error al crear proceso para " + m_BrowserLabel + ".";
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        std::freopen("/dev/null", "r", stdin);
        std::freopen("/dev/null", "w", stdout);
        std::freopen("/dev/null", "w", stderr);
        const char* argv[] = {
            m_BrowserPath.c_str(),
            "--headless=new",
            portArg.c_str(),
            "--remote-allow-origins=*",
            profArg.c_str(),
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-background-networking",
            "--disable-extensions",
            "--disable-gpu-sandbox",
            "--autoplay-policy=no-user-gesture-required",
            "--allow-file-access-from-files",
            sizeArg.c_str(),
            "about:blank",
            nullptr
        };
        execv(m_BrowserPath.c_str(), const_cast<char* const*>(argv));
        _exit(127);
    }
    m_ChildPid = pid;
    return true;
#endif
}

bool SlintWebEngine::FetchPageWebSocketUrl(int port, std::string& outWsPath) {
    NativeSocket s = ConnectLoopback(port, 800);
    if (s == kInvalidSock) return false;

    std::ostringstream req;
    req << "GET /json/list HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port << "\r\n"
        << "Connection: close\r\n\r\n";
    std::string reqStr = req.str();
    if (!SendAll(s, reqStr.data(), reqStr.size())) {
        CloseNativeSocket(s);
        return false;
    }

    std::string resp;
    char buf[4096];
    int n = 0;
    while ((n = static_cast<int>(recv(s, buf, sizeof(buf), 0))) > 0) {
        resp.append(buf, static_cast<size_t>(n));
    }
    CloseNativeSocket(s);

    size_t hdrEnd = resp.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return false;
    std::string body = resp.substr(hdrEnd + 4);

    auto j = nlohmann::json::parse(body, nullptr, false);
    if (!j.is_array()) return false;
    for (const auto& item : j) {
        if (item.value("type", "") == "page" && item.contains("webSocketDebuggerUrl")) {
            std::string wsUrl = item.value("webSocketDebuggerUrl", "");
            size_t slashPos = wsUrl.find("/devtools/");
            if (slashPos != std::string::npos) {
                outWsPath = wsUrl.substr(slashPos);
                return true;
            }
        }
    }
    return false;
}

bool SlintWebEngine::ConnectWebSocket(int port, const std::string& wsPath) {
    NativeSocket s = ConnectLoopback(port, 2000);
    if (s == kInvalidSock) return false;

    std::ostringstream req;
    req << "GET " << wsPath << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port << "\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        << "Sec-WebSocket-Version: 13\r\n\r\n";
    std::string reqStr = req.str();
    if (!SendAll(s, reqStr.data(), reqStr.size())) {
        CloseNativeSocket(s);
        return false;
    }

    std::string hdr;
    char ch = 0;
    while (hdr.size() < 8192) {
        int n = static_cast<int>(recv(s, &ch, 1, 0));
        if (n <= 0) break;
        hdr.push_back(ch);
        if (hdr.size() >= 4 && hdr.compare(hdr.size() - 4, 4, "\r\n\r\n") == 0) {
            break;
        }
    }
    if (hdr.find("101") == std::string::npos) {
        CloseNativeSocket(s);
        return false;
    }

    // Clear short recv timeout on the streaming WebSocket so idle pages don't timeout ReaderLoop
#ifdef _WIN32
    DWORD tv = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    m_Socket = static_cast<intptr_t>(s);
    m_Connected.store(true, std::memory_order_release);
    return true;
}

bool SlintWebEngine::StartIfNeeded(std::string& outError) {
    if (IsRunning()) return true;

    Stop();
    m_StopRequested.store(false, std::memory_order_release);

    const int port = FindFreeLocalPort();
    if (!FindAndLaunchBrowser(port, outError)) {
        return false;
    }

    std::string wsPath;
    for (int attempt = 0; attempt < 45; ++attempt) {
        if (FetchPageWebSocketUrl(port, wsPath)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (wsPath.empty()) {
        outError = m_BrowserLabel + " no respondió por DevTools CDP en el puerto local.";
        TerminateBrowserProcess();
        return false;
    }

    if (!ConnectWebSocket(port, wsPath)) {
        outError = "No se pudo abrir la sesión WebSocket con " + m_BrowserLabel + ".";
        TerminateBrowserProcess();
        return false;
    }

    m_ReaderThread = std::thread(&SlintWebEngine::ReaderLoop, this);

    SendCdpCommand("Page.enable");
    SendCdpCommand("Runtime.enable");
    nlohmann::json metrics = {
        {"width", m_ViewportW},
        {"height", m_ViewportH},
        {"deviceScaleFactor", 1},
        {"mobile", false}
    };
    SendCdpCommand("Emulation.setDeviceMetricsOverride", metrics.dump());
    nlohmann::json sc = {
        {"format", "jpeg"},
        {"quality", 84},
        {"maxWidth", m_ViewportW},
        {"maxHeight", m_ViewportH},
        {"everyNthFrame", 1}
    };
    SendCdpCommand("Page.startScreencast", sc.dump());
    return true;
}

void SlintWebEngine::CloseConnection() {
    m_Connected.store(false, std::memory_order_release);
    NativeSocket s = static_cast<NativeSocket>(m_Socket);
    m_Socket = -1;
    if (s != kInvalidSock) {
#ifdef _WIN32
        shutdown(s, SD_BOTH);
#else
        shutdown(s, SHUT_RDWR);
#endif
        CloseNativeSocket(s);
    }
}

void SlintWebEngine::TerminateBrowserProcess() {
#ifdef _WIN32
    if (m_ProcessHandle) {
        TerminateProcess(static_cast<HANDLE>(m_ProcessHandle), 0);
        WaitForSingleObject(static_cast<HANDLE>(m_ProcessHandle), 1500);
        CloseHandle(static_cast<HANDLE>(m_ProcessHandle));
        m_ProcessHandle = nullptr;
    }
#else
    if (m_ChildPid > 0) {
        kill(-m_ChildPid, SIGTERM);
        for (int i = 0; i < 15; ++i) {
            int st = 0;
            pid_t r = waitpid(m_ChildPid, &st, WNOHANG);
            if (r == m_ChildPid || r < 0) {
                m_ChildPid = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (m_ChildPid > 0) {
            kill(-m_ChildPid, SIGKILL);
            int st = 0;
            waitpid(m_ChildPid, &st, 0);
            m_ChildPid = -1;
        }
    }
#endif
}

void SlintWebEngine::Stop() {
    m_StopRequested.store(true, std::memory_order_release);
    CloseConnection();
    if (m_ReaderThread.joinable()) {
        m_ReaderThread.join();
    }
    TerminateBrowserProcess();
}

bool SlintWebEngine::SendWsTextFrame(const std::string& text) {
    std::lock_guard<std::mutex> lk(m_SendMutex);
    NativeSocket s = static_cast<NativeSocket>(m_Socket);
    if (s == kInvalidSock || !m_Connected.load(std::memory_order_acquire)) return false;

    std::vector<uint8_t> frame;
    const size_t len = text.size();
    frame.reserve(len + 14);
    frame.push_back(0x81); // FIN + Text

    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(0x80 | len));
    } else if (len <= 65535) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<uint8_t>((static_cast<uint64_t>(len) >> shift) & 0xFF));
        }
    }

    const uint8_t mask[4] = { 0x50, 0x72, 0x6F, 0x79 };
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(static_cast<uint8_t>(text[i]) ^ mask[i & 3]);
    }

    return SendAll(s, frame.data(), frame.size());
}

bool SlintWebEngine::SendCdpCommand(const std::string& method, const std::string& paramsJson) {
    int id = m_NextCmdId.fetch_add(1, std::memory_order_relaxed);
    std::string payload = "{\"id\":" + std::to_string(id) +
                          ",\"method\":\"" + method + "\"" +
                          ",\"params\":" + paramsJson + "}";
    return SendWsTextFrame(payload);
}

void SlintWebEngine::ReaderLoop() {
    std::string assembled;
    uint8_t assembledOpcode = 0;

    while (!m_StopRequested.load(std::memory_order_acquire)) {
        NativeSocket s = static_cast<NativeSocket>(m_Socket);
        if (s == kInvalidSock) break;

        uint8_t hdr[2];
        if (!RecvExact(s, hdr, 2)) break;

        const bool fin      = (hdr[0] & 0x80) != 0;
        const uint8_t op    = hdr[0] & 0x0F;
        const bool masked   = (hdr[1] & 0x80) != 0;
        uint64_t payloadLen = hdr[1] & 0x7F;

        if (payloadLen == 126) {
            uint8_t ext[2];
            if (!RecvExact(s, ext, 2)) break;
            payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        } else if (payloadLen == 127) {
            uint8_t ext[8];
            if (!RecvExact(s, ext, 8)) break;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | ext[i];
            }
        }

        if (payloadLen > 64ULL * 1024 * 1024) break;

        uint8_t mask[4] = {};
        if (masked && !RecvExact(s, mask, 4)) break;

        std::string payload(static_cast<size_t>(payloadLen), '\0');
        if (payloadLen > 0 && !RecvExact(s, payload.data(), static_cast<size_t>(payloadLen))) break;
        if (masked) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask[i & 3]);
            }
        }

        if (op == 0x8) { // Close
            break;
        }
        if (op == 0x9) { // Ping -> Pong
            std::lock_guard<std::mutex> lk(m_SendMutex);
            uint8_t pong[6] = { 0x8A, 0x80, 0x11, 0x22, 0x33, 0x44 };
            SendAll(s, pong, sizeof(pong));
            continue;
        }
        if (op == 0xA) { // Pong
            continue;
        }

        if (op == 0x1 || op == 0x2) {
            assembledOpcode = op;
            assembled = std::move(payload);
        } else if (op == 0x0) {
            assembled += payload;
        }

        if (fin && assembledOpcode == 0x1 && !assembled.empty()) {
            HandleCdpMessage(assembled);
            assembled.clear();
            assembledOpcode = 0;
        }
    }

    m_Connected.store(false, std::memory_order_release);
}

void SlintWebEngine::DecodeAndEmitImageBytes(const std::string& b64Data) {
    if (b64Data.empty()) return;
    auto compressed = DecodeBase64(b64Data);
    if (compressed.empty()) return;

    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(
        compressed.data(), static_cast<int>(compressed.size()), &w, &h, &comp, 4);
    if (!px || w <= 0 || h <= 0) {
        if (px) stbi_image_free(px);
        return;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    std::memcpy(rgba.data(), px, rgba.size());
    stbi_image_free(px);

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_CbMutex);
        cb = m_OnFrame;
    }
    if (cb) {
        cb(std::move(rgba), w, h);
    }
}

void SlintWebEngine::HandleCdpMessage(const std::string& jsonText) {
    auto j = nlohmann::json::parse(jsonText, nullptr, false);
    if (!j.is_object()) return;

    // 1. Screencast push frame (real-time animations, video, scrolling)
    if (j.contains("method")) {
        const std::string method = j.value("method", "");
        if (method == "Page.screencastFrame" && j.contains("params")) {
            const auto& params = j["params"];
            int sid = params.value("sessionId", 0);
            SendCdpCommand("Page.screencastFrameAck", "{\"sessionId\":" + std::to_string(sid) + "}");

            int64_t nowMs = NowSteadyMs();
            int64_t prevMs = m_LastFrameEmitMs.load(std::memory_order_relaxed);
            if (nowMs - prevMs >= 50) { // Cap ~20 FPS for smooth UI without saturating Slint thread
                m_LastFrameEmitMs.store(nowMs, std::memory_order_relaxed);
                DecodeAndEmitImageBytes(params.value("data", ""));
            }
            return;
        }

        if (method == "Page.frameNavigated" && j.contains("params")) {
            const auto& fr = j["params"].value("frame", nlohmann::json::object());
            if (fr.value("parentId", "").empty() && fr.contains("url")) {
                std::string url = fr.value("url", "");
                if (!url.empty() && url != "about:blank") {
                    std::string title;
                    {
                        std::lock_guard<std::mutex> lk(m_StateMutex);
                        m_CurrentUrl = url;
                        title = m_CurrentTitle;
                    }
                    StatusCallback cb;
                    {
                        std::lock_guard<std::mutex> lk(m_CbMutex);
                        cb = m_OnStatus;
                    }
                    if (cb) cb(url, title, "Cargando página...", true);
                }
            }
            return;
        }

        if (method == "Page.loadEventFired") {
            SendCdpCommand("Runtime.evaluate", "{\"expression\":\"document.title\",\"returnByValue\":true}");
            CaptureSnapshotNow();
            std::string url, title;
            {
                std::lock_guard<std::mutex> lk(m_StateMutex);
                url   = m_CurrentUrl;
                title = m_CurrentTitle;
            }
            StatusCallback cb;
            {
                std::lock_guard<std::mutex> lk(m_CbMutex);
                cb = m_OnStatus;
            }
            if (cb) cb(url, title, "Página lista (" + m_BrowserLabel + ")", false);
            return;
        }
    }

    // 2. Command responses (captureScreenshot / document.title)
    if (j.contains("result") && j["result"].is_object()) {
        const auto& res = j["result"];
        if (res.contains("data")) {
            DecodeAndEmitImageBytes(res.value("data", ""));
            return;
        }
        if (res.contains("result") && res["result"].is_object()) {
            const auto& r2 = res["result"];
            if (r2.value("type", "") == "string") {
                std::string newTitle = r2.value("value", "");
                std::string url;
                if (!newTitle.empty()) {
                    {
                        std::lock_guard<std::mutex> lk(m_StateMutex);
                        m_CurrentTitle = newTitle;
                        url = m_CurrentUrl;
                    }
                    StatusCallback cb;
                    {
                        std::lock_guard<std::mutex> lk(m_CbMutex);
                        cb = m_OnStatus;
                    }
                    if (cb) cb(url, newTitle, "Página lista (" + m_BrowserLabel + ")", false);
                }
            }
        }
    }
}

bool SlintWebEngine::Navigate(const std::string& urlOrFilePath) {
    std::string url = NormalizeInputToUrl(urlOrFilePath);
    if (url.empty()) return false;

    std::string err;
    if (!StartIfNeeded(err)) {
        StatusCallback cb;
        {
            std::lock_guard<std::mutex> lk(m_CbMutex);
            cb = m_OnStatus;
        }
        if (cb) cb(url, "", err, false);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(m_StateMutex);
        m_CurrentUrl   = url;
        m_CurrentTitle = "";
    }

    StatusCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_CbMutex);
        cb = m_OnStatus;
    }
    if (cb) cb(url, "", "Abriendo " + url + "...", true);

    nlohmann::json params = { {"url", url} };
    if (!SendCdpCommand("Page.navigate", params.dump())) {
        return false;
    }
    return true;
}

void SlintWebEngine::Reload() {
    if (!IsRunning()) return;
    SendCdpCommand("Page.reload", "{\"ignoreCache\":false}");
}

void SlintWebEngine::GoBack() {
    if (!IsRunning()) return;
    SendCdpCommand("Runtime.evaluate", "{\"expression\":\"history.back()\"}");
    CaptureSnapshotNow();
}

void SlintWebEngine::GoForward() {
    if (!IsRunning()) return;
    SendCdpCommand("Runtime.evaluate", "{\"expression\":\"history.forward()\"}");
    CaptureSnapshotNow();
}

void SlintWebEngine::ScrollBy(int deltaX, int deltaY) {
    if (!IsRunning()) return;
    nlohmann::json ev = {
        {"type", "mouseWheel"},
        {"x", m_ViewportW / 2},
        {"y", m_ViewportH / 2},
        {"deltaX", deltaX},
        {"deltaY", deltaY}
    };
    SendCdpCommand("Input.dispatchMouseEvent", ev.dump());
    std::string js = "window.scrollBy(" + std::to_string(deltaX) + "," + std::to_string(deltaY) + ");";
    SendCdpCommand("Runtime.evaluate", nlohmann::json({{"expression", js}}).dump());
    CaptureSnapshotNow();
}

void SlintWebEngine::ClickNormalized(float normX, float normY) {
    if (!IsRunning()) return;
    const double px = std::clamp(static_cast<double>(normX), 0.0, 1.0) * static_cast<double>(m_ViewportW);
    const double py = std::clamp(static_cast<double>(normY), 0.0, 1.0) * static_cast<double>(m_ViewportH);
    SendCdpCommand("Input.dispatchMouseEvent",
        nlohmann::json({{"type", "mouseMoved"}, {"x", px}, {"y", py}}).dump());
    SendCdpCommand("Input.dispatchMouseEvent",
        nlohmann::json({{"type", "mousePressed"}, {"x", px}, {"y", py}, {"button", "left"}, {"clickCount", 1}}).dump());
    SendCdpCommand("Input.dispatchMouseEvent",
        nlohmann::json({{"type", "mouseReleased"}, {"x", px}, {"y", py}, {"button", "left"}, {"clickCount", 1}}).dump());
    CaptureSnapshotNow();
}

void SlintWebEngine::SendNavKey(const std::string& keyName) {
    if (!IsRunning()) return;
    int vk = 0;
    if (keyName == "ArrowRight") vk = 39;
    else if (keyName == "ArrowLeft") vk = 37;
    else if (keyName == "ArrowDown") vk = 40;
    else if (keyName == "ArrowUp") vk = 38;
    else if (keyName == "PageDown") vk = 34;
    else if (keyName == "PageUp") vk = 33;
    else if (keyName == "Space") vk = 32;
    else if (keyName == "Home") vk = 36;
    if (vk == 0) return;

    nlohmann::json down = {
        {"type", "keyDown"},
        {"key", keyName == "Space" ? " " : keyName},
        {"code", keyName},
        {"windowsVirtualKeyCode", vk},
        {"nativeVirtualKeyCode", vk}
    };
    nlohmann::json up = down;
    up["type"] = "keyUp";
    SendCdpCommand("Input.dispatchKeyEvent", down.dump());
    SendCdpCommand("Input.dispatchKeyEvent", up.dump());
    CaptureSnapshotNow();
}

void SlintWebEngine::SetZoomFactor(double zoom) {
    m_ZoomFactor = std::clamp(zoom, 0.5, 2.5);
    if (!IsRunning()) return;
    SendCdpCommand("Emulation.setPageScaleFactor",
        nlohmann::json({{"pageScaleFactor", m_ZoomFactor}}).dump());
    std::string js = "document.documentElement.style.zoom = '" + std::to_string(m_ZoomFactor) + "';";
    SendCdpCommand("Runtime.evaluate", nlohmann::json({{"expression", js}}).dump());
    CaptureSnapshotNow();
}

void SlintWebEngine::CaptureSnapshotNow() {
    if (!IsRunning()) return;
    SendCdpCommand("Page.captureScreenshot", "{\"format\":\"jpeg\",\"quality\":84}");
}

} // namespace ProyecThor::SlintUI
