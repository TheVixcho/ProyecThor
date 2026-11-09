#include "AuthLicenseManager.h"
#include "AppLog.h"
#include <fstream>
#include <thread>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

namespace ProyecThor::Core {

// ── Criptografía autónoma para pasarela Flow (SHA-256 y HMAC-SHA256 sin librerías externas) ──
namespace FlowCrypto {

static inline uint32_t RightRotate(uint32_t val, uint32_t bits) {
    return (val >> bits) | (val << (32 - bits));
}

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

static void Sha256Transform(uint32_t state[8], const uint8_t buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t w[64];

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = RightRotate(w[i - 15], 7) ^ RightRotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = RightRotate(w[i - 2], 17) ^ RightRotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = RightRotate(e, 6) ^ RightRotate(e, 11) ^ RightRotate(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = RightRotate(a, 2) ^ RightRotate(a, 13) ^ RightRotate(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void Sha256Init(Sha256Ctx* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void Sha256Update(Sha256Ctx* ctx, const uint8_t* data, size_t len) {
    size_t bufferIdx = (size_t)(ctx->count & 0x3F);
    ctx->count += len;
    while (len > 0) {
        size_t toCopy = 64 - bufferIdx;
        if (len < toCopy) toCopy = len;
        std::memcpy(ctx->buffer + bufferIdx, data, toCopy);
        bufferIdx += toCopy;
        data += toCopy;
        len -= toCopy;
        if (bufferIdx == 64) {
            Sha256Transform(ctx->state, ctx->buffer);
            bufferIdx = 0;
        }
    }
}

static void Sha256Final(Sha256Ctx* ctx, uint8_t digest[32]) {
    uint64_t totalBits = ctx->count * 8;
    uint8_t pad = 0x80;
    Sha256Update(ctx, &pad, 1);
    uint8_t zero = 0;
    while ((ctx->count & 0x3F) != 56) {
        Sha256Update(ctx, &zero, 1);
    }
    uint8_t lengthBytes[8];
    for (int i = 0; i < 8; i++) {
        lengthBytes[i] = (uint8_t)((totalBits >> ((7 - i) * 8)) & 0xFF);
    }
    Sha256Update(ctx, lengthBytes, 8);
    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFF);
    }
}

static std::vector<uint8_t> Sha256(const uint8_t* data, size_t len) {
    Sha256Ctx ctx;
    Sha256Init(&ctx);
    Sha256Update(&ctx, data, len);
    std::vector<uint8_t> digest(32);
    Sha256Final(&ctx, digest.data());
    return digest;
}

static std::string HmacSha256(const std::string& keyStr, const std::string& dataStr) {
    std::vector<uint8_t> key(keyStr.begin(), keyStr.end());
    if (key.size() > 64) {
        key = Sha256(key.data(), key.size());
    }
    key.resize(64, 0);

    std::vector<uint8_t> o_key_pad(64), i_key_pad(64);
    for (size_t i = 0; i < 64; i++) {
        o_key_pad[i] = key[i] ^ 0x5c;
        i_key_pad[i] = key[i] ^ 0x36;
    }

    Sha256Ctx innerCtx;
    Sha256Init(&innerCtx);
    Sha256Update(&innerCtx, i_key_pad.data(), 64);
    Sha256Update(&innerCtx, reinterpret_cast<const uint8_t*>(dataStr.data()), dataStr.size());
    uint8_t innerHash[32];
    Sha256Final(&innerCtx, innerHash);

    Sha256Ctx outerCtx;
    Sha256Init(&outerCtx);
    Sha256Update(&outerCtx, o_key_pad.data(), 64);
    Sha256Update(&outerCtx, innerHash, 32);
    uint8_t finalHash[32];
    Sha256Final(&outerCtx, finalHash);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; i++) {
        ss << std::setw(2) << (int)finalHash[i];
    }
    return ss.str();
}

} // namespace FlowCrypto

using json = nlohmann::json;

// Helpers defensivos para nlohmann::json: nunca lanzan excepciones si el campo es JSON null
static inline std::string JsonSafeString(const json& j, const std::string& key, const std::string& defaultVal = "") {
    if (j.is_object() && j.contains(key)) {
        const auto& it = j.find(key);
        if (it != j.end() && !it->is_null() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return defaultVal;
}

static inline bool JsonSafeBool(const json& j, const std::string& key, bool defaultVal = false) {
    if (j.is_object() && j.contains(key)) {
        const auto& it = j.find(key);
        if (it != j.end() && !it->is_null() && it->is_boolean()) {
            return it->get<bool>();
        }
    }
    return defaultVal;
}

static inline int64_t JsonSafeInt64(const json& j, const std::string& key, int64_t defaultVal = 0) {
    if (j.is_object() && j.contains(key)) {
        const auto& it = j.find(key);
        if (it != j.end() && !it->is_null()) {
            if (it->is_number()) return it->get<int64_t>();
            if (it->is_string()) {
                try {
                    return std::stoll(it->get<std::string>());
                } catch (...) {}
            }
        }
    }
    return defaultVal;
}

// Helper para convertir std::string a std::wstring (UTF-8 a Wide en Windows)
#ifdef _WIN32
static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}
#endif

// Helper para codificar URLs en parámetros GET/POST
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

// Helper para parsear fechas ISO 8601 de Lemon Squeezy o fechas de Flow (YYYY-MM-DD HH:MM:SS) a timestamp Unix
static int64_t ParseIsoDate(const std::string& str) {
    if (str.empty()) return 0;
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    char sep1 = 0, sep2 = 0;
    std::istringstream ss(str);
    if (ss >> year >> sep1 >> month >> sep2 >> day) {
        if (year >= 2020 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            std::tm tm = {};
            tm.tm_year = year - 1900;
            tm.tm_mon  = month - 1;
            tm.tm_mday = day;

            // Saltar posible separador 'T' o espacio ' ' antes de la hora
            while (ss.peek() == ' ' || ss.peek() == 'T') {
                ss.get();
            }
            char sepColon1 = 0, sepColon2 = 0;
            if (ss >> hour >> sepColon1 >> min >> sepColon2 >> sec) {
                tm.tm_hour = hour;
                tm.tm_min  = min;
                tm.tm_sec  = sec;
            }
#ifdef _WIN32
            time_t t = _mkgmtime(&tm);
            if (t != (time_t)-1) return (int64_t)t;
#endif
            time_t t2 = std::mktime(&tm);
            if (t2 != (time_t)-1) return (int64_t)t2;
        }
    }
    return 0;
}

// Helper para formatear timestamp Unix a cadena legible
static std::string FormatDate(int64_t timestamp) {
    if (timestamp <= 0) return "";
    std::time_t t = (std::time_t)timestamp;
    std::tm tmBuf = {};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    char buf[64] = {};
    std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tmBuf);
    return std::string(buf);
}

AuthLicenseManager& AuthLicenseManager::Get() {
    static AuthLicenseManager s_Instance;
    return s_Instance;
}

AuthLicenseManager::AuthLicenseManager() {
    // Clave de API web de Firebase asociada al proyecto oficial ProyecThor
    m_FirebaseApiKey = "AIzaSyDmlzVe3csRujzDLXx668Hs2Glqx_QvQw8";
}

void AuthLicenseManager::Initialize() {
    LoadState();

    if (m_SessionId.empty()) {
        auto nowNano = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        m_SessionId = "SESS-" + std::to_string(nowNano) + "-" + std::to_string(rand() % 999999);
    }
    m_DeviceName = GetDefaultInstanceName();

    // Reclamar la sesión en este dispositivo si ya hay sesión de usuario iniciada
    if (m_IsLoggedIn && !m_User.localId.empty()) {
        ClaimActiveSession(m_User.localId);
    }
}

std::string AuthLicenseManager::GetConfigFilePath() const {
#ifdef _WIN32
    char appDataBuf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
        std::filesystem::path dir = std::filesystem::path(appDataBuf) / "ProyecThor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "auth_license.json").string();
    }
    return "auth_license.json";
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::current_path();
    std::filesystem::path dir = base / "ProyecThor";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "auth_license.json").string();
#endif
}

std::string AuthLicenseManager::GetDefaultInstanceName() const {
#ifdef _WIN32
    char nameBuf[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = sizeof(nameBuf);
    if (GetComputerNameA(nameBuf, &size)) {
        return std::string("PC-") + nameBuf;
    }
#endif
    return "ProyecThor-Workstation";
}

void AuthLicenseManager::SaveState() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    try {
        json j;
        j["firebase_api_key"]      = m_FirebaseApiKey;
        j["flow_checkout_url"]     = m_FlowCheckoutBaseUrl;
        j["flow_api_key"]          = m_FlowApiKey;
        j["flow_secret_key"]       = m_FlowSecretKey;
        j["flow_plan_id"]          = m_FlowPlanId;
        j["is_logged_in"]          = m_IsLoggedIn;
        j["is_admin"]              = m_IsAdmin;
        j["user"] = {
            { "local_id",      m_User.localId },
            { "email",         m_User.email },
            { "display_name",  m_User.displayName },
            { "id_token",      m_User.idToken },
            { "refresh_token", m_User.refreshToken }
        };
        // Persistencia segura del estado Pro verificado
        j["license"] = {
            { "is_valid",          m_License.isValid },
            { "key",               m_License.key },
            { "status",            m_License.status },
            { "instance_id",       m_License.instanceId },
            { "instance_name",     m_License.instanceName },
            { "customer_name",     m_License.customerName },
            { "customer_email",    m_License.customerEmail },
            { "product_name",      m_License.productName },
            { "expiry_date",       m_License.expiryDate },
            { "activated_at",      m_License.activatedAt },
            { "expires_at",        m_License.expiresAt },
            { "last_validated_at", m_License.lastValidatedAt }
        };

        std::string path = GetConfigFilePath();
        std::ofstream f(path);
        if (f.is_open()) {
            f << j.dump(2);
            f.flush();
            f.close();
        }
    } catch (...) {}
}

