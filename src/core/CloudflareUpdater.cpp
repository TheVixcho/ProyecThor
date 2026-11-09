#include "CloudflareUpdater.h"
#include "AppLog.h"
#include <thread>
#include <chrono>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
    #include <windows.h>
    #include <winhttp.h>
    #include <shellapi.h>
    #pragma comment(lib, "winhttp.lib")
#endif

namespace ProyecThor::Core {

CloudflareUpdater& CloudflareUpdater::Get() {
    static CloudflareUpdater s_Instance;
    return s_Instance;
}

std::string CloudflareUpdater::NormalizeBaseUrl(const std::string& inputUrl) {
    std::string s = inputUrl;
    // Trim whitespace
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();

    if (s.empty()) return "";

    // Si termina en un archivo .exe o .json, extraer solo el directorio base
    auto posExe = s.rfind(".exe");
    if (posExe != std::string::npos && posExe == s.length() - 4) {
        auto slash = s.rfind('/');
        if (slash != std::string::npos) {
            s = s.substr(0, slash);
        }
    }
    auto posJson = s.rfind(".json");
    if (posJson != std::string::npos && posJson == s.length() - 5) {
        auto slash = s.rfind('/');
        if (slash != std::string::npos) {
            s = s.substr(0, slash);
        }
    }

    // Quitar barra final si tiene
    while (!s.empty() && s.back() == '/') {
        s.pop_back();
    }

    return s;
}

bool CloudflareUpdater::IsDashboardUrl(const std::string& url) {
    return (url.find("dash.cloudflare.com") != std::string::npos);
}

std::string CloudflareUpdater::GetExeFilename(const std::string& version) {
    std::string clean = version;
    if (!clean.empty() && (clean.front() == 'v' || clean.front() == 'V')) {
        clean = clean.substr(1);
    }
    return "ProyecThor_Setup_" + clean + ".exe";
}

std::string CloudflareUpdater::GetNextPatchVersion(const std::string& currentVersion) {
    std::string clean = currentVersion;
    if (!clean.empty() && (clean.front() == 'v' || clean.front() == 'V')) {
        clean = clean.substr(1);
    }
    int maj = 1, min = 0, patch = 0;
    if (sscanf(clean.c_str(), "%d.%d.%d", &maj, &min, &patch) >= 3) {
        return std::to_string(maj) + "." + std::to_string(min) + "." + std::to_string(patch + 1);
    }
    return clean + ".1";
}

struct SimpleSemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

static SimpleSemVer ParseSimpleSemVer(const std::string& v) {
    SimpleSemVer res;
    std::string s = v;
    if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) s = s.substr(1);
    sscanf(s.c_str(), "%d.%d.%d", &res.major, &res.minor, &res.patch);
    return res;
}

bool CloudflareUpdater::IsVersionNewer(const std::string& localVersion, const std::string& remoteVersion) {
    SimpleSemVer l = ParseSimpleSemVer(localVersion);
    SimpleSemVer r = ParseSimpleSemVer(remoteVersion);
    if (r.major != l.major) return r.major > l.major;
    if (r.minor != l.minor) return r.minor > l.minor;
    return r.patch > l.patch;
}

std::vector<std::string> CloudflareUpdater::GetCandidateVersions(const std::string& currentVersion) {
    SimpleSemVer cur = ParseSimpleSemVer(currentVersion);
    std::vector<std::string> candidates;

    // 1. Próximos parches inmediatos y saltos de parche (ej. 1.1.4, 1.1.5, 1.1.6 ... 1.1.12)
    for (int p = cur.patch + 1; p <= cur.patch + 10; ++p) {
        candidates.push_back(std::to_string(cur.major) + "." + std::to_string(cur.minor) + "." + std::to_string(p));
    }

    // 2. Próximas versiones menores (ej. 1.2.0, 1.2.1, 1.2.2, 1.3.0, 1.4.0)
    for (int m = cur.minor + 1; m <= cur.minor + 3; ++m) {
        candidates.push_back(std::to_string(cur.major) + "." + std::to_string(m) + ".0");
        candidates.push_back(std::to_string(cur.major) + "." + std::to_string(m) + ".1");
        candidates.push_back(std::to_string(cur.major) + "." + std::to_string(m) + ".2");
    }

    // 3. Próximas versiones mayores (ej. 2.0.0, 2.0.1, 2.1.0, 3.0.0)
    for (int M = cur.major + 1; M <= cur.major + 2; ++M) {
        candidates.push_back(std::to_string(M) + ".0.0");
        candidates.push_back(std::to_string(M) + ".0.1");
        candidates.push_back(std::to_string(M) + ".1.0");
    }

    return candidates;
}

#if defined(_WIN32)
static std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return "";
    std::string out(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &out[0], sz, nullptr, nullptr);
    return out;
}

