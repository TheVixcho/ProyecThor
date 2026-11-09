#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "core/AppPaths.h"
#include "core/AppLog.h"
#include "core/CloudflareUpdater.h"
#include "ui/framework/FilePicker.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
#include <shellapi.h>
#endif

#if defined(_WIN32)
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#endif

namespace ProyecThor::UI::Settings {

// ─────────────────────────────────────────────────────────────────────────────
//  Estado global del sistema de actualización
// ─────────────────────────────────────────────────────────────────────────────

enum class UpdateStatus { Idle, Checking, UpToDate, Available, Error };

static std::atomic<UpdateStatus> s_Status{ UpdateStatus::Idle };
static std::string               s_LatestVersion  = "";
static std::string               s_DownloadUrl    = "";
static std::string               s_ErrorMsg       = "";
static std::thread               s_TaskThread;

static std::atomic<bool>         s_IsDownloading{ false };
static std::atomic<float>        s_DownloadProgress{ 0.0f };
static std::atomic<float>        s_DownloadSpeedMBs{ 0.0f };
static std::atomic<float>        s_DownloadedMB{ 0.0f };
static std::atomic<float>        s_TotalMB{ 0.0f };
static std::string               s_InstallerPath  = "";
static bool                      s_ShowModal      = false;

// ─────────────────────────────────────────────────────────────────────────────
//  Funciones internas (sin cambios en lógica de red respecto al original)
// ─────────────────────────────────────────────────────────────────────────────

// Version SemVer con sufijo de pre-release opcional ("0.6.0-beta.1" ->
// major=0 minor=6 patch=0, pre=["beta","1"]; "0.6.0" -> pre vacio).
struct SemVer {
    int major = 0, minor = 0, patch = 0;
    std::vector<std::string> pre;
};

static SemVer ParseSemVer(const std::string& v) {
    SemVer sv;
    std::string core = v, pre;
    auto dash = v.find('-');
    if (dash != std::string::npos) {
        core = v.substr(0, dash);
        pre  = v.substr(dash + 1);
    }
    sscanf(core.c_str(), "%d.%d.%d", &sv.major, &sv.minor, &sv.patch);

    size_t start = 0;
    while (!pre.empty() && start <= pre.size()) {
        size_t dot = pre.find('.', start);
        sv.pre.push_back(pre.substr(start, dot == std::string::npos ? std::string::npos : dot - start));
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return sv;
}

static bool IsAllDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

// true si la lista de identificadores 'a' es MENOR que 'b', siguiendo las
// reglas de precedencia de pre-release de SemVer (comparacion campo a campo;
// numeros comparan numericamente, texto alfabeticamente; menos campos ==
// menor si todos los compartidos son iguales -- asi "beta.1" < "beta.2" <
// "beta.10" < "rc.1").
static bool ComparePrerelease(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        if (a[i] == b[i]) continue;
        bool na = IsAllDigits(a[i]), nb = IsAllDigits(b[i]);
        if (na && nb) return std::stoll(a[i]) < std::stoll(b[i]);
        if (na != nb) return na; // identificador numerico < alfanumerico
        return a[i] < b[i];
    }
    return a.size() < b.size();
}

static bool IsNewer(const std::string& current, const std::string& latest) {
    SemVer c = ParseSemVer(current);
    SemVer l = ParseSemVer(latest);

    if (l.major != c.major) return l.major > c.major;
    if (l.minor != c.minor) return l.minor > c.minor;
    if (l.patch != c.patch) return l.patch > c.patch;

    // Mismo major.minor.patch: un release sin sufijo siempre es mas nuevo
    // que cualquier pre-release del mismo numero (0.6.0 > 0.6.0-beta.1);
    // entre dos pre-releases, compara sus identificadores.
    if (c.pre.empty()) return false; // ya estoy en el release final de este numero
    if (l.pre.empty()) return true;  // el remoto es el release final de este numero
    return ComparePrerelease(c.pre, l.pre);
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    auto end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static std::string ExtractAssetDownloadUrl(const std::string& json, const std::string& assetName) {
    std::string marker = "\"name\": \"" + assetName + "\"";
    auto pos = json.find(marker);
    if (pos == std::string::npos) {
        marker = "\"name\":\"" + assetName + "\""; // por si viene minificado
        pos = json.find(marker);
        if (pos == std::string::npos) return "";
    }
    return ExtractJsonString(json.substr(pos), "browser_download_url");
}

static std::string ExtractAppImageDownloadUrl(const std::string& json) {
    std::string search = ".AppImage\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        search = ".appimage\"";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
    }
    auto objStart = json.rfind('{', pos);
    if (objStart == std::string::npos) objStart = 0;
    return ExtractJsonString(json.substr(objStart), "browser_download_url");
}

#if defined(_WIN32)
static void ParseURL(const std::wstring& url, std::wstring& host, std::wstring& path) {
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize      = sizeof(urlComp);
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp);
    host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
    path = std::wstring(urlComp.lpszUrlPath,  urlComp.dwUrlPathLength);
}

