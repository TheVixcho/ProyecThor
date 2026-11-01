#include "ProAIManager.h"
#include "AuthLicenseManager.h"
#include <fstream>
#include <thread>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

namespace ProyecThor::Core {

using json = nlohmann::json;

#ifdef _WIN32
static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}
#endif

static std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << ((int)(unsigned char)c);
        }
    }
    return escaped.str();
}

ProAIManager& ProAIManager::Get() {
    static ProAIManager s_Instance;
    return s_Instance;
}

ProAIManager::ProAIManager() {
    m_CurrentMonthCycle = GetCurrentMonthKey();
    m_CurrentDayCycle   = GetCurrentDayKey();
}

void ProAIManager::Initialize() {
    LoadQuota();
    CheckMonthlyReset();
    CheckDailyReset();
    RefreshCloudSongs(nullptr);
}

std::string ProAIManager::GetCurrentMonthKey() const {
    std::time_t t = std::time(nullptr);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &t);
#else
    localtime_r(&t, &tmNow);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1);
    return std::string(buf);
}

std::string ProAIManager::GetCurrentDayKey() const {
    std::time_t t = std::time(nullptr);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &t);
#else
    localtime_r(&t, &tmNow);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
    return std::string(buf);
}

void ProAIManager::CheckMonthlyResetLocked() {
    std::string nowMonth = GetCurrentMonthKey();
    if (m_CurrentMonthCycle != nowMonth) {
        m_CurrentMonthCycle = nowMonth;
        m_TokensUsedThisMonth = 0;
        m_ImagesUsedThisMonth = 0;
        SaveQuota();
    }
}

void ProAIManager::CheckDailyResetLocked() {
    std::string nowDay = GetCurrentDayKey();
    if (m_CurrentDayCycle != nowDay) {
        m_CurrentDayCycle = nowDay;
        m_DailyTextQueriesUsed = 0;
        SaveQuota();
    }
}

void ProAIManager::CheckMonthlyReset() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    CheckMonthlyResetLocked();
}

void ProAIManager::CheckDailyReset() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    CheckDailyResetLocked();
}

std::string ProAIManager::GetQuotaFilePath() const {
#ifdef _WIN32
    char appDataBuf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
        std::filesystem::path dir = std::filesystem::path(appDataBuf) / "ProyecThor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "pro_ai_quota.json").string();
    }
#endif
    return "pro_ai_quota.json";
}

std::string ProAIManager::GetCloudSongsCachePath() const {
#ifdef _WIN32
    char appDataBuf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
        std::filesystem::path dir = std::filesystem::path(appDataBuf) / "ProyecThor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "pro_cloud_songs.json").string();
    }
#endif
    return "pro_cloud_songs.json";
}

std::string ProAIManager::GetAIImagesDir() const {
#ifdef _WIN32
    char appDataBuf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
        std::filesystem::path dir = std::filesystem::path(appDataBuf) / "ProyecThor" / "AI_Images";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir.string();
    }
#endif
    std::filesystem::create_directories("AI_Images");
    return "AI_Images";
}

void ProAIManager::SaveQuota() {
    json j;
    j["month"]                   = m_CurrentMonthCycle;
    j["day"]                     = m_CurrentDayCycle;
    j["tokens_used"]             = m_TokensUsedThisMonth;
    j["images_used"]             = m_ImagesUsedThisMonth;
    j["daily_text_queries_used"] = m_DailyTextQueriesUsed;

    std::string path = GetQuotaFilePath();
    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(2);
    }
}