static void CrackUrlHelper(const std::wstring& url, std::wstring& outHost, std::wstring& outPath, INTERNET_PORT& outPort, bool& outIsHttps) {
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize     = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength  = (DWORD)-1;
    urlComp.dwSchemeLength   = (DWORD)-1;

    WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp);
    outHost = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
    outPath = std::wstring(urlComp.lpszUrlPath,  urlComp.dwUrlPathLength);
    outPort = urlComp.nPort;
    outIsHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
}
#endif

CloudflareProbeResult CloudflareUpdater::ProbeExecutableSync(const std::string& baseUrl, const std::string& version) {
    std::string base = NormalizeBaseUrl(baseUrl);
    std::string filename = GetExeFilename(version);
    std::string fullUrl = base + "/" + filename;

    CloudflareProbeResult res;
    res.detectedUrl     = fullUrl;
    res.detectedVersion = version;

    auto tStart = std::chrono::steady_clock::now();

#if defined(_WIN32)
    std::wstring wUrl(fullUrl.begin(), fullUrl.end());
    std::wstring host, path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isHttps = true;
    CrackUrlHelper(wUrl, host, path, port, isHttps);

    if (host.empty()) {
        res.success  = false;
        res.errorMsg = "URL inválida o incompleta.";
        return res;
    }

    HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        res.success  = false;
        res.errorMsg = "Error al inicializar WinHttp.";
        return res;
    }

    // Timeouts rápidos para sondeo ligero (3.5s)
    WinHttpSetTimeouts(hSession, 3500, 3500, 3500, 3500);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        res.success  = false;
        res.errorMsg = "No se pudo conectar al host " + WStringToUtf8(host);
        return res;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", path.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);

    if (hRequest) {
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD statusCode = 0;
            DWORD dwSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

            res.statusCode = (int)statusCode;

            // Si HEAD no está permitido (405) o restringido, probar GET parcial (bytes=0-1024)
            if (statusCode == 405 || statusCode == 403) {
                WinHttpCloseHandle(hRequest);
                hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              flags);
                if (hRequest) {
                    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
                    LPCWSTR rangeHdr = L"Range: bytes=0-1024\r\n";
                    if (WinHttpSendRequest(hRequest, rangeHdr, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                        WinHttpReceiveResponse(hRequest, NULL)) {
                        statusCode = 0;
                        dwSize = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest,
                                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                            WINHTTP_HEADER_NAME_BY_INDEX,
                                            &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
                        res.statusCode = (int)statusCode;
                    }
                }
            }

            // Content-Length
            DWORD contentLength = 0;
            dwSize = sizeof(contentLength);
            if (WinHttpQueryHeaders(hRequest,
                                    WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    &contentLength, &dwSize, WINHTTP_NO_HEADER_INDEX)) {
                res.fileSizeMB = (float)contentLength / (1024.0f * 1024.0f);
            }

            // Server Header
            WCHAR serverBuf[128] = {};
            DWORD serverSize = sizeof(serverBuf);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SERVER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    serverBuf, &serverSize, WINHTTP_NO_HEADER_INDEX)) {
                res.serverHeader = WStringToUtf8(serverBuf);
            }

            if (res.statusCode == 200 || res.statusCode == 206) {
                res.success = true;
            } else if (res.statusCode == 404) {
                res.success  = false;
                res.errorMsg = "Archivo no encontrado (HTTP 404).";
            } else if (res.statusCode == 403) {
                res.success  = false;
                res.errorMsg = "Acceso denegado (HTTP 403).";
            } else {
                res.success  = false;
                res.errorMsg = "HTTP " + std::to_string(res.statusCode);
            }
        } else {
            res.success  = false;
            res.errorMsg = "No se pudo recibir respuesta de Cloudflare.";
        }
        WinHttpCloseHandle(hRequest);
    }

    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
#else
    std::string cmd = "curl -s -I -L -m 4 \"" + fullUrl + "\"";
    FILE* p = popen(cmd.c_str(), "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof(line), p)) {
            std::string l(line);
            if (l.find("HTTP/") == 0) {
                int sc = 0;
                if (sscanf(l.c_str(), "HTTP/%*s %d", &sc) == 1) res.statusCode = sc;
            }
            std::string lower = l;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("content-length:") != std::string::npos) {
                auto colon = l.find(':');
                if (colon != std::string::npos) {
                    try {
                        float bytes = std::stof(l.substr(colon + 1));
                        res.fileSizeMB = bytes / (1024.0f * 1024.0f);
                    } catch (...) {}
                }
            }
        }
        pclose(p);
        res.success = (res.statusCode == 200 || res.statusCode == 206);
        if (!res.success) res.errorMsg = "HTTP " + std::to_string(res.statusCode);
    } else {
        res.success  = false;
        res.errorMsg = "Error al ejecutar curl.";
    }