static std::string FetchURL(const std::wstring& host, const std::wstring& path) {
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD size = 0;
        while (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
            std::string buf(size, '\0');
            DWORD read = 0;
            WinHttpReadData(hRequest, &buf[0], size, &read);
            result += buf.substr(0, read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}
#else
static std::string FetchURL(const std::string& host, const std::string& path) {
    std::string url = "https://" + host + path;
    std::string command = "curl -s -L -H \"User-Agent: ProyecThor-Updater\" \"" + url + "\"";
    std::array<char, 4096> buffer;
    std::string result;
    struct PipeDeleter {
        void operator()(FILE* f) const { if (f) pclose(f); }
    };
    std::unique_ptr<FILE, PipeDeleter> pipe(popen(command.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#endif

static void DoDownloadAndInstall(const std::string& urlStr);

static void DoCheckUpdate(const std::string& currentVersion,
                           const std::string& channel,
                           bool showPopupIfAvailable,
                           bool autoInstallIfAvailable = false) {
    s_Status = UpdateStatus::Checking;

    // 0. Consultar repositorio oficial en Cloudflare R2 si está configurado
    std::string cfBase = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.cloudflareBaseUrl;
    cfBase = ProyecThor::Core::CloudflareUpdater::NormalizeBaseUrl(cfBase);

    if (!cfBase.empty()) {
        std::string nextVer = ProyecThor::Core::CloudflareUpdater::GetNextPatchVersion(currentVersion);
        std::string exeName = ProyecThor::Core::CloudflareUpdater::GetExeFilename(nextVer);
        std::string targetUrl = cfBase + "/" + exeName;

        ProyecThor::Core::AppLog::Inf("Updater", "Buscando actualización en Cloudflare: " + cfBase);

#if defined(_WIN32)
        std::wstring wUrl(targetUrl.begin(), targetUrl.end());
        std::wstring cfHost, cfPath;
        ParseURL(wUrl, cfHost, cfPath);

        // A. Probar primero si hay un version.json en Cloudflare
        std::wstring vPath = cfPath;
        auto slash = vPath.rfind(L'/');
        if (slash != std::wstring::npos) vPath = vPath.substr(0, slash);
        vPath += L"/version.json";

        std::string jsonBody = FetchURL(cfHost, vPath);
        if (!jsonBody.empty()) {
            std::string tag = ExtractJsonString(jsonBody, "version");
            if (tag.empty()) tag = ExtractJsonString(jsonBody, "tag_name");
            if (!tag.empty() && (tag.front() == 'v' || tag.front() == 'V')) tag = tag.substr(1);

            if (!tag.empty() && IsNewer(currentVersion, tag)) {
                s_LatestVersion = tag;
                s_DownloadUrl = ExtractJsonString(jsonBody, "download_url");
                if (s_DownloadUrl.empty()) s_DownloadUrl = ExtractJsonString(jsonBody, "browser_download_url");
                if (s_DownloadUrl.empty()) s_DownloadUrl = cfBase + "/" + ProyecThor::Core::CloudflareUpdater::GetExeFilename(tag);

                s_Status = UpdateStatus::Available;
                ProyecThor::Core::AppLog::Inf("Updater", "Nueva versión encontrada en Cloudflare: v" + tag);

                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                char timeBuf[32];
                strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", localtime(&now));
                ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

                if (autoInstallIfAvailable && !s_DownloadUrl.empty()) {
                    DoDownloadAndInstall(s_DownloadUrl);
                } else if (showPopupIfAvailable && !s_DownloadUrl.empty()) {
                    s_ShowModal = true;
                }
                return;
            }
        }

        // B. Sondeo inteligente multicandidato en Cloudflare (detecta 1.1.4, 1.1.5, 1.2.0, saltos de versión, etc.)
        std::vector<std::string> candidates = ProyecThor::Core::CloudflareUpdater::GetCandidateVersions(currentVersion);
        std::string bestVer;
        std::string bestUrl;
        float bestSize = 0.0f;

        for (const auto& cand : candidates) {
            auto probe = ProyecThor::Core::CloudflareUpdater::ProbeExecutableSync(cfBase, cand);
            if (probe.success) {
                if (bestVer.empty() || IsNewer(bestVer, cand)) {
                    bestVer = cand;
                    bestUrl = probe.detectedUrl;
                    bestSize = probe.fileSizeMB;
                }
            }
        }

        if (!bestVer.empty()) {
            s_LatestVersion = bestVer;
            s_DownloadUrl = bestUrl;
            s_Status = UpdateStatus::Available;
            ProyecThor::Core::AppLog::Inf("Updater", "Nueva versión más reciente detectada en Cloudflare: v" + bestVer + " (" + bestUrl + ")");

            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timeBuf[32];
            strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", localtime(&now));
            ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

            if (autoInstallIfAvailable && !s_DownloadUrl.empty()) {
                DoDownloadAndInstall(s_DownloadUrl);
            } else if (showPopupIfAvailable && !s_DownloadUrl.empty()) {
                s_ShowModal = true;
            }
            return;
        }
#endif
    }

    std::string body;
#if defined(_WIN32)
    // 1. Consultar endpoint oficial en Firebase Hosting (proyecthor.web.app/downloads/version.json)
    body = FetchURL(L"proyecthor.web.app", L"/downloads/version.json");

    // 2. Fallback a GitHub Releases si el servidor web no respondió
    if (body.empty()) {
        std::wstring host = L"api.github.com";
        std::wstring path = (channel == "beta")
            ? L"/repos/TheVixcho/ProyecThor/releases?per_page=1"
            : L"/repos/TheVixcho/ProyecThor/releases/latest";
        body = FetchURL(host, path);
    }
#else
    body = FetchURL("proyecthor.web.app", "/downloads/version.json");
    if (body.empty()) {
        std::string host = "api.github.com";
        std::string path = (channel == "beta")
            ? "/repos/TheVixcho/ProyecThor/releases?per_page=1"
            : "/repos/TheVixcho/ProyecThor/releases/latest";
        body = FetchURL(host, path);
    }
#endif
    if (body.empty()) {
        s_ErrorMsg = "No se pudo conectar al servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }

    std::string tag = ExtractJsonString(body, "tag_name");
    if (tag.empty()) {
        tag = ExtractJsonString(body, "version");
    }
    if (tag.empty()) {
        s_ErrorMsg = "Respuesta inesperada del servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }
    if (tag[0] == 'v') tag = tag.substr(1);

#if defined(_WIN32)
    s_DownloadUrl    = ExtractAssetDownloadUrl(body, "ProyecThor_Setup.exe");
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractJsonString(body, "browser_download_url");
    }
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractJsonString(body, "download_url");
    }
#else
    s_DownloadUrl    = ExtractAssetDownloadUrl(body, "ProyecThor-x86_64.AppImage");
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractAssetDownloadUrl(body, "ProyecThor.AppImage");
    }
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractAppImageDownloadUrl(body);
    }
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractJsonString(body, "browser_download_url");
    }
    if (s_DownloadUrl.empty()) {
        s_DownloadUrl = ExtractJsonString(body, "download_url");
    }
#endif
    s_LatestVersion  = tag;

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", localtime(&now));
    ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

    if (IsNewer(currentVersion, tag)) {
        s_Status = UpdateStatus::Available;
        if (autoInstallIfAvailable && !s_DownloadUrl.empty()) {
            DoDownloadAndInstall(s_DownloadUrl);
        } else if (showPopupIfAvailable && !s_DownloadUrl.empty()) {
            s_ShowModal = true;
        }
    } else {
        s_Status = UpdateStatus::UpToDate;
    }
}

static void DoDownloadAndInstall(const std::string& urlStr) {
#if defined(_WIN32)
    s_IsDownloading    = true;
    s_DownloadProgress = 0.0f;
    s_DownloadedMB     = 0.0f;
    s_TotalMB          = 0.0f;

    std::wstring wUrl(urlStr.begin(), urlStr.end());
    std::wstring host, path;
    ParseURL(wUrl, host, path);

    HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        // Tamaño total del archivo
        DWORD contentLength = 0;
        DWORD cbSize        = sizeof(contentLength);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &contentLength, &cbSize, WINHTTP_NO_HEADER_INDEX);

        float totalBytes = (float)contentLength;
        s_TotalMB        = totalBytes / (1024.0f * 1024.0f);

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        // .exe (instalador de Inno Setup, ver packaging/windows/ProyecThor.iss):
        // ShellExecute con verbo "open" lo corre directo, sin depender de
        // ninguna asociacion de archivo de Windows (a diferencia del .msi
        // que se uso mientras se empaqueto con WiX Toolset).
        s_InstallerPath = std::string(tempPath) + "ProyecThor_Update.exe";

        std::ofstream outFile(s_InstallerPath, std::ios::binary);
        float  downloadedBytes = 0.0f;
        auto   lastTime        = std::chrono::steady_clock::now();
        float  bytesSinceLast  = 0.0f;

        DWORD size = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
            if (size == 0) break;

            std::vector<char> buf(size);
            DWORD read = 0;
            if (WinHttpReadData(hRequest, buf.data(), size, &read)) {
                outFile.write(buf.data(), read);
                downloadedBytes  += (float)read;
                bytesSinceLast   += (float)read;
                s_DownloadedMB    = downloadedBytes / (1024.0f * 1024.0f);

                if (totalBytes > 0.0f)
                    s_DownloadProgress = downloadedBytes / totalBytes;

                // Calcular velocidad cada 300ms
                auto now  = std::chrono::steady_clock::now();
                float sec = std::chrono::duration<float>(now - lastTime).count();
                if (sec >= 0.3f) {
                    s_DownloadSpeedMBs = (bytesSinceLast / (1024.0f * 1024.0f)) / sec;
                    bytesSinceLast     = 0.0f;
                    lastTime           = now;
                }
            }
        } while (size > 0);

        outFile.close();
        ShellExecuteA(NULL, "open", s_InstallerPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        exit(0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    s_IsDownloading = false;
    s_ShowModal     = false;
#else
    s_IsDownloading    = true;
    s_DownloadProgress = 0.0f;
    s_DownloadedMB     = 0.0f;
    s_TotalMB          = 0.0f;
    s_DownloadSpeedMBs = 0.0f;

    const char* appImageEnv = std::getenv("APPIMAGE");
    std::string currentAppImage = appImageEnv ? appImageEnv : "";

    std::string tempDownload = "/tmp/ProyecThor_Update.AppImage";
    if (!currentAppImage.empty()) {
        tempDownload = currentAppImage + ".new";
    }

    // Obtener tamaño del archivo via Content-Length de headers HTTP
    std::string headCmd = "curl -s -L -I -H \"User-Agent: ProyecThor-Updater\" \"" + urlStr + "\"";
    float totalBytes = 0.0f;
    {
        FILE* hPipe = popen(headCmd.c_str(), "r");
        if (hPipe) {
            char line[512];
            while (fgets(line, sizeof(line), hPipe)) {
                std::string s(line);
                std::string sLower = s;
                std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);
                if (sLower.find("content-length:") != std::string::npos) {
                    size_t pos = s.find(':');
                    if (pos != std::string::npos) {
                        try {
                            float val = std::stof(s.substr(pos + 1));
                            if (val > totalBytes) totalBytes = val;
                        } catch (...) {}
                    }
                }
            }
            pclose(hPipe);
        }
    }
    s_TotalMB = totalBytes / (1024.0f * 1024.0f);

    std::error_code ecRemove;
    std::filesystem::remove(tempDownload, ecRemove);

    std::string dlCmd = "curl -s -L -H \"User-Agent: ProyecThor-Updater\" -o \"" + tempDownload + "\" \"" + urlStr + "\"";

    std::atomic<bool> dlFinished{false};
    int exitCode = -1;
    std::thread dlThread([dlCmd, &dlFinished, &exitCode]() {
        exitCode = system(dlCmd.c_str());
        dlFinished.store(true);
    });

    auto lastTime = std::chrono::steady_clock::now();
    float lastBytes = 0.0f;

    while (!dlFinished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::filesystem::exists(tempDownload)) {
            std::error_code ec;
            auto currentSize = (float)std::filesystem::file_size(tempDownload, ec);
            if (!ec) {
                s_DownloadedMB = currentSize / (1024.0f * 1024.0f);
                if (totalBytes > 0.0f) {
                    s_DownloadProgress = std::min(0.99f, currentSize / totalBytes);
                }

                auto now = std::chrono::steady_clock::now();
                float sec = std::chrono::duration<float>(now - lastTime).count();
                if (sec >= 0.4f) {
                    float diff = currentSize - lastBytes;
                    s_DownloadSpeedMBs = (diff / (1024.0f * 1024.0f)) / sec;
                    lastBytes = currentSize;
                    lastTime = now;
                }
            }
        }
    }
    if (dlThread.joinable()) dlThread.join();

    if (exitCode == 0 && std::filesystem::exists(tempDownload)) {
        s_DownloadProgress = 1.0f;
        std::error_code ec;
        std::filesystem::permissions(tempDownload,
            std::filesystem::perms::owner_all |
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::replace, ec);

        if (!currentAppImage.empty()) {
            std::filesystem::rename(tempDownload, currentAppImage, ec);
            if (ec) {
                std::filesystem::copy_file(tempDownload, currentAppImage,
                    std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(tempDownload, ec);
            }
            std::string launchCmd = "\"" + currentAppImage + "\" &";
            system(launchCmd.c_str());
            exit(0);
        } else {
            std::string launchCmd = "\"" + tempDownload + "\" &";
            system(launchCmd.c_str());
            exit(0);
        }
    } else {
        s_ErrorMsg = "Error al descargar el paquete de actualización.";
        s_Status = UpdateStatus::Error;
    }

    s_IsDownloading = false;
    s_ShowModal     = false;
#endif
}

#if defined(_WIN32)
// ─────────────────────────────────────────────────────────────────────────────
//  Limpieza de versiones anteriores instaladas -- mismo criterio que
//  ProyecThor.iss [Code] (GetSelfUninstallString/UninstallOldMsiIfFound), pero
//  disparado desde DENTRO de la app ya corriendo (no solo al instalar una
//  version nueva): recorre TODAS las ubicaciones de Uninstall (ver abajo)
//  buscando CUALQUIER entrada con DisplayName="ProyecThor" y desinstala en
//  silencio solo las que tengan una DisplayVersion ESTRICTAMENTE MENOR a la
//  version que esta corriendo ahora -- nunca toca la version actual ni
//  ninguna mas nueva (pedido explicito).
//
//  El instalador actual (Inno, PrivilegesRequired=lowest) registra bajo
//  HKCU -- pero versiones viejas empaquetadas con el .msi de WiX Toolset
//  (retirado, ver comentario en ProyecThor.iss) se instalaban per-machine y
//  quedaron registradas bajo HKLM. Sin escanear HKLM tambien, esas 0.4.x
//  nunca aparecian (confirmado: 5 instalaciones MSI 0.4.2.0 sueltas ahi).
//  Wow6432Node se suma por si alguna fue un build de 32 bits muy vieja.
// ─────────────────────────────────────────────────────────────────────────────

enum class CleanupStatus { Idle, Running, Done };
static std::atomic<CleanupStatus> s_CleanupStatus{ CleanupStatus::Idle };
static std::string                s_CleanupSummary;
static bool                       s_CleanupHadIssues = false; // true si algo se cancelo/fallo -- cambia el color del resumen
static std::thread                s_CleanupThread;

struct OldInstallEntry {
    std::string keyName;         // nombre de la subkey bajo Uninstall (== ProductCode para MSI)
    std::string version;
    std::string uninstallString;
    bool        isMsi = false;
};

static void ScanUninstallHive(HKEY root, const char* subPath,
                               const std::string& currentVersion,
                               std::vector<OldInstallEntry>& result) {
    HKEY hUninstall;
    // KEY_WOW64_64KEY: al leer HKLM\...\Uninstall desde un proceso de 64
    // bits (este build es x64-only, ver ArchitecturesAllowed en el .iss) ya
    // cae en la vista nativa de 64 bits por default -- el flag es explicito
    // igual para no depender de ese default silencioso.
    if (RegOpenKeyExA(root, subPath, 0, KEY_READ | KEY_WOW64_64KEY, &hUninstall) != ERROR_SUCCESS)
        return;

    for (DWORD i = 0; ; i++) {
        char  keyName[256];
        DWORD keyNameLen = sizeof(keyName);
        if (RegEnumKeyExA(hUninstall, i, keyName, &keyNameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        HKEY hSub;
        if (RegOpenKeyExA(hUninstall, keyName, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
            continue;

        char  displayName[256] = {};
        DWORD dnSize = sizeof(displayName);
        DWORD type   = 0;
        bool isOurs = (RegQueryValueExA(hSub, "DisplayName", nullptr, &type,
                        reinterpret_cast<LPBYTE>(displayName), &dnSize) == ERROR_SUCCESS) &&
                      (std::string(displayName) == "ProyecThor");

        if (isOurs) {
            char  version[64] = {};
            DWORD vSize = sizeof(version);
            RegQueryValueExA(hSub, "DisplayVersion", nullptr, &type,
                             reinterpret_cast<LPBYTE>(version), &vSize);

            char  uninstStr[1024] = {};
            DWORD uSize = sizeof(uninstStr);
            RegQueryValueExA(hSub, "UninstallString", nullptr, &type,
                             reinterpret_cast<LPBYTE>(uninstStr), &uSize);

            std::string versionStr(version);
            // Excluye la version actual y cualquiera mas nueva -- solo se
            // desinstala lo ESTRICTAMENTE mas viejo que lo que esta corriendo.
            if (!versionStr.empty() && IsNewer(versionStr, currentVersion)) {
                OldInstallEntry e;
                e.keyName         = keyName;
                e.version         = versionStr;
                e.uninstallString = uninstStr;
                e.isMsi           = e.uninstallString.find("MsiExec.exe") != std::string::npos;
                result.push_back(std::move(e));
            }
        }

        RegCloseKey(hSub);
    }

    RegCloseKey(hUninstall);
}

static std::vector<OldInstallEntry> FindOldProyecThorInstalls(const std::string& currentVersion) {
    std::vector<OldInstallEntry> result;
    const char* kUninstallPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    const char* kUninstallPathWow = "Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    ScanUninstallHive(HKEY_CURRENT_USER,  kUninstallPath,    currentVersion, result);
    ScanUninstallHive(HKEY_LOCAL_MACHINE, kUninstallPath,    currentVersion, result);
    ScanUninstallHive(HKEY_LOCAL_MACHINE, kUninstallPathWow, currentVersion, result);

    return result;
}

// FIX: la primera version de esta limpieza lanzaba msiexec con verbo "open"
// (sin elevar) para los .msi per-machine encontrados en HKLM -- el servicio
// de Windows Installer rechaza esa desinstalacion SIN mostrar ningun aviso
// (con /qn no hay UI que mostrar el error), asi que ShellExecute reportaba
// "se lanzo bien" (pudo arrancar msiexec.exe) aunque msiexec.exe adentro
// fallara al toque por falta de permisos -- el resumen decia "desinstalado"
// y en los hechos no se habia borrado nada. Confirmado por el usuario: las
// 5 entradas .msi seguian ahi despues de correr esto.
//
// Fix: pedir elevacion de verdad con el verbo "runas" (dispara el UAC real
// -- "pedirle permiso al usuario", pedido explicito) para los .msi
// per-machine, y esperar a que el proceso termine (ShellExecuteExA +
// WaitForSingleObject) para leer su codigo de salida real en vez de asumir
// exito por haber podido lanzarlo. Si el usuario cancela el UAC
// (GetLastError()==ERROR_CANCELLED) se cuenta aparte, no como exito ni como
// fallo silencioso.
struct UninstallRunResult { bool launched = false; bool userDeclinedElevation = false; bool completed = false; DWORD exitCode = 0; };

static UninstallRunResult RunUninstallerAndWait(const char* file, const std::string& args, bool elevate) {
    UninstallRunResult r;
    SHELLEXECUTEINFOA sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = elevate ? "runas" : "open";
    sei.lpFile       = file;
    sei.lpParameters = args.c_str();
    // SW_HIDE serviria igual para el proceso en si, pero el prompt de UAC
    // (secure desktop) no depende de esto -- se ve igual cualquiera sea el
    // valor. Se deja SW_HIDE para no mostrar la ventana de msiexec/unins de
    // ahi en mas (van con /qn o /VERYSILENT).
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExA(&sei)) {
        if (GetLastError() == ERROR_CANCELLED) r.userDeclinedElevation = true;
        return r;
    }
    r.launched = true;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        GetExitCodeProcess(sei.hProcess, &r.exitCode);
        CloseHandle(sei.hProcess);
        r.completed = true;
    }
    return r;
}

static void DoCleanupOldInstalls(std::string currentVersion) {
    s_CleanupStatus = CleanupStatus::Running;

    auto olds = FindOldProyecThorInstalls(currentVersion);
    if (olds.empty()) {
        s_CleanupSummary = "No se encontro ninguna version anterior instalada.";
        s_CleanupStatus  = CleanupStatus::Done;
        return;
    }

    int ok = 0, declined = 0, failed = 0;
    for (auto& e : olds) {
        UninstallRunResult r;
        if (e.isMsi) {
            // Mismo criterio que UninstallOldMsiIfFound() del .iss: el nombre
            // de la subkey es el ProductCode. Estas son per-machine (HKLM) --
            // SIEMPRE necesitan elevacion real, ver comentario arriba.
            std::string args = "/x " + e.keyName + " /qn /norestart";
            r = RunUninstallerAndWait("msiexec.exe", args, /*elevate=*/true);
        } else if (!e.uninstallString.empty()) {
            // Instalador Inno (unins000.exe) -- mismo criterio que
            // GetSelfUninstallString()/InitializeSetup() del .iss. Estos son
            // per-user (HKCU): no necesitan elevacion.
            std::string exe = e.uninstallString;
            if (!exe.empty() && exe.front() == '"') {
                auto endQuote = exe.find('"', 1);
                if (endQuote != std::string::npos) exe = exe.substr(1, endQuote - 1);
            }
            r = RunUninstallerAndWait(exe.c_str(), "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART",
                                      /*elevate=*/false);
        } else {
            continue;
        }

        if (r.userDeclinedElevation) declined++;
        else if (r.completed && (r.exitCode == 0 || r.exitCode == 3010)) ok++; // 3010 = OK, reinicio pendiente
        else failed++;
    }

    char buf[220];
    if (declined == 0 && failed == 0) {
        snprintf(buf, sizeof(buf), "Se desinstalaron %d version%s anterior%s.",
                 ok, ok == 1 ? "" : "es", ok == 1 ? "" : "es");
    } else {
        snprintf(buf, sizeof(buf),
                 "Desinstaladas: %d. Canceladas (permiso denegado): %d. Fallidas: %d.",
                 ok, declined, failed);
    }
    s_CleanupSummary   = buf;
    s_CleanupHadIssues = (declined > 0 || failed > 0);
    s_CleanupStatus    = CleanupStatus::Done;
}
#endif // _WIN32

// ─────────────────────────────────────────────────────────────────────────────
//  Carpeta de datos -- copia TODA la carpeta de datos actual (ver
//  ProyecThor::GetAppDataRoot, AppPaths.h) a la ubicacion que elija el
//  operador y guarda esa ubicacion en un archivo chico de redireccion que
//  vive SIEMPRE en la carpeta default de fabrica (nunca se mueve, ver
//  ProyecThor::GetDataDirRedirectFilePath) -- asi la app puede encontrar
//  sus datos reales incluso antes de saber donde estan. Copia, NO mueve: la
//  carpeta original queda intacta a proposito (evita perder datos si algo
//  falla a mitad de camino) -- el operador la borra a mano despues si
//  quiere el espacio de vuelta. Como GetAssetsPath()/GetAppDataRoot() se
//  cachean en variables static la primera vez que se llaman, el cambio solo
//  toma efecto de verdad reiniciando la app.
// ─────────────────────────────────────────────────────────────────────────────



// ─────────────────────────────────────────────────────────────────────────────
//  API pública: arranque y modal global
// ─────────────────────────────────────────────────────────────────────────────

void CheckUpdateOnStartup() {
    auto& u = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;
    if (u.checkOnStartup) {
        if (s_TaskThread.joinable()) s_TaskThread.join();
        s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel, true, false);
        s_TaskThread.detach();
    }
}

void RenderGlobalUpdatePopup() {
    if (s_ShowModal)
        ImGui::OpenPopup("Actualizacion Disponible");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.062f, 0.090f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(24.0f, 22.0f));

    if (ImGui::BeginPopupModal("Actualizacion Disponible", NULL,
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {

        // Encabezado del modal
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.93f, 0.97f, 1.0f));
        ImGui::TextUnformatted("Nueva versión disponible");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Versión con badge de color
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.65f, 0.14f, 1.0f));
        ImGui::Text("v%s", s_LatestVersion.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (s_IsDownloading) {
            // ── Estado: descargando ──────────────────────────────────────────

            float pct    = s_DownloadProgress.load();
            float dlMB   = s_DownloadedMB.load();
            float totMB  = s_TotalMB.load();
            float speed  = s_DownloadSpeedMBs.load();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.86f, 1.0f));
            ImGui::TextUnformatted("Descargando actualizacion...");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Barra de progreso con porcentaje integrado
            // Usamos ProgressBar nativo más el texto de MB/velocidad debajo
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.350f, 0.500f, 0.970f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.10f, 0.114f, 0.160f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::ProgressBar(pct, ImVec2(-1.0f, 10.0f), "");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            // Línea de metadatos: MB descargados, MB totales, velocidad y %
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.47f, 0.60f, 1.0f));
            if (totMB > 0.0f)
                ImGui::Text("%.1f MB / %.1f MB   %.1f MB/s   %d%%",
                            dlMB, totMB, speed, (int)(pct * 100.0f));
            else
                ImGui::Text("Descargando...  %d%%", (int)(pct * 100.0f));
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.40f, 0.40f, 1.0f));
            ImGui::TextUnformatted("No cierres ProyecThor durante la descarga.");
            ImGui::PopStyleColor();

        } else {
            // ── Estado: esperando confirmación ───────────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.86f, 1.0f));
            ImGui::TextWrapped("Hay una nueva versión disponible. Al aceptar, ProyecThor "
                               "descargara el instalador y se cerrara automaticamente "
                               "para aplicar la actualizacion.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));

            // Botón principal
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.350f, 0.500f, 0.970f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.420f, 0.570f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.280f, 0.420f, 0.880f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            if (ImGui::Button("Descargar e instalar", ImVec2(200.0f, 36.0f))) {
                if (!s_IsDownloading && !s_DownloadUrl.empty()) {
                    if (s_TaskThread.joinable()) s_TaskThread.join();
                    s_TaskThread = std::thread(DoDownloadAndInstall, s_DownloadUrl);
                    s_TaskThread.detach();
                }
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 10.0f);

            // Botón secundario
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.60f, 0.62f, 0.76f, 1.0f));

            if (ImGui::Button("Mas tarde", ImVec2(110.0f, 36.0f))) {
                s_ShowModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCategoryUpdates — panel dentro de SettingsPanel
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderCategoryUpdates() {
    auto& u  = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;
    auto  st = s_Status.load();
    u.updateChannel = "stable"; // Canal oficial único y estable

    const bool isEn = (ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language == ProyecThor::Settings::Language::English);

    if (SectionTitle(isEn ? "Installed Version" : "Versión instalada", isEn ? "Updates" : "Actualizaciones")) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float availW = ImGui::GetContentRegionAvail().x;

        // ── Tarjeta Principal de Versión & Canal Oficial ────────────────────
        ImVec2 heroPos = ImGui::GetCursorScreenPos();
        float heroH = 92.0f;
        dl->AddRectFilled(heroPos, ImVec2(heroPos.x + availW, heroPos.y + heroH),
                          IM_COL32(18, 22, 34, 235), 10.0f);
        dl->AddRect(heroPos, ImVec2(heroPos.x + availW, heroPos.y + heroH),
                    IM_COL32(56, 189, 248, 60), 10.0f, 0, 1.2f);

        // Icono circular
        ImVec2 iconC(heroPos.x + 36.0f, heroPos.y + heroH * 0.5f);
        dl->AddCircleFilled(iconC, 20.0f, IM_COL32(56, 189, 248, 30), 24);
        dl->AddCircle(iconC, 20.0f, IM_COL32(56, 189, 248, 120), 24, 1.2f);
        dl->AddLine(ImVec2(iconC.x, iconC.y - 8.0f), ImVec2(iconC.x, iconC.y + 4.0f), IM_COL32(56, 189, 248, 255), 2.0f);
        dl->AddTriangleFilled(ImVec2(iconC.x - 6.0f, iconC.y + 2.0f),
                              ImVec2(iconC.x + 6.0f, iconC.y + 2.0f),
                              ImVec2(iconC.x, iconC.y + 8.0f), IM_COL32(56, 189, 248, 255));
        dl->AddLine(ImVec2(iconC.x - 8.0f, iconC.y + 11.0f), ImVec2(iconC.x + 8.0f, iconC.y + 11.0f), IM_COL32(56, 189, 248, 255), 1.5f);

        // Textos del Hero
        float textX = heroPos.x + 70.0f;
        dl->AddText(ImVec2(textX, heroPos.y + 16.0f), IM_COL32(255, 255, 255, 255), "ProyecThor Desktop");

        std::string verStr = std::string(isEn ? "Version " : "Versión ") + "v" + u.currentVersion;
        dl->AddText(ImVec2(textX, heroPos.y + 38.0f), IM_COL32(148, 163, 184, 255), verStr.c_str());

        std::string lastChk = u.lastChecked.empty()
            ? (isEn ? "Not checked yet" : "Sin comprobaciones recientes")
            : ((isEn ? "Last check: " : "Última comprobación: ") + u.lastChecked);
        dl->AddText(ImVec2(textX, heroPos.y + 58.0f), IM_COL32(100, 116, 139, 255), lastChk.c_str());



        ImGui::SetCursorScreenPos(ImVec2(heroPos.x, heroPos.y + heroH + 16.0f));

        // ── Tarjeta de Estado ───────────────────────────────────────────────
        struct StatusStyle {
            ImVec4 borderCol;
            ImVec4 bgCol;
            ImVec4 textCol;
        };
        StatusStyle ss;
        const char* statusTitle  = "";
        const char* statusDetail = "";
        bool showSpinner         = false;

        switch (st) {
            case UpdateStatus::Idle:
                ss = { ImVec4(0.24f,0.26f,0.38f,0.8f), ImVec4(0.08f,0.09f,0.14f,0.9f), ImVec4(0.70f,0.72f,0.86f,1) };
                statusTitle  = isEn ? "Update System Ready" : "Sistema de actualización listo";
                statusDetail = isEn
                    ? "Click 'Check for Updates' to search and install the latest official release automatically."
                    : "Presiona 'Comprobar Actualizaciones' para buscar e instalar la última versión automáticamente sin complicaciones.";
                break;
            case UpdateStatus::Checking:
                ss = { ImVec4(0.22f,0.40f,0.88f,0.8f), ImVec4(0.06f,0.10f,0.22f,0.9f), ImVec4(0.55f,0.75f,1.00f,1) };
                statusTitle  = isEn ? "Checking official server..." : "Comprobando servidor oficial...";
                statusDetail = isEn
                    ? "Connecting to repository to check for updates..."
                    : "Conectando al repositorio oficial para verificar nuevas versiones estables...";
                showSpinner  = true;
                break;
            case UpdateStatus::UpToDate:
                ss = { ImVec4(0.20f,0.66f,0.40f,0.8f), ImVec4(0.05f,0.14f,0.09f,0.9f), ImVec4(0.40f,0.92f,0.60f,1) };
                statusTitle  = isEn ? "You are up to date!" : "¡Tu versión está al día!";
                statusDetail = isEn
                    ? "You have the latest release of ProyecThor installed. No updates needed."
                    : "Tienes instalada la versión más reciente de ProyecThor. No hay actualizaciones pendientes.";
                break;
            case UpdateStatus::Available:
                ss = { ImVec4(0.88f,0.58f,0.10f,0.8f), ImVec4(0.18f,0.12f,0.04f,0.9f), ImVec4(0.98f,0.80f,0.35f,1) };
                statusTitle  = isEn ? "New version found!" : "¡Nueva versión disponible!";
                statusDetail = isEn
                    ? "Downloading and launching installer automatically..."
                    : "Descargando e iniciando el instalador automáticamente...";
                break;
            case UpdateStatus::Error:
                ss = { ImVec4(0.88f,0.28f,0.28f,0.8f), ImVec4(0.16f,0.04f,0.04f,0.9f), ImVec4(0.96f,0.55f,0.55f,1) };
                statusTitle  = isEn ? "Connection error" : "No se pudo conectar";
                statusDetail = s_ErrorMsg.empty()
                    ? (isEn ? "Failed to contact update server." : "No se pudo contactar al servidor de actualizaciones.")
                    : s_ErrorMsg.c_str();
                break;
        }

        ImVec2 boxPos = ImGui::GetCursorScreenPos();
        float boxH = 68.0f;
        dl->AddRectFilled(boxPos, ImVec2(boxPos.x + availW, boxPos.y + boxH),
                          ImGui::ColorConvertFloat4ToU32(ss.bgCol), 8.0f);
        dl->AddRect(boxPos, ImVec2(boxPos.x + availW, boxPos.y + boxH),
                    ImGui::ColorConvertFloat4ToU32(ss.borderCol), 8.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 14.0f));
        if (showSpinner) {
            SpinnerWidget(8.0f, 2.0f, ss.textCol);
            ImGui::SameLine(0, 10.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ss.textCol);
        ImGui::TextUnformatted(statusTitle);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 38.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ss.textCol.x, ss.textCol.y, ss.textCol.z, 0.78f));
        ImGui::TextUnformatted(statusDetail);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(boxPos.x, boxPos.y + boxH + 16.0f));

        // ── Descarga activa con barra animada ───────────────────────────────
        if (s_IsDownloading) {
            float pct   = s_DownloadProgress.load();
            float dlMB  = s_DownloadedMB.load();
            float totMB = s_TotalMB.load();
            float speed = s_DownloadSpeedMBs.load();

            ImGui::SeparatorText(isEn ? "Downloading update" : "Descargando actualización");
            AnimatedProgressBar(pct, ImVec2(availW - 40.0f, 10.0f),
                                ImVec4(0.220f, 0.740f, 0.970f, 1.0f));

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.75f, 0.88f, 1.0f));
            if (totMB > 0.0f) {
                ImGui::Text(isEn ? "%.1f MB / %.1f MB  ·  %.1f MB/s  (Installing automatically when finished...)"
                                 : "%.1f MB / %.1f MB  ·  %.1f MB/s  (Se iniciará el instalador automáticamente al finalizar...)",
                            dlMB, totMB, speed);
            } else {
                ImGui::TextUnformatted(isEn ? "Downloading..." : "Descargando...");
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // ── Botón Prominente: "Comprobar Actualizaciones" ────────────────────
        bool busy = (st == UpdateStatus::Checking) || s_IsDownloading;
        if (busy) ImGui::BeginDisabled();

        const float actionBtnW = 260.0f;
        const float actionBtnH = 38.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(38, 100, 235, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 120, 248, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(28, 80, 200, 255));
        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(255, 255, 255, 255));

        const char* btnLabel = busy
            ? (s_IsDownloading ? (isEn ? "Downloading update..." : "Descargando actualización...")
                               : (isEn ? "Checking..." : "Comprobando..."))
            : (isEn ? "Check for Updates" : "Comprobar Actualizaciones");

        if (ImGui::Button(btnLabel, ImVec2(actionBtnW, actionBtnH))) {
            if (s_TaskThread.joinable()) s_TaskThread.join();
            s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, "stable", false, true);
            s_TaskThread.detach();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        if (busy) ImGui::EndDisabled();

        // ── Herramienta de Limpieza de versiones viejas ─────────────────────