void AuthLicenseManager::LoadState() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    std::string path = GetConfigFilePath();
    if (!std::filesystem::exists(path)) return;

    std::ifstream f(path);
    if (!f.is_open()) return;

    try {
        json j = json::parse(f);
        std::string loadedKey = JsonSafeString(j, "firebase_api_key", "");
        if (!loadedKey.empty() && loadedKey != "AIzaSyAfvX2q_3xOWG16apTsvmZwIPLqSVTRZOo") {
            m_FirebaseApiKey = loadedKey;
        } else {
            m_FirebaseApiKey = "AIzaSyDmlzVe3csRujzDLXx668Hs2Glqx_QvQw8";
        }
        m_FlowCheckoutBaseUrl  = JsonSafeString(j, "flow_checkout_url", m_FlowCheckoutBaseUrl);
        m_FlowApiKey           = JsonSafeString(j, "flow_api_key", m_FlowApiKey);
        m_FlowSecretKey        = JsonSafeString(j, "flow_secret_key", m_FlowSecretKey);
        m_FlowPlanId           = JsonSafeString(j, "flow_plan_id", m_FlowPlanId);
        m_IsLoggedIn           = JsonSafeBool(j, "is_logged_in", false);
        m_IsAdmin              = JsonSafeBool(j, "is_admin", false);

        if (j.contains("user") && j["user"].is_object()) {
            const auto& u = j["user"];
            m_User.localId = JsonSafeString(u, "local_id", "");
            m_User.email = JsonSafeString(u, "email", "");
            m_User.displayName = JsonSafeString(u, "display_name", "");
            m_User.idToken = JsonSafeString(u, "id_token", "");
            m_User.refreshToken = JsonSafeString(u, "refresh_token", "");
        }

        // Restaurar estado de licencia Pro validado si no ha expirado
        if (j.contains("license") && j["license"].is_object()) {
            const auto& lic = j["license"];
            bool isValid = JsonSafeBool(lic, "is_valid", false);
            int64_t expiresAt = lic.value("expires_at", (int64_t)0);
            int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
            if (isValid && (expiresAt == 0 || nowSec < expiresAt)) {
                m_License.isValid         = true;
                m_License.key             = JsonSafeString(lic, "key", "");
                m_License.status          = JsonSafeString(lic, "status", "active");
                m_License.instanceId      = JsonSafeString(lic, "instance_id", "");
                m_License.instanceName    = JsonSafeString(lic, "instance_name", "");
                m_License.customerName    = JsonSafeString(lic, "customer_name", "");
                m_License.customerEmail   = JsonSafeString(lic, "customer_email", "");
                m_License.productName     = JsonSafeString(lic, "product_name", "ProyecThor Pro");
                m_License.expiryDate      = JsonSafeString(lic, "expiry_date", "");
                m_License.activatedAt     = lic.value("activated_at", (int64_t)0);
                m_License.expiresAt       = expiresAt;
                m_License.lastValidatedAt = lic.value("last_validated_at", (int64_t)0);
            } else {
                m_License = LicenseInfo();
            }
        } else {
            m_License = LicenseInfo();
        }
        m_PendingFlowToken.clear();
        m_PendingCommerceOrder.clear();
    } catch (...) {}
}

// ── Ejecución de peticiones HTTPS nativas en Windows (WinHTTP) ───────────────
std::string AuthLicenseManager::PerformHttpsRequest(const std::string& method, const std::string& host, const std::string& path,
                                                    const std::string& data, const std::string& contentType,
                                                    const std::string& bearerToken,
                                                    int& outStatusCode, std::string& outError) {
    outStatusCode = 0;
    outError.clear();

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"ProyecThor-Engine/1.1",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = "Fallo al inicializar la sesión HTTPS (WinHttpOpen).";
        return "";
    }

    std::wstring wHost = Utf8ToWide(host);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = "Fallo al conectar con el servidor remoto: " + host;
        return "";
    }

    std::wstring wMethod = Utf8ToWide(method);
    std::wstring wPath   = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Fallo al abrir solicitud HTTPS.";
        return "";
    }

    // Cabecera Content-Type y Authorization Bearer
    std::wstring wHeader = L"Accept: application/json\r\n";
    if (!contentType.empty()) {
        wHeader += L"Content-Type: " + Utf8ToWide(contentType) + L"\r\n";
    }
    if (!bearerToken.empty()) {
        wHeader += L"Authorization: Bearer " + Utf8ToWide(bearerToken) + L"\r\n";
    }
    WinHttpAddRequestHeaders(hRequest, wHeader.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    DWORD dataLen = (DWORD)data.length();
    LPVOID pData  = (dataLen > 0) ? (LPVOID)data.c_str() : NULL;

    BOOL bSend = WinHttpSendRequest(hRequest,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    pData, dataLen,
                                    dataLen, 0);

    if (!bSend) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Fallo de conexión o tiempo de espera al transmitir datos.";
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "No se recibió respuesta del servidor remoto.";
        return "";
    }

    // Obtener código de estado HTTP
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatusCode = (int)statusCode;

    // Leer cuerpo de la respuesta
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
    outError = "Conectividad HTTPS nativa no disponible en esta plataforma sin OpenSSL.";
    return "";
#endif
}

std::string AuthLicenseManager::PerformHttpsPost(const std::string& host, const std::string& path,
                                                 const std::string& postData, const std::string& contentType,
                                                 int& outStatusCode, std::string& outError) {
    return PerformHttpsRequest("POST", host, path, postData, contentType, "", outStatusCode, outError);
}

bool AuthLicenseManager::RefreshFirebaseIdToken(std::string& outIdToken, std::string& outError) {
    std::string apiKey = GetFirebaseApiKey();
    std::string rToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        rToken = m_User.refreshToken;
    }

    if (rToken.empty()) {
        outError = "No hay token de sesión para renovar. Por favor inicia sesión.";
        return false;
    }

    std::string postData = "grant_type=refresh_token&refresh_token=" + UrlEncode(rToken);
    int statusCode = 0;
    std::string reqError;
    std::string res = PerformHttpsPost("securetoken.googleapis.com", "/v1/token?key=" + apiKey,
                                       postData, "application/x-www-form-urlencoded", statusCode, reqError);

    if (statusCode == 200) {
        try {
            json j = json::parse(res);
            std::string newIdToken = JsonSafeString(j, "id_token", "");
            std::string newRefreshToken = JsonSafeString(j, "refresh_token", rToken);
            if (!newIdToken.empty()) {
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    m_User.idToken = newIdToken;
                    m_User.refreshToken = newRefreshToken;
                }
                SaveState();
                outIdToken = newIdToken;
                return true;
            }
        } catch (...) {}
    }

    outError = "Error al renovar sesión en Firebase (HTTP " + std::to_string(statusCode) + ")";
    return false;
}

// ── Métodos de Firebase Auth ─────────────────────────────────────────────────

bool AuthLicenseManager::IsLoggedIn() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_IsLoggedIn;
}

const UserProfile& AuthLicenseManager::GetUser() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_User;
}

const std::string& AuthLicenseManager::GetAuthError() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_AuthError;
}

bool AuthLicenseManager::IsAuthBusy() const {
    return m_AuthBusy.load();
}

void AuthLicenseManager::SetFirebaseApiKey(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_FirebaseApiKey = key;
}

std::string AuthLicenseManager::GetFirebaseApiKey() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_FirebaseApiKey;
}

