#include "MuninCloudManager.h"
#include "core/AppPaths.h"
#include "core/MuninDRM.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/layers/LayersBgTab.h"
#include "ui/panels/biblio/LibraryMultimedia.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ProyecThor::Core {

static std::string GetMuninCachePath() {
    return GetAssetsPath() + "/munin_catalog.json";
}

MuninCloudManager& MuninCloudManager::Get() {
    static MuninCloudManager s_Instance;
    return s_Instance;
}

MuninCloudManager::MuninCloudManager() {
    // URL pública oficial en Cloudflare R2 (R2.dev)
    m_CatalogUrl = "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/catalog.json";
    Initialize();
}

void MuninCloudManager::Initialize() {
    MuninDRM::SyncWithFirebaseAsync();
    LoadCachedCatalog();
    if (m_Resources.empty()) {
        PopulateDefaultFallbackCatalog();
    }
}

std::string MuninCloudManager::GetCatalogUrl() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CatalogUrl;
}

void MuninCloudManager::SetCatalogUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CatalogUrl = url;
}

std::string MuninCloudManager::GetStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_StatusMessage;
}

bool MuninCloudManager::DownloadFileSilent(const std::string& url,
                                           const std::string& destPath,
                                           std::function<void(float progress)> onProgress) {
    if (url.empty() || destPath.empty()) return false;

#ifdef _WIN32
    std::wstring wUrl(url.begin(), url.end());

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    bool winHttpSuccess = false;
    if (WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        if (urlComp.dwExtraInfoLength > 0 && urlComp.lpszExtraInfo) {
            path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        }
        INTERNET_PORT port = urlComp.nPort;
        bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        if (port == 0) {
            port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        }

        HINTERNET hSession = WinHttpOpen(L"ProyecThor-MuninCloud/1.2",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 45000);

            HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
            if (hConnect) {
                DWORD reqFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                       NULL, WINHTTP_NO_REFERER,
                                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                       reqFlags);
                if (hRequest) {
                    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
                    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

                    if (isHttps) {
                        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                    }

                    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                        WinHttpReceiveResponse(hRequest, NULL)) {

                        DWORD statusCode = 0;
                        DWORD statusSize = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

                        if (statusCode >= 200 && statusCode < 300) {
                            DWORD contentLength = 0;
                            DWORD clSize = sizeof(contentLength);
                            bool hasLength = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                                                 WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize, WINHTTP_NO_HEADER_INDEX) && (contentLength > 0);

                            try {
                                fs::path p(destPath);
                                if (p.has_parent_path() && !fs::exists(p.parent_path())) {
                                    fs::create_directories(p.parent_path());
                                }
                            } catch (...) {}

                            std::ofstream outFile(destPath, std::ios::binary | std::ios::trunc);
                            if (outFile.is_open()) {
                                DWORD downloadedBytes = 0;
                                DWORD bytesAvailable = 0;
                                std::vector<char> buffer(64 * 1024);

                                while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                                    DWORD read = 0;
                                    DWORD toRead = std::min((DWORD)buffer.size(), bytesAvailable);
                                    if (WinHttpReadData(hRequest, buffer.data(), toRead, &read) && read > 0) {
                                        outFile.write(buffer.data(), read);
                                        downloadedBytes += read;

                                        if (onProgress && hasLength && contentLength > 0) {
                                            float p = (float)downloadedBytes / (float)contentLength;
                                            if (p > 1.0f) p = 1.0f;
                                            onProgress(p);
                                        }
                                    } else {
                                        break;
                                    }
                                }
                                outFile.close();

                                if (fs::exists(destPath) && fs::file_size(destPath) > 0) {
                                    winHttpSuccess = true;
                                    if (onProgress) onProgress(1.0f);
                                }
                            }
                        }
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }
    }

    if (winHttpSuccess) {
        return true;
    }

    // Fallback silencioso con curl.exe usando CREATE_NO_WINDOW (sin consola jamás)
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdW = L"curl.exe -s -L -m 35 -o \"" + std::wstring(destPath.begin(), destPath.end()) + L"\" \"" + wUrl + L"\"";
    std::vector<wchar_t> cmdBuf(cmdW.begin(), cmdW.end());
    cmdBuf.push_back(0);

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 40000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        bool ok = (exitCode == 0 && fs::exists(destPath) && fs::file_size(destPath) > 0);
        if (ok && onProgress) onProgress(1.0f);
        return ok;
    }

    return false;