#endif

    auto tEnd = std::chrono::steady_clock::now();
    res.latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
    return res;
}

void CloudflareUpdater::ProbeExecutableAsync(const std::string& baseUrl, const std::string& version,
                                             std::function<void(const CloudflareProbeResult& result)> onComplete) {
    std::string base = NormalizeBaseUrl(baseUrl);
    std::string filename = GetExeFilename(version);
    AppLog::Inf("Cloudflare", "Iniciando sondeo de " + filename + " en " + base);

    std::thread([base, version, onComplete]() {
        CloudflareProbeResult res = ProbeExecutableSync(base, version);
        if (res.success) {
            AppLog::Inf("Cloudflare", "Detectado con éxito: " + res.detectedUrl + " (" +
                        std::to_string(res.fileSizeMB).substr(0, 4) + " MB, HTTP " + std::to_string(res.statusCode) + ")");
        } else {
            AppLog::Warn("Cloudflare", "Sondeo de " + version + " finalizado: " + res.errorMsg);
        }
        if (onComplete) onComplete(res);
    }).detach();
}

void CloudflareUpdater::FindHighestAvailableUpdateAsync(
    const std::string& baseUrl, const std::string& currentVersion,
    std::function<void(bool found, const std::string& highestVer,
                       const std::string& downloadUrl, float sizeMB,
                       const std::vector<std::string>& allFound)> onComplete) {
    std::string base = NormalizeBaseUrl(baseUrl);
    if (base.empty()) {
        if (onComplete) onComplete(false, "", "", 0.0f, {});
        return;
    }

    std::thread([base, currentVersion, onComplete]() {
        std::vector<std::string> candidates = GetCandidateVersions(currentVersion);
        std::vector<std::string> foundVersions;
        std::string highestVer;
        std::string highestUrl;
        float highestSize = 0.0f;
        std::mutex mtx;

        // Sondeamos candidatos en lotes concurrentes para máxima velocidad de respuesta
        const size_t batchSize = 6;
        for (size_t i = 0; i < candidates.size(); i += batchSize) {
            size_t end = std::min(i + batchSize, candidates.size());
            std::vector<std::thread> workers;

            for (size_t j = i; j < end; ++j) {
                const std::string cand = candidates[j];
                workers.emplace_back([base, cand, &highestVer, &highestUrl, &highestSize, &foundVersions, &mtx]() {
                    CloudflareProbeResult r = ProbeExecutableSync(base, cand);
                    if (r.success) {
                        std::lock_guard<std::mutex> lock(mtx);
                        foundVersions.push_back(cand);
                        if (highestVer.empty() || IsVersionNewer(highestVer, cand)) {
                            highestVer = cand;
                            highestUrl = r.detectedUrl;
                            highestSize = r.fileSizeMB;
                        }
                    }
                });
            }

            for (auto& w : workers) {
                if (w.joinable()) w.join();
            }
        }

        bool found = !highestVer.empty();
        if (found) {
            AppLog::Inf("Updater", "Versión más alta detectada en Cloudflare: v" + highestVer + " (" +
                        std::to_string(highestSize).substr(0, 4) + " MB)");
        }
        if (onComplete) {
            onComplete(found, highestVer, highestUrl, highestSize, foundVersions);
        }
    }).detach();
}

void CloudflareUpdater::CheckForUpdateAsync(const std::string& baseUrl, const std::string& currentVersion,
                                            std::function<void(bool updateAvailable, const std::string& newVer,
                                                               const std::string& downloadUrl, const std::string& msg)> onComplete) {
    std::string base = NormalizeBaseUrl(baseUrl);
    if (base.empty()) {
        if (onComplete) onComplete(false, "", "", "URL de Cloudflare no configurada.");
        return;
    }

    AppLog::Inf("Updater", "Escaneando repositorio Cloudflare para actualizaciones superiores a v" + currentVersion);

    FindHighestAvailableUpdateAsync(base, currentVersion, [onComplete](bool found, const std::string& highestVer,
                                                                        const std::string& downloadUrl, float sizeMB,
                                                                        const std::vector<std::string>& allFound) {
        if (found) {
            std::string msg = "Nueva versión v" + highestVer + " detectada en Cloudflare (" +
                              std::to_string(sizeMB).substr(0, 4) + " MB).";
            if (onComplete) onComplete(true, highestVer, downloadUrl, msg);
        } else {
            if (onComplete) onComplete(false, "", "", "No se encontraron versiones superiores en Cloudflare.");
        }
    });
}