void AuthLicenseManager::LoginWithFirebase(const std::string& email, const std::string& password,
                                          std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_AuthBusy.exchange(true)) return;

    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_AuthError.clear();
    }

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, email, password, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:signInWithPassword?key=" + apiKey;

        json payload;
        payload["email"] = email;
        payload["password"] = password;
        payload["returnSecureToken"] = true;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = false;
        std::string finalMsg;

        if (!reqError.empty()) {
            finalMsg = reqError;
        } else if (statusCode == 200) {
            try {
                json j = json::parse(res);
                std::string uid = JsonSafeString(j, "localId", "");
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    m_IsLoggedIn = true;
                    m_User.localId      = uid;
                    m_User.email        = JsonSafeString(j, "email", email);
                    m_User.displayName  = JsonSafeString(j, "displayName", "");
                    m_User.idToken      = JsonSafeString(j, "idToken", "");
                    m_User.refreshToken = JsonSafeString(j, "refreshToken", "");
                    m_AuthError.clear();

                    // El estado de licencia inicia estrictamente inactivo hasta validación online
                    m_License = LicenseInfo();
                    m_LicenseError.clear();
                    m_PendingFlowToken.clear();
                    m_PendingCommerceOrder.clear();
                }
                success = true;
                finalMsg = "Sesión iniciada correctamente.";

                m_SessionKicked.store(false);
                ClaimActiveSession(uid);

                // Comprobar automáticamente el estado Pro en Flow y servidores
                RefreshProStatus(nullptr);
            } catch (...) {
                finalMsg = "Respuesta del servidor de autenticación ilegible.";
            }
        } else {
            try {
                json j = json::parse(res);
                std::string errCode = j["error"]["message"].get<std::string>();
                if (errCode == "EMAIL_NOT_FOUND" || errCode == "INVALID_LOGIN_CREDENTIALS" || errCode == "INVALID_PASSWORD") {
                    finalMsg = "Credenciales incorrectas (correo o contraseña no válidos).";
                } else if (errCode == "USER_DISABLED") {
                    finalMsg = "Esta cuenta ha sido inhabilitada por el administrador.";
                } else if (errCode == "TOO_MANY_ATTEMPTS_TRY_LATER") {
                    finalMsg = "Demasiados intentos fallidos. Inténtalo más tarde.";
                } else {
                    finalMsg = "Error de Firebase: " + errCode;
                }
            } catch (...) {
                finalMsg = "Error HTTP " + std::to_string(statusCode) + " al autenticar con Firebase.";
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!success) m_AuthError = finalMsg;
        }

        if (success) SaveState();
        m_AuthBusy.store(false);

        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::SignUpWithFirebase(const std::string& email, const std::string& password,
                                           std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_AuthBusy.exchange(true)) return;

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, email, password, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:signUp?key=" + apiKey;

        json payload;
        payload["email"] = email;
        payload["password"] = password;
        payload["returnSecureToken"] = true;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = false;
        std::string finalMsg;

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                std::string newLocalId = JsonSafeString(j, "localId", "");
                std::string newEmail   = JsonSafeString(j, "email", email);
                std::string newIdToken = JsonSafeString(j, "idToken", "");

                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    m_IsLoggedIn = true;
                    m_User.localId      = newLocalId;
                    m_User.email        = newEmail;
                    m_User.displayName  = JsonSafeString(j, "displayName", "");
                    m_User.idToken      = newIdToken;
                    m_User.refreshToken = JsonSafeString(j, "refreshToken", "");
                    m_AuthError.clear();

                    m_License = LicenseInfo();
                    m_LicenseError.clear();
                    m_PendingFlowToken.clear();
                    m_PendingCommerceOrder.clear();
                }

                // Crear o inicializar documento en Firestore en collection 'users/{uid}'
                // para que aparezca de inmediato en la consola de Firebase con isPro = false
                if (!newLocalId.empty()) {
                    json fsDoc;
                    fsDoc["fields"]["email"]["stringValue"]       = newEmail;
                    fsDoc["fields"]["uid"]["stringValue"]         = newLocalId;
                    fsDoc["fields"]["isPro"]["booleanValue"]      = false;
                    fsDoc["fields"]["banned"]["booleanValue"]     = false;
                    fsDoc["fields"]["role"]["stringValue"]        = "user";

                    int fsStatus = 0;
                    std::string fsErr;
                    std::string patchPath = "/v1/projects/proyecthor/databases/(default)/documents/users/" + newLocalId +
                                            "?updateMask.fieldPaths=email&updateMask.fieldPaths=uid&updateMask.fieldPaths=isPro&updateMask.fieldPaths=banned&updateMask.fieldPaths=role&key=" + apiKey;

                    PerformHttpsRequest("PATCH", "firestore.googleapis.com", patchPath, fsDoc.dump(), "application/json", newIdToken, fsStatus, fsErr);
                }

                success = true;
                finalMsg = "Cuenta creada e iniciada con éxito.";
            } catch (...) {
                finalMsg = "Respuesta de registro no válida.";
            }
        } else {
            try {
                json j = json::parse(res);
                std::string errCode = j["error"]["message"].get<std::string>();
                if (errCode == "EMAIL_EXISTS") {
                    finalMsg = "Este correo ya está registrado. Usa Iniciar Sesión.";
                } else if (errCode == "WEAK_PASSWORD : Password should be at least 6 characters") {
                    finalMsg = "La contraseña debe tener al menos 6 caracteres.";
                } else {
                    finalMsg = "Error: " + errCode;
                }
            } catch (...) {
                finalMsg = "Fallo al crear cuenta (HTTP " + std::to_string(statusCode) + ").";
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!success) m_AuthError = finalMsg;
        }

        if (success) SaveState();
        m_AuthBusy.store(false);

        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::Logout() {
    CancelPaymentPolling();
    m_AuthBusy.store(false);
    m_LicenseBusy.store(false);

    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_IsLoggedIn = false;
        m_IsAdmin    = false;
        m_User = UserProfile();
        m_AuthError.clear();

        // Limpiar completamente el estado de licencia Pro en memoria
        m_License = LicenseInfo();
        m_LicenseError.clear();
        m_PendingFlowToken.clear();
        m_PendingCommerceOrder.clear();
        m_SessionKicked.store(false);
        m_KickedByDevice.clear();
        m_KickedMessage.clear();
    }
    SaveState();
}

// ── Gestión de Perfil de Usuario en Firebase ─────────────────────────────────

void AuthLicenseManager::UpdateDisplayName(const std::string& newDisplayName,
                                           std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_AuthBusy.exchange(true)) return;

    std::string idToken, localId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
        localId = m_User.localId;
    }

    if (idToken.empty()) {
        m_AuthBusy.store(false);
        if (onComplete) onComplete(false, "No hay sesión activa.");
        return;
    }

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, newDisplayName, idToken, localId, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:update?key=" + apiKey;

        json payload;
        payload["idToken"] = idToken;
        payload["displayName"] = newDisplayName;
        payload["returnSecureToken"] = true;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = false;
        std::string finalMsg;

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    m_User.displayName = newDisplayName;
                    if (j.contains("idToken")) m_User.idToken = j["idToken"].get<std::string>();
                    if (j.contains("refreshToken")) m_User.refreshToken = j["refreshToken"].get<std::string>();
                }

                // Sincronizar en Firestore
                if (!localId.empty()) {
                    json fsDoc;
                    fsDoc["fields"]["displayName"]["stringValue"] = newDisplayName;
                    int fsStatus = 0;
                    std::string fsErr;
                    std::string patchPath = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId +
                                            "?updateMask.fieldPaths=displayName&key=" + apiKey;
                    PerformHttpsRequest("PATCH", "firestore.googleapis.com", patchPath, fsDoc.dump(), "application/json", idToken, fsStatus, fsErr);
                }

                success = true;
                finalMsg = "Nombre de usuario actualizado con éxito.";
                SaveState();
            } catch (...) {
                finalMsg = "Error al procesar la respuesta del servidor.";
            }
        } else {
            finalMsg = "No se pudo actualizar el nombre (HTTP " + std::to_string(statusCode) + ").";
        }

        m_AuthBusy.store(false);
        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::ChangePassword(const std::string& newPassword,
                                       std::function<void(bool success, const std::string& msg)> onComplete) {
    if (newPassword.length() < 6) {
        if (onComplete) onComplete(false, "La nueva contraseña debe tener al menos 6 caracteres.");
        return;
    }

    if (m_AuthBusy.exchange(true)) return;

    std::string idToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
    }

    if (idToken.empty()) {
        m_AuthBusy.store(false);
        if (onComplete) onComplete(false, "No hay sesión activa.");
        return;
    }

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, newPassword, idToken, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:update?key=" + apiKey;

        json payload;
        payload["idToken"] = idToken;
        payload["password"] = newPassword;
        payload["returnSecureToken"] = true;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = false;
        std::string finalMsg;

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    if (j.contains("idToken")) m_User.idToken = j["idToken"].get<std::string>();
                    if (j.contains("refreshToken")) m_User.refreshToken = j["refreshToken"].get<std::string>();
                }
                success = true;
                finalMsg = "Contraseña actualizada exitosamente.";
                SaveState();
            } catch (...) {
                finalMsg = "Error al procesar la respuesta del servidor.";
            }
        } else {
            finalMsg = "Error al cambiar contraseña (HTTP " + std::to_string(statusCode) + ").";
        }

        m_AuthBusy.store(false);
        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::DeleteAccount(std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_AuthBusy.exchange(true)) return;

    std::string idToken, localId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
        localId = m_User.localId;
    }

    if (idToken.empty()) {
        m_AuthBusy.store(false);
        if (onComplete) onComplete(false, "No hay sesión activa.");
        return;
    }

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, idToken, localId, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:delete?key=" + apiKey;

        json payload;
        payload["idToken"] = idToken;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = false;
        std::string finalMsg;

        if (statusCode == 200) {
            // Eliminar documento de Firestore
            if (!localId.empty()) {
                int fsStatus = 0;
                std::string fsErr;
                std::string delPath = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId + "?key=" + apiKey;
                PerformHttpsRequest("DELETE", "firestore.googleapis.com", delPath, "", "application/json", idToken, fsStatus, fsErr);
            }

            Logout();
            success = true;
            finalMsg = "Tu cuenta ha sido eliminada permanentemente.";
        } else {
            finalMsg = "No se pudo eliminar la cuenta. Por seguridad, vuelve a iniciar sesión e inténtalo de nuevo.";
        }

        m_AuthBusy.store(false);
        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::SendEmailVerification(std::function<void(bool success, const std::string& msg)> onComplete) {
    std::string idToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
    }

    if (idToken.empty()) {
        if (onComplete) onComplete(false, "Debes iniciar sesión para verificar tu correo.");
        return;
    }

    std::string apiKey = GetFirebaseApiKey();
    std::thread([this, idToken, apiKey, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/accounts:sendOobCode?key=" + apiKey;

        json payload;
        payload["requestType"] = "VERIFY_EMAIL";
        payload["idToken"] = idToken;

        std::string res = PerformHttpsPost(m_FirebaseHost, path, payload.dump(), "application/json", statusCode, reqError);

        bool success = (statusCode == 200);
        std::string msg = success
            ? "Te hemos enviado un correo de verificación. Revisa tu bandeja de entrada o spam."
            : "No se pudo enviar el correo de verificación (HTTP " + std::to_string(statusCode) + ").";

        if (onComplete) onComplete(success, msg);
    }).detach();
}

// ── Pasarela de Pago: Compra y Checkout (Flow Chile / Lemon Squeezy) ─────────

bool AuthLicenseManager::CanBuySubscription() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_IsLoggedIn;
}

void AuthLicenseManager::SetFlowCheckoutUrl(const std::string& url) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_FlowCheckoutBaseUrl = url;
}

std::string AuthLicenseManager::GetFlowCheckoutUrl() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_FlowCheckoutBaseUrl;
}

bool AuthLicenseManager::OpenCheckoutUrl(std::string& outError) {
    std::string email;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (!m_IsLoggedIn) {
            outError = "Debes iniciar sesión con tu correo y contraseña antes de suscribirte.";
            return false;
        }
        email = m_User.email;
    }

    // Pasarela principal activa: Flow Chile (Plan PROPROYECTHOR - $9.990 CLP)
    std::string finalUrl = m_FlowCheckoutBaseUrl;
    if (!email.empty()) {
        if (finalUrl.find('?') == std::string::npos) {
            finalUrl += "?email=" + UrlEncode(email);
        } else {
            finalUrl += "&email=" + UrlEncode(email);
        }
    }

    // Lemon Squeezy (guardado para venta internacional futura):
    // std::string lemonUrl = "https://vixcho.lemonsqueezy.com/checkout/buy/e609efc0-4393-46fc-b6ed-d535cb9dd2dc";
    // if (!email.empty()) lemonUrl += "?checkout[email]=" + UrlEncode(email);