#else
    std::string cmd = "curl -s -L -o \"" + destPath + "\" \"" + url + "\"";
    int res = std::system(cmd.c_str());
    bool ok = (res == 0 && fs::exists(destPath) && fs::file_size(destPath) > 0);
    if (ok && onProgress) onProgress(1.0f);
    return ok;
#endif
}

void MuninCloudManager::RefreshCatalog(std::function<void(bool success, const std::string& msg)> onDone) {
    if (m_IsBusy.load()) {
        if (onDone) onDone(false, "Actualización en curso");
        return;
    }

    m_IsBusy.store(true);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_StatusMessage = "Conectando con MuninCloud (Cloudflare R2)...";
    }

    std::string url = GetCatalogUrl();
    std::thread([this, url, onDone]() {
        std::string tempPath = GetAssetsPath() + "/munin_temp.json";

        // Descarga silenciosa mediante WinHTTP / CREATE_NO_WINDOW (sin ventana CMD)
        bool dlOk = DownloadFileSilent(url, tempPath);

        bool success = false;
        std::string msg;

        if (dlOk && fs::exists(tempPath) && fs::file_size(tempPath) > 10) {
            try {
                std::ifstream f(tempPath);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                f.close();

                json root = json::parse(content);
                std::vector<MuninResource> newItems;

                auto parseList = [&](const json& arr) {
                    for (const auto& item : arr) {
                        MuninResource r;
                        r.id = item.value("id", "");
                        r.title = item.value("title", "Sin título");
                        r.author = item.value("author", "ProyecThor Cloud");
                        r.sizeStr = item.value("size", "0 MB");
                        r.downloadUrl = item.value("download_url", "");
                        r.thumbnailUrl = item.value("thumbnail_url", "");
                        r.filename = item.value("filename", "");
                        r.description = item.value("description", "");

                        std::string tStr = item.value("type", "video");
                        r.typeStr = tStr;
                        if (tStr == "video") r.type = MuninResourceType::Video;
                        else if (tStr == "image") r.type = MuninResourceType::Image;
                        else if (tStr == "bible") r.type = MuninResourceType::Bible;
                        else if (tStr == "song") r.type = MuninResourceType::Song;
                        else r.type = MuninResourceType::Video;

                        if (r.filename.empty() && !r.downloadUrl.empty()) {
                            auto pos = r.downloadUrl.rfind('/');
                            if (pos != std::string::npos) r.filename = r.downloadUrl.substr(pos + 1);
                        }

                        if (item.contains("tags") && item["tags"].is_array()) {
                            for (const auto& tag : item["tags"]) {
                                if (tag.is_string()) r.tags.push_back(tag.get<std::string>());
                            }
                        }

                        if (!r.id.empty() && !r.downloadUrl.empty()) {
                            newItems.push_back(std::move(r));
                        }
                    }
                };

                if (root.contains("resources") && root["resources"].is_array()) {
                    parseList(root["resources"]);
                } else if (root.is_array()) {
                    parseList(root);
                }

                if (!newItems.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(m_Mutex);
                        m_Resources = std::move(newItems);
                        m_StatusMessage = "Catálogo MuninCloud actualizado.";
                    }
                    SaveCachedCatalog(content);
                    success = true;
                    msg = "Catálogo sincronizado exitosamente (" + std::to_string(m_Resources.size()) + " recursos).";
                } else {
                    msg = "El catálogo en la nube no contiene recursos válidos.";
                }
            } catch (const std::exception& e) {
                msg = std::string("Error al procesar catálogo: ") + e.what();
            }
        } else {
            msg = "No se pudo conectar a Cloudflare R2 (comprueba la URL pública o tu conexión).";
        }

        std::error_code ec;
        fs::remove(tempPath, ec);

        m_IsBusy.store(false);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_StatusMessage = msg;
        }

        if (onDone) onDone(success, msg);
    }).detach();
}