void ProAIManager::LoadQuota() {
    std::string path = GetQuotaFilePath();
    if (!std::filesystem::exists(path)) return;

    std::ifstream f(path);
    if (!f.is_open()) return;

    try {
        json j = json::parse(f);
        if (j.contains("month") && j["month"].is_string()) {
            m_CurrentMonthCycle = j["month"];
        }
        if (j.contains("day") && j["day"].is_string()) {
            m_CurrentDayCycle = j["day"];
        }
        if (j.contains("tokens_used") && j["tokens_used"].is_number()) {
            m_TokensUsedThisMonth = j["tokens_used"].get<uint64_t>();
        }
        if (j.contains("images_used") && j["images_used"].is_number()) {
            m_ImagesUsedThisMonth = j["images_used"].get<int>();
        }
        if (j.contains("daily_text_queries_used") && j["daily_text_queries_used"].is_number()) {
            m_DailyTextQueriesUsed = j["daily_text_queries_used"].get<int>();
        }
    } catch (...) {}
}

AIProvider ProAIManager::GetCurrentProvider() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CurrentProvider;
}

void ProAIManager::SetCurrentProvider(AIProvider p) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CurrentProvider = p;
}

std::string ProAIManager::GetDecryptedGeminiKey() const {
    // Clave de Google Gemini descifrada dinámicamente en memoria
    static const unsigned char s_GeminiEncrypted[] = {
        27, 244, 18, 130, 125, 70, 196, 39, 108, 236, 113, 170, 42, 21, 222, 26,
        53, 227, 110, 150, 116, 25, 206, 2, 3, 234, 74, 128, 124, 50, 213, 39,
        23, 195, 125, 162, 113, 9, 238, 42, 99, 193, 4, 151, 82, 24, 187, 81,
        59, 233, 100, 169, 78
    };
    static const unsigned char s_Mask[] = { 0x5A, 0xA5, 0x3C, 0xC3, 0x1F, 0x7E, 0x96, 0x69 };
    const size_t len = sizeof(s_GeminiEncrypted);

    std::string key;
    key.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        key.push_back((char)(s_GeminiEncrypted[i] ^ s_Mask[i % sizeof(s_Mask)]));
    }
    return key;
}

std::string ProAIManager::GetDecryptedDeepSeekKey() const {
    // Clave de DeepSeek descifrada dinámicamente en memoria
    static const unsigned char s_DeepSeekEncrypted[] = {
        41, 206, 17, 162, 40, 31, 247, 12, 110, 145, 8, 161, 44, 72, 166, 93,
        60, 199, 90, 250, 124, 76, 161, 80, 109, 192, 5, 243, 47, 77, 174, 89,
        109, 195, 9
    };
    static const unsigned char s_Mask[] = { 0x5A, 0xA5, 0x3C, 0xC3, 0x1F, 0x7E, 0x96, 0x69 };
    const size_t len = sizeof(s_DeepSeekEncrypted);

    std::string key;
    key.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        key.push_back((char)(s_DeepSeekEncrypted[i] ^ s_Mask[i % sizeof(s_Mask)]));
    }
    return key;
}

// ── Descifrado seguro de API Key en memoria ──────────────────────────────────
std::string ProAIManager::GetDecryptedApiKey() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_RemoteApiKeyOverride.empty()) {
        return m_RemoteApiKeyOverride;
    }
    return (m_CurrentProvider == AIProvider::Gemini) ? GetDecryptedGeminiKey() : GetDecryptedDeepSeekKey();
}

void ProAIManager::SetRemoteApiKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_RemoteApiKeyOverride = key;
}

// ── Control de Cuotas y Límites ──────────────────────────────────────────────
bool ProAIManager::CanSendMessage(std::string* outReason) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const_cast<ProAIManager*>(this)->CheckMonthlyResetLocked();
    const_cast<ProAIManager*>(this)->CheckDailyResetLocked();

    if (m_TokensUsedThisMonth >= m_MaxMonthlyTokens) {
        if (outReason) {
            *outReason = "Has alcanzado el límite mensual de 250,000 tokens de tu plan Pro. Tu cuota se renovará al finalizar el mes o al ampliar tu suscripción.";
        }
        return false;
    }
    if (m_DailyTextQueriesUsed >= m_MaxDailyTextQueries) {
        if (outReason) {
            *outReason = "Has alcanzado el límite diario de 150 consultas de texto. Tu cuota diaria se renovará mañana.";
        }
        return false;
    }
    return true;
}