#ifdef _WIN32
    HINSTANCE hRes = ShellExecuteA(NULL, "open", finalUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)hRes <= 32) {
        outError = "No se pudo abrir el navegador web del sistema.";
        return false;
    }
    return true;
#else
    std::string cmd = "xdg-open \"" + finalUrl + "\" >/dev/null 2>&1 &";
    int res = system(cmd.c_str());
    if (res != 0) {
        outError = "Error al ejecutar xdg-open para el navegador.";
        return false;
    }
    return true;
#endif
}

void AuthLicenseManager::SetFlowCredentials(const std::string& apiKey, const std::string& secretKey) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_FlowApiKey = apiKey;
    m_FlowSecretKey = secretKey;
    SaveState();
}

bool AuthLicenseManager::IsPollingPayment() const {
    return m_IsPollingPayment.load();
}

std::string AuthLicenseManager::GetPendingPaymentToken() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_PendingFlowToken;
}

void AuthLicenseManager::CancelPaymentPolling() {
    m_StopPollingRequested.store(true);
    m_IsPollingPayment.store(false);
}

bool AuthLicenseManager::CreateFlowPaymentOrder(std::string& outRedirectUrl, std::string& outError,
                                                std::function<void(bool success, const std::string& msg)> onComplete) {
    std::string email;
    std::string localId;
    std::string apiKey;
    std::string secretKey;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (!m_IsLoggedIn) {
            outError = "Debes iniciar sesión con tu cuenta antes de realizar el pago.";
            return false;
        }
        email = m_User.email;
        localId = m_User.localId;
        apiKey = m_FlowApiKey;
        secretKey = m_FlowSecretKey;
    }

    if (email.empty() || apiKey.empty() || secretKey.empty()) {
        outError = "Faltan credenciales de Flow o el correo del usuario.";
        return false;
    }

    // Generar orden de comercio única
    int64_t now = (int64_t)std::time(nullptr);
    std::string uidShort = localId.substr(0, std::min<size_t>(8, localId.size()));
    std::string commerceOrder = "PRO-" + uidShort + "-" + std::to_string(now);

    std::string amount = "9990";
    std::string currency = "CLP";
    std::string subject = "ProyecThor for Professionals (1 Mes)";
    std::string urlConfirmation = "https://proyecthor.web.app/api/flow-confirm";
    std::string urlReturn = "https://proyecthor.web.app/#flow-return";

    // Parámetros ordenados alfabéticamente para la firma Flow HMAC-SHA256:
    // amount, apiKey, commerceOrder, currency, email, subject, urlConfirmation, urlReturn
    std::string toSign = "amount" + amount +
                         "apiKey" + apiKey +
                         "commerceOrder" + commerceOrder +
                         "currency" + currency +
                         "email" + email +
                         "subject" + subject +
                         "urlConfirmation" + urlConfirmation +
                         "urlReturn" + urlReturn;

    std::string signature = FlowCrypto::HmacSha256(secretKey, toSign);

    std::string postBody = "amount=" + UrlEncode(amount) +
                           "&apiKey=" + UrlEncode(apiKey) +
                           "&commerceOrder=" + UrlEncode(commerceOrder) +
                           "&currency=" + UrlEncode(currency) +
                           "&email=" + UrlEncode(email) +
                           "&s=" + signature +
                           "&subject=" + UrlEncode(subject) +
                           "&urlConfirmation=" + UrlEncode(urlConfirmation) +
                           "&urlReturn=" + UrlEncode(urlReturn);

    int statusCode = 0;
    std::string reqError;
    std::string res = PerformHttpsPost("www.flow.cl", "/api/payment/create", postBody, "application/x-www-form-urlencoded", statusCode, reqError);

    if (!reqError.empty() || statusCode != 200) {
        outError = !reqError.empty() ? reqError : ("Error del servidor Flow (HTTP " + std::to_string(statusCode) + "): " + res);
        return false;
    }

    std::string token;
    std::string flowUrl;
    try {
        json j = json::parse(res);
        flowUrl = JsonSafeString(j, "url", "");
        token = JsonSafeString(j, "token", "");
    } catch (...) {
        outError = "Respuesta de Flow ilegible.";
        return false;
    }

    if (flowUrl.empty() || token.empty()) {
        outError = "Flow no devolvió una URL o token válido.";
        return false;
    }

    outRedirectUrl = flowUrl + "?token=" + token;

    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_PendingFlowToken = token;
        m_PendingCommerceOrder = commerceOrder;
    }
    SaveState();

    // Abrir pasarela en navegador predeterminado
#ifdef _WIN32
    ShellExecuteA(NULL, "open", outRedirectUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
    std::string cmd = "xdg-open \"" + outRedirectUrl + "\" >/dev/null 2>&1 &";
    system(cmd.c_str());
#endif

    // Iniciar polling de detección de pago automático
    StartPaymentPolling(token, onComplete);
    return true;
}

void AuthLicenseManager::StartPaymentPolling(const std::string& token,
                                             std::function<void(bool success, const std::string& msg)> onComplete) {
    if (token.empty()) return;

    CancelPaymentPolling();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    m_StopPollingRequested.store(false);
    m_IsPollingPayment.store(true);

    std::thread([this, token, onComplete]() {
        std::string apiKey, secretKey, localId, userEmail;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            apiKey = m_FlowApiKey;
            secretKey = m_FlowSecretKey;
            localId = m_User.localId;
            userEmail = m_User.email;
        }

        std::string toSign = "apiKey" + apiKey + "token" + token;
        std::string s = FlowCrypto::HmacSha256(secretKey, toSign);
        std::string path = "/api/payment/getStatus?apiKey=" + UrlEncode(apiKey) + "&token=" + UrlEncode(token) + "&s=" + s;

        const int maxAttempts = 150;
        bool paymentCompleted = false;
        std::string resultMsg;

        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            if (m_StopPollingRequested.load()) {
                break;
            }
            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                if (!m_IsLoggedIn) break;
            }

            int statusCode = 0;
            std::string reqError;
            std::string res = PerformHttpsRequest("GET", "www.flow.cl", path, "", "application/json", "", statusCode, reqError);

            if (statusCode == 200) {
                try {
                    json j = json::parse(res);
                    int64_t status = JsonSafeInt64(j, "status", 0);
                    int64_t flowOrder = JsonSafeInt64(j, "flowOrder", 0);

                    if (status == 2) {
                        int64_t now = (int64_t)std::time(nullptr);
                        int64_t expiresAt = now + (30LL * 24 * 3600);

                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            if (!m_IsLoggedIn || m_StopPollingRequested.load()) break;
                            m_License.isValid = true;
                            m_License.status = "active";
                            m_License.productName = "ProyecThor for Professionals";
                            m_License.customerEmail = userEmail;
                            m_License.customerName = m_User.displayName.empty() ? userEmail : m_User.displayName;
                            m_License.activatedAt = now;
                            m_License.expiresAt = expiresAt;
                            m_License.expiryDate = FormatDate(expiresAt);
                            m_License.lastValidatedAt = now;
                            m_LicenseError.clear();

                            m_PendingFlowToken.clear();
                            m_PendingCommerceOrder.clear();
                        }

                        UpdateFirestoreProStatus(localId, true, std::to_string(flowOrder), expiresAt);
                        SaveState();

                        paymentCompleted = true;
                        resultMsg = "¡Pago confirmado con éxito en Flow! Tu cuenta ProyecThor for Professionals está activa por 1 mes.";
                        break;
                    } else if (status == 3 || status == 4) {
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            m_PendingFlowToken.clear();
                            m_PendingCommerceOrder.clear();
                        }
                        SaveState();
                        resultMsg = (status == 3) ? "La transacción fue rechazada en la pasarela de pago Flow."
                                                  : "La transacción fue anulada por el usuario en Flow.";
                        break;
                    }
                } catch (...) {}
            }

            for (int i = 0; i < 16; ++i) {
                if (m_StopPollingRequested.load()) break;
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    if (!m_IsLoggedIn) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }

        m_IsPollingPayment.store(false);

        bool stillLoggedIn = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            stillLoggedIn = m_IsLoggedIn;
        }

        if (stillLoggedIn) {
            if (paymentCompleted) {
                if (onComplete) onComplete(true, resultMsg);
            } else if (!m_StopPollingRequested.load()) {
                if (resultMsg.empty()) {
                    resultMsg = "Tiempo de espera agotado. Si ya completaste el pago en Flow, pulsa 'Comprobar suscripción ahora'.";
                }
                if (onComplete) onComplete(false, resultMsg);
            }
        }
    }).detach();
}