std::vector<MuninResource> MuninCloudManager::GetResources(MuninResourceType filterType, const std::string& search) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<MuninResource> filtered;

    std::string sLower = search;
    std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

    for (const auto& r : m_Resources) {
        if (filterType != MuninResourceType::All && r.type != filterType) {
            continue;
        }

        if (!sLower.empty()) {
            std::string titleLower = r.title;
            std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
            std::string authorLower = r.author;
            std::transform(authorLower.begin(), authorLower.end(), authorLower.begin(), ::tolower);

            bool matchTag = false;
            for (const auto& tag : r.tags) {
                std::string tLower = tag;
                std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
                if (tLower.find(sLower) != std::string::npos) {
                    matchTag = true;
                    break;
                }
            }

            if (titleLower.find(sLower) == std::string::npos &&
                authorLower.find(sLower) == std::string::npos &&
                !matchTag) {
                continue;
            }
        }

        filtered.push_back(r);
    }
    return filtered;
}

std::string MuninCloudManager::ResolveTargetPath(const MuninResource& res) const {
    std::string dir;
    switch (res.type) {
        case MuninResourceType::Video:
            dir = ProyecThor::Library::GetVideoLibraryPath();
            break;
        case MuninResourceType::Image:
            dir = GetAssetsPath() + "/backgrounds";
            break;
        case MuninResourceType::Bible:
            dir = GetAssetsPath() + "/bibles";
            break;
        case MuninResourceType::Song:
            dir = GetAssetsPath() + "/songs";
            break;
        default:
            dir = GetAssetsPath() + "/backgrounds";
            break;
    }

    std::string fn = res.filename.empty() ? (res.id + ".dat") : res.filename;
    return dir + "/" + fn;
}

bool MuninCloudManager::IsResourceInstalled(const MuninResource& res) const {
    std::string target = ResolveTargetPath(res);
    return fs::exists(target);
}

bool MuninCloudManager::IsDownloading(const std::string& resId) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_ActiveDownloads.find(resId);
    return (it != m_ActiveDownloads.end() && it->second);
}

float MuninCloudManager::GetDownloadProgress(const std::string& resId) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_DownloadProgress.find(resId);
    return (it != m_DownloadProgress.end()) ? it->second : 0.0f;
}

void MuninCloudManager::DownloadResource(const MuninResource& res,
                                        std::function<void(bool success, const std::string& msg)> onDone) {
    if (IsDownloading(res.id)) {
        if (onDone) onDone(false, "Descarga en curso");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_ActiveDownloads[res.id] = true;
        m_DownloadProgress[res.id] = 0.05f;
    }

    std::string targetPath = ResolveTargetPath(res);
    std::string url = res.downloadUrl;
    std::string id = res.id;

    std::thread([this, id, url, targetPath, res, onDone]() {
        try {
            fs::path parent = fs::path(targetPath).parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                fs::create_directories(parent);
            }
        } catch (...) {}

        std::string tempPart = targetPath + ".part";
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_DownloadProgress[id] = 0.05f;
        }

        // Descarga silenciosa sin ventanas de comando
        bool dlOk = DownloadFileSilent(url, tempPart, [this, id](float p) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_DownloadProgress[id] = std::max(0.05f, std::min(0.95f, p));
        });

        bool ok = false;
        std::string msg;

        if (dlOk && fs::exists(tempPart) && fs::file_size(tempPart) > 0) {
            std::error_code ec;

            if (res.type == MuninResourceType::Image) {
                // ── Encriptar el fondo con MuninDRM para proteger contra copia externa ──
                bool encOk = MuninDRM::EncryptFile(tempPart, targetPath);
                fs::remove(tempPart, ec);

                if (encOk) {
                    ok = true;
                    msg = "Fondo descargado y protegido con DRM en tu librería.";

                    std::string altDir = GetAssetsPath() + "/images";
                    try {
                        fs::create_directories(altDir);
                        std::error_code cpec;
                        fs::copy_file(targetPath, altDir + "/" + (res.filename.empty() ? (res.id + ".jpg") : res.filename),
                                      fs::copy_options::overwrite_existing, cpec);
                    } catch (...) {}

                    ProyecThor::UI::BumpBackgroundsListRevision();
                    ProyecThor::Library::RefreshMultimediaLists();
                } else {
                    msg = "Error al encriptar el fondo para almacenamiento seguro.";
                }
            } else {
                fs::rename(tempPart, targetPath, ec);
                if (!ec) {
                    ok = true;
                    msg = "Recurso descargado e instalado con éxito.";
                    if (res.type == MuninResourceType::Video) {
                        ProyecThor::Library::RefreshMultimediaLists();
                    }
                } else {
                    msg = "Descargado, pero error al mover archivo: " + ec.message();
                }
            }
        } else {
            msg = "Error en la descarga desde Cloudflare R2.";
            std::error_code ec;
            fs::remove(tempPart, ec);
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ActiveDownloads[id] = false;
            m_DownloadProgress[id] = ok ? 1.0f : 0.0f;
        }

        if (onDone) onDone(ok, msg);
    }).detach();
}