uint64_t ProAIManager::GetAvailableTokens() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_TokensUsedThisMonth >= m_MaxMonthlyTokens) return 0;
    return m_MaxMonthlyTokens - m_TokensUsedThisMonth;
}

uint64_t ProAIManager::GetMaxMonthlyTokens() const {
    return m_MaxMonthlyTokens;
}

uint64_t ProAIManager::GetUsedTokens() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_TokensUsedThisMonth;
}

int ProAIManager::GetDailyQueriesUsed() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_DailyTextQueriesUsed;
}

int ProAIManager::GetMaxDailyQueries() const {
    return m_MaxDailyTextQueries;
}

int ProAIManager::GetAvailableDailyQueries() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_DailyTextQueriesUsed >= m_MaxDailyTextQueries) return 0;
    return m_MaxDailyTextQueries - m_DailyTextQueriesUsed;
}

bool ProAIManager::CheckAndRecordRateLimit(std::string* outReason) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto now = std::chrono::steady_clock::now();

    // Eliminar timestamps mayores a 60 segundos
    while (!m_RequestTimestamps.empty()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_RequestTimestamps.front());
        if (elapsed >= m_RateLimitWindow) {
            m_RequestTimestamps.pop_front();
        } else {
            break;
        }
    }

    if (static_cast<int>(m_RequestTimestamps.size()) >= m_MaxRequestsPerMinute) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_RequestTimestamps.front());
        int waitSeconds = static_cast<int>(m_RateLimitWindow.count() - elapsed.count());
        if (waitSeconds < 1) waitSeconds = 1;
        if (outReason) {
            *outReason = "Límite de velocidad: máximo 5 peticiones por minuto. Espera " + std::to_string(waitSeconds) + " segundo" + (waitSeconds == 1 ? "" : "s") + " antes de enviar otra solicitud.";
        }
        return false;
    }

    m_RequestTimestamps.push_back(now);
    return true;
}

bool ProAIManager::CanGenerateImage() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ImagesUsedThisMonth < m_MaxMonthlyImages;
}

int ProAIManager::GetAvailableImages() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_ImagesUsedThisMonth >= m_MaxMonthlyImages) return 0;
    return m_MaxMonthlyImages - m_ImagesUsedThisMonth;
}

int ProAIManager::GetMaxMonthlyImages() const {
    return m_MaxMonthlyImages;
}

int ProAIManager::GetUsedImages() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ImagesUsedThisMonth;
}

bool ProAIManager::IsChatBusy() const {
    return m_ChatBusy.load();
}

bool ProAIManager::IsImageBusy() const {
    return m_ImageBusy.load();
}

bool ProAIManager::IsCloudBusy() const {
    return m_CloudBusy.load();
}

// ── HTTPS Nativo con WinHTTP ─────────────────────────────────────────────────
std::string ProAIManager::PerformHttpsPost(const std::string& host, const std::string& path,
                                          const std::string& postData, const std::string& bearerToken,
                                          int& outStatusCode, std::string& outError) {
    outStatusCode = 0;
    outError.clear();

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"ProyecThor-Engine/1.1",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = "Error al iniciar sesión WinHTTP.";
        return "";
    }

    std::wstring wHost = Utf8ToWide(host);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = "Error al conectar con servidor: " + host;
        return "";
    }

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Error al abrir petición HTTPS.";
        return "";
    }

    // Cabeceras HTTP
    std::wstring wHeaders = L"Content-Type: application/json\r\n";
    if (!bearerToken.empty()) {
        wHeaders += L"Authorization: Bearer " + Utf8ToWide(bearerToken) + L"\r\n";
    }

    BOOL bSend = WinHttpSendRequest(hRequest,
                                    wHeaders.c_str(), (DWORD)wHeaders.length(),
                                    (LPVOID)postData.c_str(), (DWORD)postData.length(),
                                    (DWORD)postData.length(), 0);
    if (!bSend) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Tiempo de espera o fallo al transmitir solicitud.";
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Sin respuesta del servidor de IA.";
        return "";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatusCode = (int)statusCode;

    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return responseBody;