void AuthLicenseManager::CheckFlowSubscriptionActive(std::function<void(bool isPro, const std::string& msg)> onComplete) {
    std::string userEmail;
    std::string localId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (!m_IsLoggedIn || m_User.email.empty()) {
            if (onComplete) onComplete(false, "No hay sesión de usuario iniciada.");
            return;
        }
        userEmail = m_User.email;
        localId = m_User.localId;
    }

    std::thread([this, userEmail, localId, onComplete]() {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!m_IsLoggedIn) {
                if (onComplete) onComplete(false, "Sesión cerrada.");
                return;
            }
        }

        std::string lowerUserEmail = userEmail;
        for (char& c : lowerUserEmail) c = (char)tolower((unsigned char)c);

        int statusCode = 0;
        std::string reqError;

        // 1. Obtener lista de clientes de Flow
        std::string toSignCust = "apiKey" + m_FlowApiKey;
        std::string sCust = FlowCrypto::HmacSha256(m_FlowSecretKey, toSignCust);
        std::string custPath = "/api/customer/list?apiKey=" + UrlEncode(m_FlowApiKey) + "&s=" + sCust;

        std::string custRes = PerformHttpsRequest("GET", "www.flow.cl", custPath, "", "application/json", "", statusCode, reqError);
        std::string matchedCustomerId;

        if (statusCode == 200) {
            try {
                json jCust = json::parse(custRes);
                if (jCust.contains("data") && jCust["data"].is_array()) {
                    for (const auto& item : jCust["data"]) {
                        std::string email = JsonSafeString(item, "email", "");
                        for (char& c : email) c = (char)tolower((unsigned char)c);
                        if (email == lowerUserEmail) {
                            matchedCustomerId = JsonSafeString(item, "customerId", "");
                            break;
                        }
                    }
                }
            } catch (...) {}
        }

        if (!matchedCustomerId.empty()) {
            int subStatusCode = 0;
            std::string subReqError;
            std::string toSignSub = "apiKey" + m_FlowApiKey + "planId" + m_FlowPlanId;
            std::string sSub = FlowCrypto::HmacSha256(m_FlowSecretKey, toSignSub);
            std::string subPath = "/api/subscription/list?apiKey=" + UrlEncode(m_FlowApiKey) + "&planId=" + UrlEncode(m_FlowPlanId) + "&s=" + sSub;

            std::string subRes = PerformHttpsRequest("GET", "www.flow.cl", subPath, "", "application/json", "", subStatusCode, subReqError);
            bool foundSubForCustomer = false;
            bool isSubActiveAndPaid   = false;
            bool hasPendingPayment   = false;
            std::string activeSubId;
            int64_t invEndTime       = 0;
            int64_t now              = (int64_t)std::time(nullptr);

            if (subStatusCode == 200) {
                try {
                    json jSub = json::parse(subRes);
                    if (jSub.contains("data") && jSub["data"].is_array()) {
                        for (const auto& sub : jSub["data"]) {
                            std::string cId = JsonSafeString(sub, "customerId", JsonSafeString(sub, "customer_id", ""));
                            if (cId != matchedCustomerId) continue;

                            foundSubForCustomer = true;
                            std::string subId = JsonSafeString(sub, "subscriptionId", JsonSafeString(sub, "subscription_id", ""));
                            int64_t subStatus = JsonSafeInt64(sub, "status", 0);
                            int64_t subMorose = JsonSafeInt64(sub, "morose", 0);

                            // En Flow: 1 = Activa, 2 = Inactiva, 3 = Suspendida, 4 = Cancelada
                            if (subStatus == 4) {
                                // Suscripción expresamente cancelada: ignorar
                                continue;
                            }

                            // Consultar /subscription/get para auditar las facturas pagadas vs estado de cuenta
                            int getStatus = 0;
                            std::string getErr;
                            std::string toSignGet = "apiKey" + m_FlowApiKey + "subscriptionId" + subId;
                            std::string sGet = FlowCrypto::HmacSha256(m_FlowSecretKey, toSignGet);
                            std::string getPath = "/api/subscription/get?apiKey=" + UrlEncode(m_FlowApiKey) + "&subscriptionId=" + UrlEncode(subId) + "&s=" + sGet;
                            std::string getRes = PerformHttpsRequest("GET", "www.flow.cl", getPath, "", "application/json", "", getStatus, getErr);

                            int64_t detailStatus = subStatus;
                            int64_t morose = subMorose;
                            int64_t curPeriodEnd = 0;
                            bool subHasPaidInvoice = false;
                            bool subHasPendingInvoice = false;

                            if (getStatus == 200) {
                                try {
                                    json jDetail = json::parse(getRes);
                                    detailStatus = JsonSafeInt64(jDetail, "status", subStatus);
                                    morose = JsonSafeInt64(jDetail, "morose", subMorose);

                                    std::string pEndStr = JsonSafeString(jDetail, "period_end", "");
                                    if (pEndStr.empty()) {
                                        pEndStr = JsonSafeString(jDetail, "next_invoice_date", "");
                                    }
                                    curPeriodEnd = ParseIsoDate(pEndStr);

                                    if (jDetail.contains("invoices") && jDetail["invoices"].is_array()) {
                                        for (const auto& inv : jDetail["invoices"]) {
                                            int64_t invStatus = JsonSafeInt64(inv, "status", 0);
                                            // En Flow: 1 = Pendiente, 2 = Pagada
                                            if (invStatus == 2) {
                                                subHasPaidInvoice = true;
                                                std::string pEnd = JsonSafeString(inv, "period_end", "");
                                                int64_t pe = ParseIsoDate(pEnd);
                                                if (pe > curPeriodEnd) curPeriodEnd = pe;
                                            } else if (invStatus == 1) {
                                                subHasPendingInvoice = true;
                                            }
                                        }
                                    }
                                } catch (...) {}
                            }

                            if (curPeriodEnd <= 0 && detailStatus == 1) {
                                curPeriodEnd = now + (30LL * 24 * 3600);
                            }

                            // Criterio de suscripción ACTIVA en Flow:
                            // 1. Factura efectivamente pagada (status: 2) con período vigente
                            // O BIEN
                            // 2. Suscripción en estado Activa (status: 1) y sin mora (morose: 0 / Pendiente de Pago: No en Flow)
                            //    lo que cubre pagos manuales/externos registrados por el administrador en Flow y cargos automáticos al día.
                            if ((subHasPaidInvoice || (detailStatus == 1 && morose == 0)) && curPeriodEnd > now) {
                                isSubActiveAndPaid = true;
                                activeSubId = subId;
                                if (curPeriodEnd > invEndTime) invEndTime = curPeriodEnd;
                                break;
                            } else if (detailStatus == 1 && morose == 1) {
                                // Suscripción activa pero morosa (pendiente de pago en Flow)
                                hasPendingPayment = true;
                            } else if (subHasPendingInvoice && morose != 0) {
                                hasPendingPayment = true;
                            }
                        }
                    }
                } catch (...) {}
            }

            // Exclusivamente si la suscripción está efectivamente ACTIVA y vigente en Flow
            if (isSubActiveAndPaid && invEndTime > now) {
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    if (!m_IsLoggedIn) return;
                    m_License.isValid = true;
                    m_License.status = "active";
                    m_License.productName = "ProyecThor for Professionals";
                    m_License.customerEmail = userEmail;
                    m_License.customerName = m_User.displayName.empty() ? userEmail : m_User.displayName;
                    m_License.activatedAt = now;
                    m_License.expiresAt = invEndTime;
                    m_License.expiryDate = FormatDate(invEndTime);
                    m_License.lastValidatedAt = now;
                    m_LicenseError.clear();
                    m_PendingFlowToken.clear();
                    m_PendingCommerceOrder.clear();
                }

                UpdateFirestoreProStatus(localId, true, activeSubId, invEndTime);
                SaveState();
                if (onComplete) onComplete(true, "Suscripción verificada y activa en Flow: ProyecThor for Professionals habilitado.");
                return;
            }

            if (foundSubForCustomer) {
                // Flow es la fuente primaria de verdad: el usuario tiene suscripción en Flow pero NO está pagada en Webpay
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    if (!m_IsLoggedIn) return;
                    m_License.isValid = false;
                    m_License.status = hasPendingPayment ? "unpaid" : "inactive";
                    m_LicenseError = hasPendingPayment 
                        ? "Suscripción registrada en Flow pero con pago pendiente en Webpay. Completa el pago para activar Pro."
                        : "Suscripción inactiva o no pagada en Flow. Realiza el pago para activar Pro.";
                }
                UpdateFirestoreProStatus(localId, false, "", 0);
                SaveState();
                if (onComplete) onComplete(false, m_LicenseError);
                return;
            }
        }

        if (onComplete) onComplete(false, "No se encontró suscripción pagada en Flow.");
    }).detach();
}

void AuthLicenseManager::UpdateFirestoreProStatus(const std::string& localId, bool isPro,
                                                  const std::string& flowOrder, int64_t expiresAt) {
    if (localId.empty()) return;

    std::string apiKey = GetFirebaseApiKey();
    std::string idToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
    }

    if (idToken.empty()) return;

    std::thread([this, localId, isPro, flowOrder, expiresAt, apiKey, idToken]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId +
                           "?updateMask.fieldPaths=isPro&updateMask.fieldPaths=licenseActive&updateMask.fieldPaths=flowOrder&updateMask.fieldPaths=expiresAt&key=" + apiKey;

        json payload;
        payload["fields"]["isPro"]["booleanValue"] = isPro;
        payload["fields"]["licenseActive"]["booleanValue"] = isPro;
        payload["fields"]["flowOrder"]["stringValue"] = flowOrder;
        payload["fields"]["expiresAt"]["integerValue"] = std::to_string(expiresAt);

        std::string curToken = idToken;
        PerformHttpsRequest("PATCH", "firestore.googleapis.com", path, payload.dump(), "application/json", curToken, statusCode, reqError);
        // Si el token expiró (HTTP 401), renovarlo automáticamente y reintentar
        if (statusCode == 401) {
            std::string refreshedToken, refreshErr;
            if (RefreshFirebaseIdToken(refreshedToken, refreshErr)) {
                PerformHttpsRequest("PATCH", "firestore.googleapis.com", path, payload.dump(), "application/json", refreshedToken, statusCode, reqError);
            }
        }
    }).detach();
}

// ── Licenciamiento y Validación (Cloud Firestore / Flow) ────────────────────

bool AuthLicenseManager::IsPro() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_IsLoggedIn) {
        return false;
    }
    if (m_IsAdmin) {
        return true;
    }
    if (!m_License.isValid) {
        return false;
    }
    int64_t now = (int64_t)std::time(nullptr);
    // Anti-trampa: si el reloj del sistema fue alterado hacia atrás más de 24 horas
    if (m_License.lastValidatedAt > 0 && now < (m_License.lastValidatedAt - 86400)) {
        return false;
    }
    // Si la licencia tiene expiración y ya venció
    if (m_License.expiresAt > 0 && now >= m_License.expiresAt) {
        return false;
    }
    return true;
}

bool AuthLicenseManager::IsAdmin() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_IsLoggedIn) return false;
    return m_IsAdmin;
}

void AuthLicenseManager::SetAdmin(bool admin) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_IsAdmin = admin;
    }
    SaveState();
}