void MuninCloudManager::LoadCachedCatalog() {
    std::string path = GetMuninCachePath();
    if (!fs::exists(path)) return;

    try {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        json root = json::parse(content);

        std::vector<MuninResource> list;
        auto parse = [&](const json& arr) {
            for (const auto& item : arr) {
                MuninResource r;
                r.id = item.value("id", "");
                r.title = item.value("title", "");
                r.author = item.value("author", "");
                r.sizeStr = item.value("size", "");
                r.downloadUrl = item.value("download_url", "");
                r.thumbnailUrl = item.value("thumbnail_url", "");
                r.filename = item.value("filename", "");
                r.description = item.value("description", "");

                std::string tStr = item.value("type", "video");
                std::transform(tStr.begin(), tStr.end(), tStr.begin(), ::tolower);
                r.typeStr = tStr;
                if (tStr == "video") r.type = MuninResourceType::Video;
                else if (tStr == "image" || tStr == "background" || tStr == "fondo") r.type = MuninResourceType::Image;
                else if (tStr == "bible" || tStr == "biblia") r.type = MuninResourceType::Bible;
                else if (tStr == "song" || tStr == "cancion") r.type = MuninResourceType::Song;

                // Corregir URL si apunta al ID anterior de bucket
                const std::string oldDomain = "pub-dd833744e9f937b3d18fa6004aa612d1.r2.dev";
                const std::string newDomain = "pub-e93b87deb084472fab2cf57e514c0a49.r2.dev";
                size_t pos = r.downloadUrl.find(oldDomain);
                if (pos != std::string::npos) {
                    r.downloadUrl.replace(pos, oldDomain.length(), newDomain);
                }
                pos = r.thumbnailUrl.find(oldDomain);
                if (pos != std::string::npos) {
                    r.thumbnailUrl.replace(pos, oldDomain.length(), newDomain);
                }

                if (item.contains("tags") && item["tags"].is_array()) {
                    for (const auto& tag : item["tags"]) {
                        if (tag.is_string()) r.tags.push_back(tag.get<std::string>());
                    }
                }
                if (!r.id.empty()) list.push_back(std::move(r));
            }
        };

        if (root.contains("resources") && root["resources"].is_array()) {
            parse(root["resources"]);
        } else if (root.is_array()) {
            parse(root);
        }

        if (!list.empty()) {
            m_Resources = std::move(list);
        }
    } catch (...) {}
}

void MuninCloudManager::SaveCachedCatalog(const std::string& rawJson) {
    try {
        std::ofstream f(GetMuninCachePath(), std::ios::trunc);
        if (f.is_open()) {
            f << rawJson;
        }
    } catch (...) {}
}