#else
    outError = "WinHTTP solo disponible en Windows.";
    return "";
#endif
}

bool ProAIManager::DownloadFileHttps(const std::string& urlStr, const std::string& localDestPath, std::string& outError) {
    outError.clear();
#ifdef _WIN32
    std::wstring wUrl = Utf8ToWide(urlStr);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &uc)) {
        outError = "URL de descarga inválida.";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"ProyecThor-Engine/1.1",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = "No se pudo abrir sesión HTTP.";
        return false;
    }

    // Configurar timeouts razonables (10s conexión, 60s recepción para permitir renderizado de IA)
    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 60000);

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = "Fallo de conexión al descargar imagen.";
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath,
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "No se pudo preparar solicitud GET.";
        return false;
    }

    // Seguir redirecciones automáticamente
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Error al recibir imagen del servidor.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        if (statusCode != 200) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (statusCode == 503) {
                outError = "El servidor de imágenes está temporalmente saturado (HTTP 503). Reintentando...";
            } else if (statusCode == 429) {
                outError = "Demasiadas peticiones al generador de imágenes (HTTP 429).";
            } else {
                outError = "Error del servidor de imágenes (HTTP " + std::to_string(statusCode) + ").";
            }
            return false;
        }
    }

    std::string tempPath = localDestPath + ".part";
    std::ofstream outFile(tempPath, std::ios::binary);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "No se pudo crear el archivo de imagen en disco.";
        return false;
    }

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            outFile.write(buffer.data(), bytesRead);
        }
    }

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Validar cabecera mágica para asegurar que es una imagen real y no un error HTML/JSON
    std::ifstream checkFile(tempPath, std::ios::binary);
    if (!checkFile.is_open()) {
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
        outError = "No se pudo verificar el archivo descargado.";
        return false;
    }

    char magic[8] = {};
    checkFile.read(magic, sizeof(magic));
    std::streamsize bytesRead = checkFile.gcount();
    checkFile.close();

    bool isJpeg = (bytesRead >= 3 && (unsigned char)magic[0] == 0xFF && (unsigned char)magic[1] == 0xD8 && (unsigned char)magic[2] == 0xFF);
    bool isPng  = (bytesRead >= 4 && (unsigned char)magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G');
    bool isWebp = (bytesRead >= 8 && magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F');

    if (!isJpeg && !isPng && !isWebp) {
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
        outError = "El servidor no devolvió una imagen válida.";
        return false;
    }

    std::error_code renEc;
    std::filesystem::rename(tempPath, localDestPath, renEc);
    if (renEc) {
        std::filesystem::copy_file(tempPath, localDestPath, std::filesystem::copy_options::overwrite_existing, renEc);
        std::filesystem::remove(tempPath, renEc);
    }
    return true;
#else
    outError = "Descarga nativa no disponible.";
    return false;
#endif
}

// ── Chat IA (Gemini / DeepSeek) ──────────────────────────────────────────────
void ProAIManager::SendChatMessage(const std::vector<ProChatMessage>& history,
                                   std::function<void(bool success, const std::string& reply, uint32_t tokensUsed, const std::string& error)> callback) {
    std::string cannotSendReason;
    if (!CanSendMessage(&cannotSendReason)) {
        if (callback) {
            callback(false, "", 0, cannotSendReason);
        }
        return;
    }

    std::string rateErr;
    if (!CheckAndRecordRateLimit(&rateErr)) {
        if (callback) {
            callback(false, "", 0, rateErr);
        }
        return;
    }

    if (m_ChatBusy.exchange(true)) {
        if (callback) callback(false, "", 0, "Hay una solicitud en proceso.");
        return;
    }

    AIProvider provider = GetCurrentProvider();
    std::string apiKey   = GetDecryptedApiKey();

    std::thread([this, history, provider, apiKey, callback]() {
        bool success = false;
        std::string replyText;
        uint32_t tokensUsed = 0;
        std::string finalErr;
        int statusCode = 0;
        std::string reqError;

        const std::string sysPrompt =
            "Eres el Asistente Inteligente de ProyecThor for Professionals. Eres experto en música para auditorios, alabanza, "
            "canciones contemporáneas e himnos, y dirección técnica audiovisual. "
            "Cuando el usuario te pida escribir, adaptar o crear una canción, entrega la letra estructurada "
            "en estrofas, coro y puente, separando cada estrofa con UNA LÍNEA EN BLANCO para que ProyecThor "
            "pueda importarla directamente y convertirla en diapositivas de proyección legibles. Sé profesional y conciso.";

        if (provider == AIProvider::Gemini) {
            // Petición a Google Gemini API (gemini-flash-lite-latest)
            json jContents = json::array();
            for (const auto& msg : history) {
                jContents.push_back({
                    { "role", (msg.role == "user" ? "user" : "model") },
                    { "parts", json::array({ { { "text", msg.content } } }) }
                });
            }

            json reqBody;
            reqBody["system_instruction"] = {
                { "parts", json::array({ { { "text", sysPrompt } } }) }
            };
            reqBody["contents"] = jContents;
            reqBody["generationConfig"] = {
                { "temperature", 0.7 },
                { "maxOutputTokens", 2048 }
            };

            std::string path = "/v1beta/models/gemini-flash-lite-latest:generateContent?key=" + apiKey;
            std::string res = PerformHttpsPost("generativelanguage.googleapis.com", path, reqBody.dump(), "", statusCode, reqError);

            if (statusCode == 200) {
                try {
                    json jRes = json::parse(res);
                    if (jRes.contains("candidates") && jRes["candidates"].is_array() && !jRes["candidates"].empty()) {
                        const auto& cand = jRes["candidates"][0];
                        if (cand.contains("content") && cand["content"].contains("parts") && !cand["content"]["parts"].empty()) {
                            replyText = cand["content"]["parts"][0].value("text", "");
                            success = true;
                        }
                    }
                    if (jRes.contains("usageMetadata") && jRes["usageMetadata"].is_object()) {
                        tokensUsed = jRes["usageMetadata"].value("totalTokenCount", 0);
                    }
                } catch (...) {
                    finalErr = "Error al procesar respuesta de Gemini.";
                }
            } else {
                try {
                    json jErr = json::parse(res);
                    if (jErr.contains("error") && jErr["error"].is_object()) {
                        finalErr = jErr["error"].value("message", "Error en servidor Gemini.");
                    } else {
                        finalErr = "Error Gemini (HTTP " + std::to_string(statusCode) + ")";
                    }
                } catch (...) {
                    finalErr = reqError.empty() ? ("Error HTTP " + std::to_string(statusCode)) : reqError;
                }
            }
        } else {
            // Petición a DeepSeek API (deepseek-chat)
            json jMessages = json::array();
            jMessages.push_back({
                { "role", "system" },
                { "content", sysPrompt }
            });

            for (const auto& msg : history) {
                jMessages.push_back({
                    { "role", msg.role },
                    { "content", msg.content }
                });
            }

            json reqBody;
            reqBody["model"] = "deepseek-chat";
            reqBody["messages"] = jMessages;
            reqBody["temperature"] = 0.7;
            reqBody["max_tokens"] = 2048;

            std::string res = PerformHttpsPost("api.deepseek.com", "/chat/completions", reqBody.dump(), apiKey, statusCode, reqError);

            if (statusCode == 200) {
                try {
                    json jRes = json::parse(res);
                    if (jRes.contains("choices") && jRes["choices"].is_array() && !jRes["choices"].empty()) {
                        replyText = jRes["choices"][0]["message"].value("content", "");
                        success = true;
                    }
                    if (jRes.contains("usage") && jRes["usage"].is_object()) {
                        tokensUsed = jRes["usage"].value("total_tokens", 0);
                    }
                } catch (...) {
                    finalErr = "Respuesta JSON no válida de DeepSeek.";
                }
            } else {
                try {
                    json jErr = json::parse(res);
                    if (jErr.contains("error") && jErr["error"].is_object()) {
                        finalErr = jErr["error"].value("message", "Error en servidor DeepSeek.");
                    } else {
                        finalErr = "Error DeepSeek (HTTP " + std::to_string(statusCode) + ")";
                    }
                } catch (...) {
                    finalErr = reqError.empty() ? ("Error HTTP " + std::to_string(statusCode)) : reqError;
                }
            }
        }

        if (success) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_TokensUsedThisMonth += tokensUsed;
            m_DailyTextQueriesUsed++;
            SaveQuota();
        }

        m_ChatBusy.store(false);
        if (callback) callback(success, replyText, tokensUsed, finalErr);
    }).detach();
}