void AuthLicenseManager::CheckFirestoreUserPro(const std::string& localId,
                                              std::function<void(bool isPro, const std::string& msg)> onComplete) {
    if (localId.empty()) {
        if (onComplete) onComplete(false, "No hay sesión de usuario activa.");
        return;
    }

    m_LicenseBusy.store(true);
    std::string apiKey = GetFirebaseApiKey();
    std::string idToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
    }

    std::thread([this, localId, apiKey, idToken, onComplete]() {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!m_IsLoggedIn) {
                m_LicenseBusy.store(false);
                if (onComplete) onComplete(false, "Sesión cerrada.");
                return;
            }
        }

        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId + "?key=" + apiKey;

        std::string curToken = idToken;
        std::string res = PerformHttpsRequest("GET", "firestore.googleapis.com", path, "", "application/json", curToken, statusCode, reqError);

        // Si Firebase responde 401 Unauthorized (token expirado tras 1 hora), renovar automáticamente y reintentar
        if (statusCode == 401) {
            std::string refreshedToken, refreshErr;
            if (RefreshFirebaseIdToken(refreshedToken, refreshErr)) {
                curToken = refreshedToken;
                statusCode = 0;
                reqError.clear();
                res = PerformHttpsRequest("GET", "firestore.googleapis.com", path, "", "application/json", curToken, statusCode, reqError);
            }
        }

        bool isPro = false;
        std::string finalMsg;

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                if (j.contains("fields") && j["fields"].is_object()) {
                    const auto& f = j["fields"];
                    bool isAdmin = false;
                    if (f.contains("role") && f["role"].is_object() && f["role"].contains("stringValue")) {
                        if (f["role"]["stringValue"].get<std::string>() == "admin") {
                            isAdmin = true;
                        }
                    }
                    if (f.contains("role") && f["role"].is_string()) {
                        if (f["role"].get<std::string>() == "admin") {
                            isAdmin = true;
                        }
                    }
                    std::string emailLower = m_User.email;
                    std::transform(emailLower.begin(), emailLower.end(), emailLower.begin(), ::tolower);
                    if (emailLower.find("vixcho") != std::string::npos || emailLower.find("thevixcho") != std::string::npos) {
                        isAdmin = true;
                    }
                    bool isBanned = false;
                    if (f.contains("banned") && f["banned"].is_object() && f["banned"].contains("booleanValue")) {
                        isBanned = f["banned"]["booleanValue"].get<bool>();
                    }

                    int64_t now = (int64_t)std::time(nullptr);

                    if (isAdmin && !isBanned) {
                        isPro = true;
                        int64_t finalExp = now + (365LL * 24 * 3600);
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            if (!m_IsLoggedIn) {
                                m_LicenseBusy.store(false);
                                return;
                            }
                            m_IsAdmin                 = true;
                            m_License.isValid         = true;
                            m_License.status          = "active";
                            m_License.productName     = "ProyecThor for Professionals";
                            m_License.customerEmail   = m_User.email;
                            m_License.customerName    = m_User.displayName.empty() ? m_User.email : m_User.displayName;
                            m_License.lastValidatedAt = now;
                            m_License.expiresAt       = finalExp;
                            m_License.expiryDate      = FormatDate(finalExp);
                            m_LicenseError.clear();
                        }
                        finalMsg = "Cuenta con rol Administrador: ProyecThor for Professionals habilitado.";
                    } else {
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            m_IsAdmin = false;
                        }

                        // ── Comprobación de Sesión Concurrente (1 PC a la vez) ──
                        std::string remoteSessionId;
                        if (f.contains("activeSessionId") && f["activeSessionId"].is_object() && f["activeSessionId"].contains("stringValue")) {
                            remoteSessionId = f["activeSessionId"]["stringValue"].get<std::string>();
                        }
                        std::string remoteDeviceName;
                        if (f.contains("activeDeviceName") && f["activeDeviceName"].is_object() && f["activeDeviceName"].contains("stringValue")) {
                            remoteDeviceName = f["activeDeviceName"]["stringValue"].get<std::string>();
                        }

                        if (remoteSessionId.empty()) {
                            ClaimActiveSession(localId);
                        } else if (remoteSessionId != m_SessionId) {
                            m_SessionKicked.store(true);
                            {
                                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                                m_KickedByDevice = remoteDeviceName.empty() ? "Otro equipo" : remoteDeviceName;
                                m_KickedMessage = "Tu cuenta se ha abierto en otra computadora (" + m_KickedByDevice + "). Tu suscripción Pro solo permite el uso en 1 PC a la vez.";
                                m_License.isValid = false;
                                m_License.status = "session_kicked";
                                m_LicenseError = m_KickedMessage;
                            }
                            SaveState();
                            m_LicenseBusy.store(false);
                            if (onComplete) onComplete(false, m_KickedMessage);
                            return;
                        }

                        // Usuario regular: Flow es la autoridad exclusiva para verificar pagos.
                        // Aunque en Firestore figure isPro: true por edición manual o remanente,
                        // si Flow no reporta factura pagada, la cuenta queda estrictamente en Edición Base.
                        isPro = false;
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            if (!m_IsLoggedIn) {
                                m_LicenseBusy.store(false);
                                return;
                            }
                            m_License.isValid   = false;
                            m_License.expiresAt = 0;
                            m_License.expiryDate.clear();
                            if (m_License.status != "unpaid") {
                                m_License.status = "inactive";
                            }
                            if (m_LicenseError.empty()) {
                                m_LicenseError = "Edición Base Gratuita. Activa tu suscripción en Flow para acceder a Pro.";
                            }
                            finalMsg = m_LicenseError;
                        }
                        UpdateFirestoreProStatus(localId, false, "", 0);
                    }
                }
                SaveState();
            } catch (...) {
                finalMsg = "Error al procesar la respuesta del servidor de licencias.";
            }
        } else if (statusCode == 404 || statusCode == 403) {
            // El usuario aún no tiene el documento en Firestore o no tiene isPro = true
            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                m_License.isValid = false;
                m_License.status  = "inactive";
                m_LicenseError.clear();
            }
            finalMsg = "Cuenta verificada: Edición Base gratuita activa.";
        } else if (statusCode == 401) {
            finalMsg = "Sesión de Firebase expirada. Inicia sesión nuevamente con tu correo.";
        } else {
            finalMsg = "Fallo de conexión con el servidor (HTTP " + std::to_string(statusCode) + ").";
        }

        m_LicenseBusy.store(false);
        if (onComplete) onComplete(isPro, finalMsg);
    }).detach();
}

void AuthLicenseManager::RefreshProStatus(std::function<void(bool isPro, const std::string& msg)> onComplete) {
    std::string localId;
    std::string pendingToken;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (!m_IsLoggedIn || m_User.localId.empty()) {
            if (onComplete) onComplete(false, "Inicia sesión con tu cuenta para comprobar tu estado Pro.");
            return;
        }
        localId = m_User.localId;
        pendingToken = m_PendingFlowToken;
    }

    if (!pendingToken.empty()) {
        std::thread([this, localId, pendingToken, onComplete]() {
            int statusCode = 0;
            std::string reqError;
            std::string toSign = "apiKey" + m_FlowApiKey + "token" + pendingToken;
            std::string s = FlowCrypto::HmacSha256(m_FlowSecretKey, toSign);
            std::string path = "/api/payment/getStatus?apiKey=" + UrlEncode(m_FlowApiKey) + "&token=" + UrlEncode(pendingToken) + "&s=" + s;

            std::string res = PerformHttpsRequest("GET", "www.flow.cl", path, "", "application/json", "", statusCode, reqError);
            if (statusCode == 200) {
                try {
                    json j = json::parse(res);
                    int64_t status = JsonSafeInt64(j, "status", 0);
                    int64_t flowOrder = JsonSafeInt64(j, "flowOrder", 0);
                    if (status == 2) {
                        int64_t now = (int64_t)std::time(nullptr);
                        int64_t expiresAt = now + (30LL * 24 * 3600);
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            m_License.isValid = true;
                            m_License.status = "active";
                            m_License.productName = "ProyecThor for Professionals";
                            m_License.customerEmail = m_User.email;
                            m_License.customerName = m_User.displayName.empty() ? m_User.email : m_User.displayName;
                            m_License.activatedAt = now;
                            m_License.expiresAt = expiresAt;
                            m_License.expiryDate = FormatDate(expiresAt);
                            m_License.lastValidatedAt = now;
                            m_LicenseError.clear();
                            m_PendingFlowToken.clear();
                            m_PendingCommerceOrder.clear();
                        }
                        UpdateFirestoreProStatus(localId, true, std::to_string(flowOrder), expiresAt);
                        SaveState();
                        if (onComplete) onComplete(true, "¡Pago confirmado en Flow! ProyecThor for Professionals activo.");
                        return;
                    } else if (status == 3 || status == 4) {
                        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                        m_PendingFlowToken.clear();
                        m_PendingCommerceOrder.clear();
                        SaveState();
                    }
                } catch (...) {}
            }

            CheckFirestoreUserPro(localId, [this, localId, onComplete](bool firestorePro, const std::string& fMsg) {
                if (m_IsAdmin) {
                    if (onComplete) onComplete(true, "Cuenta con rol Administrador.");
                    return;
                }
                CheckFlowSubscriptionActive([this, localId, onComplete, firestorePro, fMsg](bool flowPro, const std::string& flowMsg) {
                    if (flowPro) {
                        if (onComplete) onComplete(true, flowMsg);
                    } else if (onComplete) {
                        onComplete(firestorePro, fMsg);
                    }
                });
            });
        }).detach();
        return;
    }

    CheckFirestoreUserPro(localId, [this, localId, onComplete](bool firestorePro, const std::string& fMsg) {
        if (m_IsAdmin) {
            if (onComplete) onComplete(true, "Cuenta con rol Administrador.");
            return;
        }
        CheckFlowSubscriptionActive([this, localId, onComplete, firestorePro, fMsg](bool flowPro, const std::string& flowMsg) {
            if (flowPro) {
                if (onComplete) onComplete(true, flowMsg);
            } else if (onComplete) {
                onComplete(firestorePro, fMsg);
            }
        });
    });
}

bool AuthLicenseManager::IsLicenseExpired() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_License.isValid || m_License.expiresAt <= 0) return true;
    int64_t now = (int64_t)std::time(nullptr);
    return now >= m_License.expiresAt;
}