void CloudflareUpdater::StartDownload(const std::string& url, const std::string& targetVersion, bool launchInstaller,
                                      std::function<void(bool success, const std::string& pathOrErr)> onFinished) {
    if (m_DownloadState.isDownloading.exchange(true)) {
        AppLog::Warn("Updater", "Ya hay una descarga en progreso.");
        return;
    }

    m_DownloadState.progress         = 0.0f;
    m_DownloadState.downloadedMB     = 0.0f;
    m_DownloadState.totalMB          = 0.0f;
    m_DownloadState.speedMBs         = 0.0f;
    m_DownloadState.cancelRequested  = false;
    m_DownloadState.currentFilename  = GetExeFilename(targetVersion);
    m_DownloadState.errorMsg.clear();

    AppLog::Inf("Updater", "Iniciando descarga de " + url);

    std::thread([this, url, launchInstaller, onFinished]() {
        bool success = false;
        std::string finalPath;
        std::string errMsg;

#if defined(_WIN32)
        std::wstring wUrl(url.begin(), url.end());
        std::wstring host, path;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        bool isHttps = true;
        CrackUrlHelper(wUrl, host, path, port, isHttps);

        HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host.c_str(), port, 0) : NULL;
        DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                           NULL, WINHTTP_NO_REFERER,
                                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                           flags) : NULL;

        if (hRequest) {
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, NULL)) {

                DWORD contentLength = 0;
                DWORD cbSize = sizeof(contentLength);
                WinHttpQueryHeaders(hRequest,
                                    WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    &contentLength, &cbSize, WINHTTP_NO_HEADER_INDEX);

                float totalBytes = (float)contentLength;
                m_DownloadState.totalMB = totalBytes / (1024.0f * 1024.0f);

                char tempDir[MAX_PATH];
                GetTempPathA(MAX_PATH, tempDir);
                finalPath = std::string(tempDir) + m_DownloadState.currentFilename;
                m_DownloadState.savedFilePath = finalPath;

                std::ofstream outFile(finalPath, std::ios::binary);
                float downloadedBytes = 0.0f;
                auto lastTime = std::chrono::steady_clock::now();
                float bytesSinceLast = 0.0f;

                DWORD size = 0;
                do {
                    if (m_DownloadState.cancelRequested.load()) {
                        errMsg = "Descarga cancelada por el usuario.";
                        break;
                    }

                    if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
                    if (size == 0) break;

                    std::vector<char> buf(size);
                    DWORD read = 0;
                    if (WinHttpReadData(hRequest, buf.data(), size, &read)) {
                        outFile.write(buf.data(), read);
                        downloadedBytes += (float)read;
                        bytesSinceLast  += (float)read;

                        m_DownloadState.downloadedMB = downloadedBytes / (1024.0f * 1024.0f);
                        if (totalBytes > 0.0f) {
                            m_DownloadState.progress = downloadedBytes / totalBytes;
                        }

                        auto now = std::chrono::steady_clock::now();
                        float sec = std::chrono::duration<float>(now - lastTime).count();
                        if (sec >= 0.25f) {
                            m_DownloadState.speedMBs = (bytesSinceLast / (1024.0f * 1024.0f)) / sec;
                            bytesSinceLast = 0.0f;
                            lastTime = now;
                        }
                    }
                } while (size > 0);

                outFile.close();

                if (!m_DownloadState.cancelRequested.load() && (totalBytes <= 0.0f || downloadedBytes >= totalBytes * 0.95f)) {
                    success = true;
                    m_DownloadState.progress = 1.0f;
                    AppLog::Inf("Updater", "Descarga completada con éxito: " + finalPath);
                } else if (!m_DownloadState.cancelRequested.load()) {
                    errMsg = "Descarga incompleta o conexión interrumpida.";
                    AppLog::Err("Updater", errMsg);
                }
            } else {
                errMsg = "Fallo al enviar solicitud de descarga.";
                AppLog::Err("Updater", errMsg);
            }
            WinHttpCloseHandle(hRequest);
        } else {
            errMsg = "Error al abrir conexión de red con el host.";
            AppLog::Err("Updater", errMsg);
        }

        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
#endif

        m_DownloadState.isDownloading = false;
        m_DownloadState.errorMsg      = errMsg;

        if (success && launchInstaller) {
            AppLog::Inf("Updater", "Iniciando instalador: " + finalPath);
            LaunchInstallerAndExit(finalPath);
        }

        if (onFinished) {
            onFinished(success, success ? finalPath : errMsg);
        }
    }).detach();
}

void CloudflareUpdater::CancelDownload() {
    m_DownloadState.cancelRequested.store(true);
}

bool CloudflareUpdater::LaunchInstallerAndExit(const std::string& installerPath) {
#if defined(_WIN32)
    HINSTANCE hInst = ShellExecuteA(NULL, "open", installerPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)hInst > 32) {
        exit(0);
        return true;
    }
    return false;
#else
    std::string cmd = "chmod +x \"" + installerPath + "\" && \"" + installerPath + "\" &";
    system(cmd.c_str());
    exit(0);
    return true;
#endif
}

} // namespace ProyecThor::Core