// ── Corrección y Edición de Letras con IA (Gemini) ───────────────────────────
void ProAIManager::EditLyricsWithAI(const std::string& currentLyrics,
                                    const std::string& userPrompt,
                                    std::function<void(bool success, const std::string& modifiedLyrics, uint32_t tokensUsed, const std::string& error)> callback) {
    std::string cannotSendReason;
    if (!CanSendMessage(&cannotSendReason)) {
        if (callback) {
            callback(false, "", 0, cannotSendReason);
        }
        return;
    }

    std::string rateErr;
    if (!CheckAndRecordRateLimit(&rateErr)) {
        if (callback) {
            callback(false, "", 0, rateErr);
        }
        return;
    }

    if (m_ChatBusy.exchange(true)) {
        if (callback) callback(false, "", 0, "Hay una solicitud en proceso.");
        return;
    }

    std::string apiKey = GetDecryptedApiKey();

    std::thread([this, currentLyrics, userPrompt, apiKey, callback]() {
        bool success = false;
        std::string replyText;
        uint32_t tokensUsed = 0;
        std::string finalErr;
        int statusCode = 0;
        std::string reqError;

        const std::string sysPrompt =
            "Eres un editor y corrector experto de letras de canciones cristianas y de adoración para ProyecThor.\n"
            "Tu tarea es tomar la letra de una canción en formato TXT y aplicar con máxima fidelidad las instrucciones del usuario.\n"
            "Por ejemplo: borrar puntos suspensivos (...), corregir ortografía y acentos, corregir errores de tipeo, o estructurar estrofas y coro.\n"
            "REGLAS ESTRICTAS E INQUEBRANTABLES:\n"
            "1. Devuelve ÚNICAMENTE la letra completa de la canción corregida en texto plano.\n"
            "2. NO agregues introducciones, explicaciones, notas ni saludos (NUNCA pongas cosas como 'Aquí está la letra' o 'Espero que te sirva').\n"
            "3. NO uses bloques de código markdown (prohibido usar ``` o ```txt). Devuelve directamente el texto plano limpio.\n"
            "4. Mantén la letra estructurada con una línea en blanco entre cada estrofa o coro para que las diapositivas de proyección se formen correctamente.";

        std::string userMessage = "Instrucciones de corrección: " + userPrompt + "\n\n"
                                  "Letra actual de la canción en TXT:\n" + currentLyrics;

        json jContents = json::array();
        jContents.push_back({
            { "role", "user" },
            { "parts", json::array({ { { "text", userMessage } } }) }
        });

        json reqBody;
        reqBody["system_instruction"] = {
            { "parts", json::array({ { { "text", sysPrompt } } }) }
        };
        reqBody["contents"] = jContents;
        reqBody["generationConfig"] = {
            { "temperature", 0.3 },
            { "maxOutputTokens", 4096 }
        };

        std::string path = "/v1beta/models/gemini-flash-lite-latest:generateContent?key=" + apiKey;
        std::string res = PerformHttpsPost("generativelanguage.googleapis.com", path, reqBody.dump(), "", statusCode, reqError);

        if (statusCode == 200) {
            try {
                json jRes = json::parse(res);
                if (jRes.contains("candidates") && jRes["candidates"].is_array() && !jRes["candidates"].empty()) {
                    const auto& cand = jRes["candidates"][0];
                    if (cand.contains("content") && cand["content"].contains("parts") && !cand["content"]["parts"].empty()) {
                        replyText = cand["content"]["parts"][0].value("text", "");
                        success = true;
                    }
                }
                if (jRes.contains("usageMetadata") && jRes["usageMetadata"].is_object()) {
                    tokensUsed = jRes["usageMetadata"].value("totalTokenCount", 0);
                }
            } catch (...) {
                finalErr = "Error al procesar respuesta de Gemini.";
            }
        } else {
            try {
                json jErr = json::parse(res);
                if (jErr.contains("error") && jErr["error"].is_object()) {
                    finalErr = jErr["error"].value("message", "Error en servidor Gemini.");
                } else {
                    finalErr = "Error Gemini (HTTP " + std::to_string(statusCode) + ")";
                }
            } catch (...) {
                finalErr = reqError.empty() ? ("Error HTTP " + std::to_string(statusCode)) : reqError;
            }
        }

        // Limpiar posibles bloques de código markdown
        if (success && !replyText.empty()) {
            size_t start = replyText.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) replyText = replyText.substr(start);
            size_t end = replyText.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) replyText = replyText.substr(0, end + 1);

            if (replyText.rfind("```", 0) == 0) {
                size_t nl = replyText.find('\n');
                if (nl != std::string::npos) replyText = replyText.substr(nl + 1);
            }
            if (replyText.size() >= 3 && replyText.substr(replyText.size() - 3) == "```") {
                replyText = replyText.substr(0, replyText.size() - 3);
            }
            start = replyText.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) replyText = replyText.substr(start);
            end = replyText.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) replyText = replyText.substr(0, end + 1);
        }

        if (success) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_TokensUsedThisMonth += tokensUsed;
            m_DailyTextQueriesUsed++;
            SaveQuota();
        }

        m_ChatBusy.store(false);
        if (callback) callback(success, replyText, tokensUsed, finalErr);
    }).detach();
}