int64_t AuthLicenseManager::GetDaysRemaining() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_License.isValid) return 0;
    int64_t now = (int64_t)std::time(nullptr);
    if (m_License.expiresAt <= 0) return 30;
    if (now >= m_License.expiresAt) return 0;
    int64_t diffSeconds = m_License.expiresAt - now;
    // Redondeo hacia arriba: si quedan 29 días y 12 horas, son 30 días restantes de acceso
    return (diffSeconds + 86399) / 86400;
}

const LicenseInfo& AuthLicenseManager::GetLicenseInfo() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_License;
}

const std::string& AuthLicenseManager::GetLicenseError() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_LicenseError;
}

bool AuthLicenseManager::IsLicenseBusy() const {
    return m_LicenseBusy.load();
}

void AuthLicenseManager::ActivateLicense(const std::string& licenseKey, const std::string& instanceName,
                                        std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_LicenseBusy.exchange(true)) return;

    // Sanitizar clave: recortar espacios en blanco, saltos de línea y tabulaciones
    std::string cleanKey = licenseKey;
    while (!cleanKey.empty() && (cleanKey.back() == ' ' || cleanKey.back() == '\r' || cleanKey.back() == '\n' || cleanKey.back() == '\t')) cleanKey.pop_back();
    while (!cleanKey.empty() && (cleanKey.front() == ' ' || cleanKey.front() == '\r' || cleanKey.front() == '\n' || cleanKey.front() == '\t')) cleanKey.erase(cleanKey.begin());

    std::string finalInstName = instanceName.empty() ? GetDefaultInstanceName() : instanceName;

    std::thread([this, cleanKey, finalInstName, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string body = "license_key=" + UrlEncode(cleanKey) + "&instance_name=" + UrlEncode(finalInstName);

        std::string res = PerformHttpsPost(m_LemonHost, "/v1/licenses/activate", body, "application/x-www-form-urlencoded", statusCode, reqError);

        bool success = false;
        std::string finalMsg;
        std::string activateErrMsg;

        if (!reqError.empty()) {
            finalMsg = reqError;
        } else {
            // Intentar leer si /v1/licenses/activate fue exitoso
            if (statusCode == 200) {
                try {
                    json j = json::parse(res);
                    bool activated = JsonSafeBool(j, "activated", false);
                    if (activated) {
                        int64_t now = (int64_t)std::time(nullptr);
                        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                        m_License.isValid      = true;
                        m_License.key          = cleanKey;
                        m_License.instanceName = finalInstName;
                        m_License.activatedAt  = now;
                        m_License.lastValidatedAt = now;

                        if (j.contains("instance") && j["instance"].is_object()) {
                            const auto& inst = j["instance"];
                            if (inst.contains("id")) {
                                if (inst["id"].is_string()) m_License.instanceId = inst["id"].get<std::string>();
                                else if (inst["id"].is_number()) m_License.instanceId = std::to_string(inst["id"].get<int64_t>());
                            }
                        }
                        if (j.contains("license_key") && j["license_key"].is_object()) {
                            const auto& lk = j["license_key"];
                            m_License.status = JsonSafeString(lk, "status", "active");
                            std::string rawExp = JsonSafeString(lk, "expires_at", "");
                            int64_t parsedExp = ParseIsoDate(rawExp);
                            if (parsedExp > now) {
                                m_License.expiresAt = parsedExp;
                            } else {
                                // Por defecto: 1 mes de validez (30 días)
                                m_License.expiresAt = now + (30LL * 24 * 3600);
                            }
                            m_License.expiryDate = FormatDate(m_License.expiresAt);
                        } else {
                            m_License.status    = "active";
                            m_License.expiresAt = now + (30LL * 24 * 3600);
                            m_License.expiryDate = FormatDate(m_License.expiresAt);
                        }
                        if (j.contains("meta") && j["meta"].is_object()) {
                            const auto& meta = j["meta"];
                            m_License.customerName  = JsonSafeString(meta, "customer_name", JsonSafeString(meta, "user_name", ""));
                            m_License.customerEmail = JsonSafeString(meta, "customer_email", JsonSafeString(meta, "user_email", ""));
                            m_License.productName   = JsonSafeString(meta, "product_name", "ProyecThor for Professionals");
                        }
                        m_LicenseError.clear();
                        success = true;
                        finalMsg = "¡Licencia activada con éxito! ProyecThor for Professionals está activo por 1 mes.";
                    } else {
                        activateErrMsg = JsonSafeString(j, "error", "");
                    }
                } catch (...) {
                    activateErrMsg = "Respuesta de Lemon Squeezy no válida.";
                }
            } else {
                try {
                    json j = json::parse(res);
                    activateErrMsg = JsonSafeString(j, "error", "");
                } catch (...) {
                    activateErrMsg = "Error HTTP " + std::to_string(statusCode) + " al activar la licencia.";
                }
            }

            // Si activate falló (por ejemplo: límite de activación alcanzado en Lemon Squeezy):
            // Consultamos directamente /v1/licenses/validate. Si la clave es válida y está activa en los servidores,
            // la vinculamos y permitimos al usuario utilizar su suscripción legítima.
            if (!success) {
                int vStatus = 0;
                std::string vErr;
                std::string vBody = "license_key=" + UrlEncode(cleanKey);
                std::string vRes = PerformHttpsPost(m_LemonHost, "/v1/licenses/validate", vBody, "application/x-www-form-urlencoded", vStatus, vErr);
                if (vStatus == 200) {
                    try {
                        json vj = json::parse(vRes);
                        bool isValid = JsonSafeBool(vj, "valid", false);
                        std::string licStatus = "";
                        if (vj.contains("license_key") && vj["license_key"].is_object()) {
                            licStatus = JsonSafeString(vj["license_key"], "status", "");
                        }

                        // Considerar válida si valid es true y el estado no es revoked/expired/disabled
                        if (isValid && (licStatus == "active" || licStatus.empty())) {
                            int64_t now = (int64_t)std::time(nullptr);
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            m_License.isValid      = true;
                            m_License.key          = cleanKey;
                            m_License.instanceName = finalInstName;
                            m_License.activatedAt  = now;
                            m_License.lastValidatedAt = now;

                            if (vj.contains("instance") && vj["instance"].is_object()) {
                                const auto& inst = vj["instance"];
                                if (inst.contains("id")) {
                                    if (inst["id"].is_string()) m_License.instanceId = inst["id"].get<std::string>();
                                    else if (inst["id"].is_number()) m_License.instanceId = std::to_string(inst["id"].get<int64_t>());
                                }
                            }
                            if (vj.contains("license_key") && vj["license_key"].is_object()) {
                                const auto& lk = vj["license_key"];
                                m_License.status = licStatus.empty() ? "active" : licStatus;
                                std::string rawExp = JsonSafeString(lk, "expires_at", "");
                                int64_t parsedExp = ParseIsoDate(rawExp);
                                if (parsedExp > now) {
                                    m_License.expiresAt = parsedExp;
                                } else {
                                    m_License.expiresAt = now + (30LL * 24 * 3600);
                                }
                                m_License.expiryDate = FormatDate(m_License.expiresAt);
                            } else {
                                m_License.status    = "active";
                                m_License.expiresAt = now + (30LL * 24 * 3600);
                                m_License.expiryDate = FormatDate(m_License.expiresAt);
                            }
                            if (vj.contains("meta") && vj["meta"].is_object()) {
                                const auto& meta = vj["meta"];
                                m_License.customerName  = JsonSafeString(meta, "customer_name", JsonSafeString(meta, "user_name", ""));
                                m_License.customerEmail = JsonSafeString(meta, "customer_email", JsonSafeString(meta, "user_email", ""));
                                m_License.productName   = JsonSafeString(meta, "product_name", "ProyecThor for Professionals");
                            }
                            m_LicenseError.clear();
                            success = true;
                            finalMsg = "¡Licencia vinculada con éxito! ProyecThor for Professionals está activo por 1 mes.";
                        } else {
                            std::string vErrMsg = JsonSafeString(vj, "error", "");
                            finalMsg = !vErrMsg.empty() ? vErrMsg : (!activateErrMsg.empty() ? activateErrMsg : "La clave de licencia no es válida o ha caducado.");
                        }
                    } catch (...) {
                        finalMsg = !activateErrMsg.empty() ? activateErrMsg : "Error al procesar la respuesta del servidor.";
                    }
                } else {
                    finalMsg = !activateErrMsg.empty() ? activateErrMsg : "No se pudo contactar con el servidor de licencias.";
                }
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!success) m_LicenseError = finalMsg;
            else m_LicenseError.clear();
        }

        if (success) SaveState();
        m_LicenseBusy.store(false);

        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::ValidateLicense(std::function<void(bool success, const std::string& msg)> onComplete) {
    std::string key, instId;
    int64_t curExpiresAt = 0;
    bool wasValid = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        key = m_License.key;
        instId = m_License.instanceId;
        curExpiresAt = m_License.expiresAt;
        wasValid = m_License.isValid;
    }
    if (key.empty()) {
        if (onComplete) onComplete(false, "No hay ninguna clave de licencia registrada.");
        return;
    }

    m_LicenseBusy.store(true);
    std::thread([this, key, instId, curExpiresAt, wasValid, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string body = "license_key=" + UrlEncode(key);
        if (!instId.empty()) body += "&instance_id=" + UrlEncode(instId);

        std::string res = PerformHttpsPost(m_LemonHost, "/v1/licenses/validate", body, "application/x-www-form-urlencoded", statusCode, reqError);

        // Si falló con instance_id, probamos directamente con license_key
        if (statusCode == 200 && !instId.empty()) {
            try {
                json j = json::parse(res);
                if (!JsonSafeBool(j, "valid", false)) {
                    int sc2 = 0;
                    std::string re2;
                    std::string res2 = PerformHttpsPost(m_LemonHost, "/v1/licenses/validate", "license_key=" + UrlEncode(key),
                                                        "application/x-www-form-urlencoded", sc2, re2);
                    if (sc2 == 200) {
                        json j2 = json::parse(res2);
                        if (JsonSafeBool(j2, "valid", false)) {
                            res = res2;
                        }
                    }
                }
            } catch (...) {}
        }

        bool success = false;
        std::string finalMsg;
        int64_t now = (int64_t)std::time(nullptr);

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                bool valid = JsonSafeBool(j, "valid", false);
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                m_License.lastValidatedAt = now;

                if (valid) {
                    // La suscripción está activa y pagada
                    m_License.isValid = true;
                    m_License.status  = "active";
                    if (j.contains("license_key") && j["license_key"].is_object()) {
                        const auto& lk = j["license_key"];
                        m_License.status = JsonSafeString(lk, "status", "active");
                        std::string rawExp = JsonSafeString(lk, "expires_at", "");
                        int64_t parsedExp = ParseIsoDate(rawExp);
                        if (parsedExp > now) {
                            m_License.expiresAt = parsedExp;
                        } else {
                            // Renovación extendida por 1 mes (30 días)
                            m_License.expiresAt = now + (30LL * 24 * 3600);
                        }
                        m_License.expiryDate = FormatDate(m_License.expiresAt);
                    } else if (m_License.expiresAt <= now) {
                        m_License.expiresAt = now + (30LL * 24 * 3600);
                        m_License.expiryDate = FormatDate(m_License.expiresAt);
                    }
                    if (j.contains("meta") && j["meta"].is_object()) {
                        const auto& meta = j["meta"];
                        std::string cname = JsonSafeString(meta, "customer_name", JsonSafeString(meta, "user_name", ""));
                        std::string cemail = JsonSafeString(meta, "customer_email", JsonSafeString(meta, "user_email", ""));
                        if (!cname.empty()) m_License.customerName = cname;
                        if (!cemail.empty()) m_License.customerEmail = cemail;
                    }
                    m_LicenseError.clear();
                    success = true;
                    finalMsg = "Licencia válida y activa. Suscripción vigente hasta: " + m_License.expiryDate;
                } else {
                    // Lemon Squeezy respondió valid = false (cancelada, reembolsada, suspendida o expirada)
                    m_License.isValid = false;
                    m_License.status  = (curExpiresAt > 0 && now >= curExpiresAt) ? "expired" : "inactive";
                    m_LicenseError    = "La suscripción no está activa en los servidores de Lemon Squeezy.";
                    success = false;
                    finalMsg = "Licencia inactiva o revocada en los servidores.";
                }
            } catch (...) {
                finalMsg = "Error al procesar la respuesta de Lemon Squeezy.";
            }
            SaveState();
        } else {
            // Error de conexión o servidor inalcanzable
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (wasValid) {
                // Sin margen de gracia: si ya se cumplió el mes (now >= expiresAt), requiere wifi para pagar y renovar
                if (curExpiresAt > 0 && now >= curExpiresAt) {
                    m_License.isValid = false;
                    m_License.status  = "expired";
                    m_LicenseError    = "El período de suscripción ha finalizado. Conéctate a internet para validar el pago.";
                    success = false;
                    finalMsg = m_LicenseError;
                    SaveState();
                } else {
                    // Aún está dentro de su mes pagado: se mantiene activo localmente
                    m_License.isValid = true;
                    success = true;
                    finalMsg = "Sin conexión con el servidor. La suscripción sigue activa localmente.";
                }
            } else {
                finalMsg = "No se pudo contactar con el servidor de licencias.";
            }
        }

        m_LicenseBusy.store(false);
        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::ValidateOnAppLoaded() {
    std::string localId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (m_IsLoggedIn && !m_User.localId.empty()) {
            localId = m_User.localId;
        }
    }

    if (!localId.empty()) {
        // Valida automáticamente el estado Pro en Flow y Firestore al iniciar la aplicación
        RefreshProStatus(nullptr);
    }
}

void AuthLicenseManager::DeactivateLicense(std::function<void(bool success, const std::string& msg)> onComplete) {
    std::string key, instId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        key = m_License.key;
        instId = m_License.instanceId;
    }
    if (key.empty() || instId.empty()) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_License = LicenseInfo();
        SaveState();
        if (onComplete) onComplete(true, "Licencia desvinculada localmente.");
        return;
    }

    m_LicenseBusy.store(true);
    std::thread([this, key, instId, onComplete]() {
        int statusCode = 0;
        std::string reqError;
        std::string body = "license_key=" + UrlEncode(key) + "&instance_id=" + UrlEncode(instId);

        PerformHttpsPost(m_LemonHost, "/v1/licenses/deactivate", body, "application/x-www-form-urlencoded", statusCode, reqError);

        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_License = LicenseInfo();
            m_LicenseError.clear();
        }
        SaveState();
        m_LicenseBusy.store(false);

        if (onComplete) onComplete(true, "Licencia desactivada y liberada para otros equipos.");
    }).detach();
}