#ifdef _WIN32
        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::SeparatorText(isEn ? "Maintenance Tools" : "Herramientas de instalación");

        auto cst         = s_CleanupStatus.load();
        bool cleanupBusy = (cst == CleanupStatus::Running);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.65f, 0.68f, 0.80f, 1.0f));

        if (cleanupBusy) ImGui::BeginDisabled();
        if (ImGui::Button(cleanupBusy ? (isEn ? "Searching..." : "Buscando...") : (isEn ? "Uninstall previous versions" : "Desinstalar versiones anteriores"),
                          ImVec2(230.0f, 28.0f)))
            ImGui::OpenPopup("Desinstalar versiones anteriores?##cleanupOld");
        if (cleanupBusy) ImGui::EndDisabled();

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImVec2 mcenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(mcenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Desinstalar versiones anteriores?##cleanupOld", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped(
                isEn ? "Previous ProyecThor installations older than v%s will be searched and removed. Your current version will not be affected."
                     : "Se buscarán instalaciones de ProyecThor más viejas que v%s y se desinstalarán en silencio. Esto no afecta la versión que estás usando ahora.",
                u.currentVersion.c_str());
            ImGui::Spacing();
            if (ImGui::Button(isEn ? "Uninstall" : "Desinstalar", ImVec2(120, 0))) {
                if (s_CleanupThread.joinable()) s_CleanupThread.join();
                s_CleanupStatus = CleanupStatus::Running;
                s_CleanupThread = std::thread(DoCleanupOldInstalls, u.currentVersion);
                s_CleanupThread.detach();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(isEn ? "Cancel" : "Cancelar", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (cst == CleanupStatus::Done && !s_CleanupSummary.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, s_CleanupHadIssues
                ? ImVec4(0.90f, 0.70f, 0.35f, 1.0f)
                : ImVec4(0.50f, 0.80f, 0.55f, 1.0f));
            ImGui::TextWrapped("%s", s_CleanupSummary.c_str());
            ImGui::PopStyleColor();
        }
#endif
    }
}

} // namespace ProyecThor::UI::Settings