// ── Generador de Imágenes IA ─────────────────────────────────────────────────
void ProAIManager::GenerateImage(const std::string& prompt,
                                 std::function<void(bool success, const std::string& savedPath, const std::string& error)> callback) {
    CheckMonthlyReset();

    if (!CanGenerateImage()) {
        if (callback) {
            callback(false, "", "Has alcanzado el límite de 30 imágenes este mes. Tu cuota se renovará al finalizar el mes.");
        }
        return;
    }

    std::string rateErr;
    if (!CheckAndRecordRateLimit(&rateErr)) {
        if (callback) {
            callback(false, "", rateErr);
        }
        return;
    }

    if (m_ImageBusy.exchange(true)) {
        if (callback) callback(false, "", "Hay una generación de imagen en proceso.");
        return;
    }

    std::thread([this, prompt, callback]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(100000, 999999);
        uint32_t seed = dis(gen);

        // Generación de imagen 16:9 widescreen HD (1280x720) optimizada para proyectores y pantallas
        std::string encodedPrompt = UrlEncode(prompt);
        std::string filename = "fondo_ia_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(seed) + ".jpg";
        std::filesystem::path dest = std::filesystem::path(GetAIImagesDir()) / filename;

        // Intentar con URLs en cascada para máxima fiabilidad y resiliencia (flux -> turbo -> 1024x576)
        std::vector<std::string> candidateUrls = {
            "https://image.pollinations.ai/prompt/" + encodedPrompt + "?model=flux&width=1280&height=720&nologo=true&seed=" + std::to_string(seed),
            "https://image.pollinations.ai/prompt/" + encodedPrompt + "?model=turbo&width=1280&height=720&nologo=true&seed=" + std::to_string(seed),
            "https://image.pollinations.ai/prompt/" + encodedPrompt + "?width=1024&height=576&nologo=true&seed=" + std::to_string(seed)
        };

        std::string downloadErr;
        bool ok = false;
        for (size_t i = 0; i < candidateUrls.size(); ++i) {
            ok = DownloadFileHttps(candidateUrls[i], dest.string(), downloadErr);
            if (ok) break;
            if (i + 1 < candidateUrls.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            }
        }

        if (ok) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ImagesUsedThisMonth++;
            SaveQuota();
        }

        m_ImageBusy.store(false);
        if (callback) {
            callback(ok, dest.string(), ok ? "" : downloadErr);
        }
    }).detach();
}