// ── Control de Sesión Concurrente (1 PC a la vez) ────────────────────────────

bool AuthLicenseManager::IsSessionKicked() const {
    return m_SessionKicked.load();
}

std::string AuthLicenseManager::GetKickedByDevice() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_KickedByDevice;
}

std::string AuthLicenseManager::GetKickedMessage() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_KickedMessage;
}

void AuthLicenseManager::AcknowledgeSessionKicked() {
    m_SessionKicked.store(false);
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_KickedByDevice.clear();
    m_KickedMessage.clear();
}

void AuthLicenseManager::ReclaimActiveSession(std::function<void(bool success, const std::string& msg)> onComplete) {
    m_SessionKicked.store(false);
    std::string localId;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        localId = m_User.localId;
        m_KickedByDevice.clear();
        m_KickedMessage.clear();
    }
    if (localId.empty()) {
        if (onComplete) onComplete(false, "No hay sesión iniciada.");
        return;
    }

    ClaimActiveSession(localId);
    RefreshProStatus([onComplete](bool isPro, const std::string& msg) {
        if (onComplete) onComplete(isPro, msg);
    });
}

void AuthLicenseManager::ClaimActiveSession(const std::string& localId) {
    if (localId.empty()) return;

    std::string apiKey = GetFirebaseApiKey();
    std::string idToken;
    std::string sessId;
    std::string devName;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        idToken = m_User.idToken;
        sessId  = m_SessionId;
        devName = m_DeviceName;
    }
    if (idToken.empty() || sessId.empty()) return;

    std::thread([this, localId, apiKey, idToken, sessId, devName]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId +
                           "?updateMask.fieldPaths=activeSessionId&updateMask.fieldPaths=activeDeviceName&updateMask.fieldPaths=activeSessionAt&key=" + apiKey;

        json payload;
        payload["fields"]["activeSessionId"]["stringValue"] = sessId;
        payload["fields"]["activeDeviceName"]["stringValue"] = devName;
        payload["fields"]["activeSessionAt"]["integerValue"] = std::to_string((int64_t)std::time(nullptr));

        std::string curToken = idToken;
        PerformHttpsRequest("PATCH", "firestore.googleapis.com", path, payload.dump(), "application/json", curToken, statusCode, reqError);
        if (statusCode == 401) {
            std::string refreshedToken, refreshErr;
            if (RefreshFirebaseIdToken(refreshedToken, refreshErr)) {
                PerformHttpsRequest("PATCH", "firestore.googleapis.com", path, payload.dump(), "application/json", refreshedToken, statusCode, reqError);
            }
        }
    }).detach();
}

void AuthLicenseManager::CheckSessionHeartbeat() {
    int64_t now = (int64_t)std::time(nullptr);
    if (now - m_LastHeartbeatAt < 45) return; // Chequeo ligero cada 45 segundos
    m_LastHeartbeatAt = now;

    if (!m_IsLoggedIn || !m_License.isValid || m_IsAdmin || m_User.localId.empty()) return;
    if (m_HeartbeatBusy.exchange(true)) return;

    std::string localId;
    std::string apiKey = GetFirebaseApiKey();
    std::string idToken;
    std::string mySess;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        localId = m_User.localId;
        idToken = m_User.idToken;
        mySess  = m_SessionId;
    }

    std::thread([this, localId, apiKey, idToken, mySess]() {
        int statusCode = 0;
        std::string reqError;
        std::string path = "/v1/projects/proyecthor/databases/(default)/documents/users/" + localId + "?key=" + apiKey;

        std::string curToken = idToken;
        std::string res = PerformHttpsRequest("GET", "firestore.googleapis.com", path, "", "application/json", curToken, statusCode, reqError);
        if (statusCode == 401) {
            std::string refreshedToken, refreshErr;
            if (RefreshFirebaseIdToken(refreshedToken, refreshErr)) {
                curToken = refreshedToken;
                statusCode = 0;
                reqError.clear();
                res = PerformHttpsRequest("GET", "firestore.googleapis.com", path, "", "application/json", curToken, statusCode, reqError);
            }
        }

        if (statusCode == 200) {
            try {
                json j = json::parse(res);
                if (j.contains("fields") && j["fields"].is_object()) {
                    const auto& f = j["fields"];
                    std::string remoteSessionId;
                    if (f.contains("activeSessionId") && f["activeSessionId"].is_object() && f["activeSessionId"].contains("stringValue")) {
                        remoteSessionId = f["activeSessionId"]["stringValue"].get<std::string>();
                    }
                    std::string remoteDeviceName;
                    if (f.contains("activeDeviceName") && f["activeDeviceName"].is_object() && f["activeDeviceName"].contains("stringValue")) {
                        remoteDeviceName = f["activeDeviceName"]["stringValue"].get<std::string>();
                    }

                    if (!remoteSessionId.empty() && remoteSessionId != mySess) {
                        m_SessionKicked.store(true);
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                            m_KickedByDevice = remoteDeviceName.empty() ? "Otro equipo" : remoteDeviceName;
                            m_KickedMessage = "Tu cuenta se ha abierto en otra computadora (" + m_KickedByDevice + "). Tu suscripción Pro solo permite 1 PC a la vez.";
                            m_License.isValid = false;
                            m_License.status = "session_kicked";
                            m_LicenseError = m_KickedMessage;
                        }
                        SaveState();
                        AppLog::Warn("Auth", "Sesión Pro desplazada: iniciada en " + m_KickedByDevice);
                    }
                }
            } catch (...) {}
        }
        m_HeartbeatBusy.store(false);
    }).detach();
}

} // namespace ProyecThor::Core