void MuninCloudManager::PopulateDefaultFallbackCatalog() {
    m_Resources = {
        // ── Fondos de Imagen Oficiales en R2 ──
        {
            "img_coloursgrain",
            MuninResourceType::Image,
            "background",
            "Colours Grain",
            "ProyecThor Studio",
            "181 KB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/coloursgrain.jpg",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/coloursgrain.jpg",
            "coloursgrain.jpg",
            { "fondo", "gradiente", "color", "textura" },
            "Fondo texturizado con grano y mezcla de colores vivos para proyección."
        },
        {
            "img_midnight",
            MuninResourceType::Image,
            "background",
            "Midnight Gradient",
            "ProyecThor Studio",
            "40 KB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/midnight.jpg",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/midnight.jpg",
            "midnight.jpg",
            { "medianoche", "oscuro", "minimal", "fondo" },
            "Fondo oscuro elegante estilo noche profunda para versículos y lecturas bíblicas."
        },
        {
            "img_moon",
            MuninResourceType::Image,
            "background",
            "Moon & Night Sky",
            "ProyecThor Studio",
            "121 KB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/moon.jpg",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/moon.jpg",
            "moon.jpg",
            { "luna", "noche", "cielo", "espacio" },
            "Fondo nocturno con luna llena y cielo estrellado en alta definición."
        },
        {
            "img_newyearchile",
            MuninResourceType::Image,
            "background",
            "New Year Celebration",
            "ProyecThor Studio",
            "230 KB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/newyearchile.jpg",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/newyearchile.jpg",
            "newyearchile.jpg",
            { "celebracion", "luces", "fiesta", "chile" },
            "Fondo festivo con luces y ambientación para eventos y reuniones especiales."
        },
        {
            "img_rencacloud",
            MuninResourceType::Image,
            "background",
            "Renca Cloud Sky",
            "ProyecThor Studio",
            "379 KB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/rencacloud.jpg",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/backgrounds/rencacloud.jpg",
            "rencacloud.jpg",
            { "nubes", "cielo", "atardecer", "naturaleza" },
            "Cielo panorámico con formaciones de nubes cálidas y luz suave."
        },
        // ── 24 Biblias Oficiales en Cloudflare R2 ──
        {
            "bible_1569",
            MuninResourceType::Bible,
            "bible",
            "Biblia del Oso 1569",
            "Casiodoro de Reina",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/Spanish1569Bible.xml",
            "",
            "Spanish1569Bible.xml",
            { "biblia", "1569", "oso", "casiodoro", "historica" },
            "La histórica primera traducción completa de la Biblia al castellano (1569)."
        },
        {
            "bible_nt1858",
            MuninResourceType::Bible,
            "bible",
            "Nuevo Testamento 1858",
            "Sociedad Bíblica Americana",
            "1.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/Spanish1858Bible.xml",
            "",
            "Spanish1858Bible.xml",
            { "biblia", "nt1858", "nuevo testamento", "historica" },
            "Versión histórica del Nuevo Testamento publicada en 1858."
        },
        {
            "bible_rva1989",
            MuninResourceType::Bible,
            "bible",
            "Reina Valera Actualizada 1989 (RVA1989)",
            "Editorial Mundo Hispano",
            "5.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/Spanish1989Bible.xml",
            "",
            "Spanish1989Bible.xml",
            { "biblia", "rva", "1989", "mundo hispano" },
            "Edición clásica de la Reina Valera Actualizada de 1989."
        },
        {
            "bible_bdo",
            MuninResourceType::Bible,
            "bible",
            "Biblia del Oso (BDO)",
            "Casiodoro de Reina",
            "4.9 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBDOBible.xml",
            "",
            "SpanishBDOBible.xml",
            { "biblia", "bdo", "oso", "clasica" },
            "Texto de la venerada Biblia del Oso con ortografía adaptada para lectura fluida."
        },
        {
            "bible_bhti",
            MuninResourceType::Bible,
            "bible",
            "La Biblia Hispanoamericana (BHTI)",
            "Sociedad Bíblica de España",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBHTIBible.xml",
            "",
            "SpanishBHTIBible.xml",
            { "biblia", "bhti", "hispanoamericana", "traduccion" },
            "Traducción interconfesional en lenguaje hispanoamericano actual y preciso."
        },
        {
            "bible_blp",
            MuninResourceType::Bible,
            "bible",
            "La Palabra — Edición España (BLP)",
            "Sociedad Bíblica de España",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBLPBible.xml",
            "",
            "SpanishBLPBible.xml",
            { "biblia", "blp", "la palabra", "espana" },
            "Traducción en lenguaje dinámico y fiel adaptada al español peninsular."
        },
        {
            "bible_blph",
            MuninResourceType::Bible,
            "bible",
            "La Palabra — Edición Hispanoamérica (BLPH)",
            "Sociedad Bíblica",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBLPHBible.xml",
            "",
            "SpanishBLPHBible.xml",
            { "biblia", "blph", "la palabra", "latinoamerica" },
            "Traducción dinámica en español contemporáneo para América Latina."
        },
        {
            "bible_bti",
            MuninResourceType::Bible,
            "bible",
            "Biblia Traducción Interconfesional (BTI)",
            "BAC / Sociedades Bíblicas Unidas",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBTIBible.xml",
            "",
            "SpanishBTIBible.xml",
            { "biblia", "bti", "interconfesional", "ecumenica" },
            "Edición rigurosa ecuménica de gran valor filológico y literario."
        },
        {
            "bible_btx",
            MuninResourceType::Bible,
            "bible",
            "Biblia Textual IV Edición (BTX)",
            "Sociedad Bíblica Iberoamericana",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBTXBible.xml",
            "",
            "SpanishBTXBible.xml",
            { "biblia", "btx", "textual", "critica" },
            "Traducción contextual rigurosa basada en el texto crítico de los idiomas originales."
        },
        {
            "bible_rvr_clasica",
            MuninResourceType::Bible,
            "bible",
            "Santa Biblia Clásica (RVR)",
            "Sociedades Bíblicas",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishBible.xml",
            "",
            "SpanishBible.xml",
            { "biblia", "rvr", "clasica", "canonica" },
            "Texto canónico de la Santa Biblia en la tradición tradicional hispana."
        },
        {
            "bible_dhh",
            MuninResourceType::Bible,
            "bible",
            "Dios Habla Hoy (DHH)",
            "Sociedades Bíblicas Unidas",
            "5.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishDHHBible.xml",
            "",
            "SpanishDHHBible.xml",
            { "biblia", "dhh", "popular", "sbu" },
            "Versión en lenguaje popular y directo de fácil comprensión para todas las edades."
        },
        {
            "bible_jbs",
            MuninResourceType::Bible,
            "bible",
            "Jubilee Bible 2000 (JBS)",
            "Life Sentence Publishing",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishJBSBible.xml",
            "",
            "SpanishJBSBible.xml",
            { "biblia", "jbs", "jubileo", "2000" },
            "Traducción del jubileo siguiendo el uso sistemático de las raíces hebreas y griegas."
        },
        {
            "bible_lbla",
            MuninResourceType::Bible,
            "bible",
            "La Biblia de las Américas (LBLA)",
            "The Lockman Foundation",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishLBLABible.xml",
            "",
            "SpanishLBLABible.xml",
            { "biblia", "lbla", "lockman", "estudio" },
            "Traducción formal y precisa palabra por palabra desde los idiomas originales."
        },
        {
            "bible_nbla",
            MuninResourceType::Bible,
            "bible",
            "Nueva Biblia de las Américas (NBLA)",
            "The Lockman Foundation",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNBLABible.xml",
            "",
            "SpanishNBLABible.xml",
            { "biblia", "nbla", "moderna", "lockman" },
            "Actualización moderna de LBLA en español latinoamericano claro y actual."
        },
        {
            "bible_nbv",
            MuninResourceType::Bible,
            "bible",
            "Nueva Biblia Viva (NBV)",
            "Biblica Inc.",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNBVBible.xml",
            "",
            "SpanishNBVBible.xml",
            { "biblia", "nbv", "viva", "parafrasis" },
            "Paráfrasis moderna en lenguaje dinámico, cercano y de fácil lectura."
        },
        {
            "bible_ntbiz",
            MuninResourceType::Bible,
            "bible",
            "Nuevo Testamento Interlineal (NTBIZ)",
            "Sociedad Bíblica",
            "1.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNTBIZBible.xml",
            "",
            "SpanishNTBIZBible.xml",
            { "biblia", "ntbiz", "nuevo testamento", "estudio" },
            "Nuevo Testamento condensado de referencia para estudio y exégesis."
        },
        {
            "bible_ntv",
            MuninResourceType::Bible,
            "bible",
            "Nueva Traducción Viviente (NTV)",
            "Tyndale House Publishers",
            "5.4 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNTVBible.xml",
            "",
            "SpanishNTVBible.xml",
            { "biblia", "ntv", "tyndale", "contemporanea" },
            "Traducción contemporánea de gran belleza estilística y claridad conmovedora."
        },
        {
            "bible_nvi",
            MuninResourceType::Bible,
            "bible",
            "Nueva Versión Internacional (NVI)",
            "Biblica Inc.",
            "5.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNVIBible.xml",
            "",
            "SpanishNVIBible.xml",
            { "biblia", "nvi", "biblica", "oficial" },
            "Texto bíblico en español contemporáneo optimizado para predicación y proyección."
        },
        {
            "bible_nvis",
            MuninResourceType::Bible,
            "bible",
            "Nueva Versión Internacional Simplificada (NVIS)",
            "Biblica Inc.",
            "1.3 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishNVISBible.xml",
            "",
            "SpanishNVISBible.xml",
            { "biblia", "nvis", "simplificada", "ninos" },
            "Edición con vocabulario simplificado accesible para niños y nuevos creyentes."
        },
        {
            "bible_pdt",
            MuninResourceType::Bible,
            "bible",
            "Palabra de Dios para Todos (PDT)",
            "Centro Mundial de Traducción de la Biblia",
            "5.2 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishPDTBible.xml",
            "",
            "SpanishPDTBible.xml",
            { "biblia", "pdt", "sencilla", "clara" },
            "Versión en español natural que comunica el significado con máxima claridad."
        },
        {
            "bible_rv2020",
            MuninResourceType::Bible,
            "bible",
            "Reina Valera 2020 (RV2020)",
            "Sociedad Bíblica de España",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishRV2020Bible.xml",
            "",
            "SpanishRV2020Bible.xml",
            { "biblia", "rv2020", "espana", "actual" },
            "Revisión moderna y rigurosa del venerado texto clásico Reina Valera."
        },
        {
            "bible_rva2015",
            MuninResourceType::Bible,
            "bible",
            "Reina Valera Actualizada 2015 (RVA2015)",
            "Editorial Mundo Hispano",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishRVA2015Bible.xml",
            "",
            "SpanishRVA2015Bible.xml",
            { "biblia", "rva2015", "actualizada", "fiel" },
            "Texto clásico con léxico y sintaxis contemporánea, fiel a los originales."
        },
        {
            "bible_rvc",
            MuninResourceType::Bible,
            "bible",
            "Reina Valera Contemporánea (RVC)",
            "Sociedades Bíblicas Unidas",
            "5.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishRVCBible.xml",
            "",
            "SpanishRVCBible.xml",
            { "biblia", "rvc", "sbu", "contemporanea" },
            "El estilo y ritmo de la Reina Valera en el español de América Latina de hoy."
        },
        {
            "bible_rves",
            MuninResourceType::Bible,
            "bible",
            "Reina Valera 1909 (RVES)",
            "Sociedades Bíblicas Unidas",
            "5.0 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/bibles/SpanishRVESBible.xml",
            "",
            "SpanishRVESBible.xml",
            { "biblia", "1909", "rves", "clasica", "antigua" },
            "La venerada y solemne versión Reina Valera histórica de 1909."
        },
        // ── Videos en Bucle ──
        {
            "vid_particulas_doradas",
            MuninResourceType::Video,
            "video",
            "Partículas Doradas en Bucle",
            "ProyecThor Oficial",
            "18.4 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/videos/particulas_doradas.mp4",
            "",
            "particulas_doradas.mp4",
            { "alabanza", "dorado", "bucle", "particulas" },
            "Fondo de video dinámico con partículas de polvo estelar y destellos cálidos para adoración."
        },
        {
            "vid_cielo_estrellado",
            MuninResourceType::Video,
            "video",
            "Cielo Cósmico & Nebulosa",
            "ProyecThor Oficial",
            "24.1 MB",
            "https://pub-e93b87deb084472fab2cf57e514c0a49.r2.dev/videos/cielo_estrellado.mp4",
            "",
            "cielo_estrellado.mp4",
            { "universo", "azul", "estrellas", "motion" },
            "Movimiento celestial lento y relajante ideal para momentos de oración e introspección."
        }
    };
}

} // namespace ProyecThor::Core