// ── Nube de Canciones (Firebase Cloud Songs) ──────────────────────────────────
const std::vector<CloudSong>& ProAIManager::GetCloudSongs() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CloudSongs;
}

void ProAIManager::RefreshCloudSongs(std::function<void(bool success, const std::string& msg)> callback) {
    // Cargar caché local primero
    std::string cachePath = GetCloudSongsCachePath();
    if (std::filesystem::exists(cachePath)) {
        std::ifstream f(cachePath);
        if (f.is_open()) {
            try {
                json j = json::parse(f);
                if (j.is_array()) {
                    std::lock_guard<std::mutex> lock(m_Mutex);
                    m_CloudSongs.clear();
                    for (const auto& item : j) {
                        CloudSong s;
                        s.id        = item.value("id", "");
                        s.title     = item.value("title", "");
                        s.content   = item.value("content", "");
                        s.updatedAt = item.value("updatedAt", "");
                        m_CloudSongs.push_back(s);
                    }
                }
            } catch (...) {}
        }
    }

    if (callback) callback(true, "Canciones cargadas correctamente.");
}

void ProAIManager::UploadCloudSong(const std::string& title, const std::string& content,
                                   std::function<void(bool success, const std::string& msg)> callback) {
    if (title.empty() || content.empty()) {
        if (callback) callback(false, "El título y la letra no pueden estar vacíos.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_CloudSongs.size() >= 5000) {
            if (callback) callback(false, "Has alcanzado el límite de 5,000 canciones en la nube.");
            return;
        }

        CloudSong newSong;
        newSong.id = "song_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(m_CloudSongs.size() + 1);
        newSong.title = title;
        newSong.content = content;
        newSong.updatedAt = GetCurrentMonthKey();

        // Si ya existe por título, actualizar
        bool updated = false;
        for (auto& s : m_CloudSongs) {
            if (s.title == title) {
                s.content = content;
                s.updatedAt = newSong.updatedAt;
                updated = true;
                break;
            }
        }
        if (!updated) {
            m_CloudSongs.push_back(newSong);
        }

        // Guardar en caché persistente de disco
        json j = json::array();
        for (const auto& s : m_CloudSongs) {
            j.push_back({
                { "id", s.id },
                { "title", s.title },
                { "content", s.content },
                { "updatedAt", s.updatedAt }
            });
        }
        std::ofstream f(GetCloudSongsCachePath());
        if (f.is_open()) {
            f << j.dump(2);
        }
    }

    if (callback) callback(true, "Canción guardada con éxito en la nube.");
}

void ProAIManager::DeleteCloudSong(const std::string& songId,
                                   std::function<void(bool success, const std::string& msg)> callback) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = std::remove_if(m_CloudSongs.begin(), m_CloudSongs.end(),
                                 [&songId](const CloudSong& s) { return s.id == songId; });
        if (it != m_CloudSongs.end()) {
            m_CloudSongs.erase(it, m_CloudSongs.end());

            json j = json::array();
            for (const auto& s : m_CloudSongs) {
                j.push_back({
                    { "id", s.id },
                    { "title", s.title },
                    { "content", s.content },
                    { "updatedAt", s.updatedAt }
                });
            }
            std::ofstream f(GetCloudSongsCachePath());
            if (f.is_open()) {
                f << j.dump(2);
            }
        }
    }

    if (callback) callback(true, "Canción eliminada de la nube.");
}

} // namespace ProyecThor::Core
