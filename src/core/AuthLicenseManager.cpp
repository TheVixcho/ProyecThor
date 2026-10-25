#include "AuthLicenseManager.h"
#include <fstream>
#include <thread>
#include <filesystem>
#include <sstream>
#include <iomanip>

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

using json = nlohmann::json;

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

// Helper para parsear fechas ISO 8601 de Lemon Squeezy a timestamp Unix
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
            char sepT = 0, sepColon1 = 0, sepColon2 = 0;
            if (ss >> sepT >> hour >> sepColon1 >> min >> sepColon2 >> sec) {
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
    // Clave de API web de Firebase por defecto (asociada al proyecto)
    m_FirebaseApiKey = "AIzaSyAfvX2q_3xOWG16apTsvmZwIPLqSVTRZOo";
}

void AuthLicenseManager::Initialize() {
    LoadState();

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_License.isValid && !m_License.key.empty()) {
        int64_t now = (int64_t)std::time(nullptr);
        // Si la licencia guardada no tenía fecha de expiración (versión previa),
        // se le otorga 1 mes (30 días) a partir de la activación o fecha actual
        if (m_License.expiresAt <= 0) {
            int64_t baseTime = (m_License.activatedAt > 0) ? m_License.activatedAt : now;
            m_License.expiresAt = baseTime + (30LL * 24 * 3600);
            if (m_License.expiryDate.empty()) {
                m_License.expiryDate = FormatDate(m_License.expiresAt);
            }
        }
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    json j;
    j["firebase_api_key"] = m_FirebaseApiKey;
    j["is_logged_in"]     = m_IsLoggedIn;
    j["user"] = {
        { "local_id",      m_User.localId },
        { "email",         m_User.email },
        { "display_name",  m_User.displayName },
        { "id_token",      m_User.idToken },
        { "refresh_token", m_User.refreshToken }
    };
    j["license"] = {
        { "is_valid",       m_License.isValid },
        { "key",            m_License.key },
        { "status",         m_License.status },
        { "instance_id",    m_License.instanceId },
        { "instance_name",  m_License.instanceName },
        { "customer_name",  m_License.customerName },
        { "customer_email", m_License.customerEmail },
        { "product_name",   m_License.productName },
        { "expiry_date",    m_License.expiryDate },
        { "activated_at",   m_License.activatedAt },
        { "expires_at",     m_License.expiresAt },
        { "last_validated", m_License.lastValidatedAt }
    };

    std::string path = GetConfigFilePath();
    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(2);
    }
}

void AuthLicenseManager::LoadState() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string path = GetConfigFilePath();
    if (!std::filesystem::exists(path)) return;

    std::ifstream f(path);
    if (!f.is_open()) return;

    try {
        json j = json::parse(f);
        if (j.contains("firebase_api_key") && j["firebase_api_key"].is_string()) {
            std::string k = j["firebase_api_key"];
            if (!k.empty()) m_FirebaseApiKey = k;
        }
        if (j.contains("is_logged_in")) m_IsLoggedIn = j["is_logged_in"].get<bool>();

        if (j.contains("user") && j["user"].is_object()) {
            const auto& u = j["user"];
            if (u.contains("local_id") && u["local_id"].is_string()) m_User.localId = u["local_id"];
            if (u.contains("email") && u["email"].is_string()) m_User.email = u["email"];
            if (u.contains("display_name") && u["display_name"].is_string()) m_User.displayName = u["display_name"];
            if (u.contains("id_token") && u["id_token"].is_string()) m_User.idToken = u["id_token"];
            if (u.contains("refresh_token") && u["refresh_token"].is_string()) m_User.refreshToken = u["refresh_token"];
        }

        if (j.contains("license") && j["license"].is_object()) {
            const auto& l = j["license"];
            if (l.contains("is_valid")) m_License.isValid = l["is_valid"].get<bool>();
            if (l.contains("key") && l["key"].is_string()) m_License.key = l["key"];
            if (l.contains("status") && l["status"].is_string()) m_License.status = l["status"];
            if (l.contains("instance_id") && l["instance_id"].is_string()) m_License.instanceId = l["instance_id"];
            if (l.contains("instance_name") && l["instance_name"].is_string()) m_License.instanceName = l["instance_name"];
            if (l.contains("customer_name") && l["customer_name"].is_string()) m_License.customerName = l["customer_name"];
            if (l.contains("customer_email") && l["customer_email"].is_string()) m_License.customerEmail = l["customer_email"];
            if (l.contains("product_name") && l["product_name"].is_string()) m_License.productName = l["product_name"];
            if (l.contains("expiry_date") && l["expiry_date"].is_string()) m_License.expiryDate = l["expiry_date"];
            if (l.contains("activated_at") && l["activated_at"].is_number()) m_License.activatedAt = l["activated_at"].get<int64_t>();
            if (l.contains("expires_at") && l["expires_at"].is_number()) m_License.expiresAt = l["expires_at"].get<int64_t>();
            if (l.contains("last_validated") && l["last_validated"].is_number()) m_License.lastValidatedAt = l["last_validated"].get<int64_t>();
        }
    } catch (...) {}
}

// ── Ejecución de peticiones HTTPS nativas en Windows (WinHTTP) ───────────────
std::string AuthLicenseManager::PerformHttpsPost(const std::string& host, const std::string& path,
                                                 const std::string& postData, const std::string& contentType,
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

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Fallo al abrir solicitud HTTPS.";
        return "";
    }

    // Cabecera Content-Type
    std::wstring wHeader = L"Content-Type: " + Utf8ToWide(contentType) + L"\r\nAccept: application/json\r\n";
    BOOL bSend = WinHttpSendRequest(hRequest,
                                    wHeader.c_str(), (DWORD)wHeader.length(),
                                    (LPVOID)postData.c_str(), (DWORD)postData.length(),
                                    (DWORD)postData.length(), 0);

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

// ── Métodos de Firebase Auth ─────────────────────────────────────────────────

bool AuthLicenseManager::IsLoggedIn() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_IsLoggedIn;
}

const UserProfile& AuthLicenseManager::GetUser() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_User;
}

const std::string& AuthLicenseManager::GetAuthError() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_AuthError;
}

bool AuthLicenseManager::IsAuthBusy() const {
    return m_AuthBusy.load();
}

void AuthLicenseManager::SetFirebaseApiKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_FirebaseApiKey = key;
}

std::string AuthLicenseManager::GetFirebaseApiKey() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_FirebaseApiKey;
}

void AuthLicenseManager::LoginWithFirebase(const std::string& email, const std::string& password,
                                          std::function<void(bool success, const std::string& msg)> onComplete) {
    if (m_AuthBusy.exchange(true)) return;

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
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
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_IsLoggedIn = true;
                m_User.localId      = j.value("localId", "");
                m_User.email        = j.value("email", email);
                m_User.displayName  = j.value("displayName", "");
                m_User.idToken      = j.value("idToken", "");
                m_User.refreshToken = j.value("refreshToken", "");
                m_AuthError.clear();
                success = true;
                finalMsg = "Sesión iniciada correctamente.";
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
            std::lock_guard<std::mutex> lock(m_Mutex);
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
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_IsLoggedIn = true;
                m_User.localId      = j.value("localId", "");
                m_User.email        = j.value("email", email);
                m_User.displayName  = j.value("displayName", "");
                m_User.idToken      = j.value("idToken", "");
                m_User.refreshToken = j.value("refreshToken", "");
                m_AuthError.clear();
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
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!success) m_AuthError = finalMsg;
        }

        if (success) SaveState();
        m_AuthBusy.store(false);

        if (onComplete) onComplete(success, finalMsg);
    }).detach();
}

void AuthLicenseManager::Logout() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_IsLoggedIn = false;
        m_User = UserProfile();
        m_AuthError.clear();
    }
    SaveState();
}

// ── Lemon Squeezy: Compra y Checkout ─────────────────────────────────────────

bool AuthLicenseManager::CanBuySubscription() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_IsLoggedIn;
}

bool AuthLicenseManager::OpenCheckoutUrl(std::string& outError) {
    std::string email;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_IsLoggedIn) {
            outError = "Debes iniciar sesión con tu correo y contraseña antes de comprar la suscripción.";
            return false;
        }
        email = m_User.email;
    }

    std::string finalUrl = m_CheckoutBaseUrl;
    if (!email.empty()) {
        finalUrl += "?checkout[email]=" + UrlEncode(email);
    }

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

// ── Lemon Squeezy: Licenciamiento ────────────────────────────────────────────

bool AuthLicenseManager::IsPro() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_License.isValid || m_License.key.empty()) {
        return false;
    }
    int64_t now = (int64_t)std::time(nullptr);
    // Anti-trampa: si el reloj del sistema fue alterado hacia atrás
    if (m_License.lastValidatedAt > 0 && now < (m_License.lastValidatedAt - 3600)) {
        return false;
    }
    // Si ya venció el mes: estricto, sin margen de gracia, requiere wifi para pagar y revalidar
    if (m_License.expiresAt > 0 && now >= m_License.expiresAt) {
        return false;
    }
    return true;
}

bool AuthLicenseManager::IsLicenseExpired() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_License.isValid || m_License.expiresAt <= 0) return true;
    int64_t now = (int64_t)std::time(nullptr);
    return now >= m_License.expiresAt;
}

int64_t AuthLicenseManager::GetDaysRemaining() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_License.isValid || m_License.expiresAt <= 0) return 0;
    int64_t now = (int64_t)std::time(nullptr);
    if (now >= m_License.expiresAt) return 0;
    return (m_License.expiresAt - now) / (24 * 3600);
}

const LicenseInfo& AuthLicenseManager::GetLicenseInfo() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_License;
}

const std::string& AuthLicenseManager::GetLicenseError() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
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
                    bool activated = j.value("activated", false);
                    if (activated) {
                        int64_t now = (int64_t)std::time(nullptr);
                        std::lock_guard<std::mutex> lock(m_Mutex);
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
                            m_License.status = j["license_key"].value("status", "active");
                            std::string rawExp = j["license_key"].value("expires_at", "");
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
                            m_License.customerName  = j["meta"].value("user_name", "");
                            m_License.customerEmail = j["meta"].value("user_email", "");
                            m_License.productName   = j["meta"].value("product_name", "ProyecThor for Professionals");
                        }
                        m_LicenseError.clear();
                        success = true;
                        finalMsg = "¡Licencia activada con éxito! ProyecThor for Professionals está activo por 1 mes.";
                    } else {
                        activateErrMsg = j.value("error", "");
                    }
                } catch (...) {
                    activateErrMsg = "Respuesta de Lemon Squeezy no válida.";
                }
            } else {
                try {
                    json j = json::parse(res);
                    activateErrMsg = j.value("error", "");
                } catch (...) {
                    activateErrMsg = "Error HTTP " + std::to_string(statusCode) + " al activar la licencia.";
                }
            }

            // Si activate falló (por ejemplo: error "This license key has reached the activation limit."
            // debido a que ya se había activado este equipo u otro anteriormente en Lemon Squeezy):
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
                        bool isValid = vj.value("valid", false);
                        std::string licStatus = "";
                        if (vj.contains("license_key") && vj["license_key"].is_object()) {
                            licStatus = vj["license_key"].value("status", "");
                        }

                        // Considerar válida si valid es true y el estado no es revoked/expired/disabled
                        if (isValid && (licStatus == "active" || licStatus.empty())) {
                            int64_t now = (int64_t)std::time(nullptr);
                            std::lock_guard<std::mutex> lock(m_Mutex);
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
                                m_License.status = licStatus.empty() ? "active" : licStatus;
                                std::string rawExp = vj["license_key"].value("expires_at", "");
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
                                m_License.customerName  = vj["meta"].value("user_name", "");
                                m_License.customerEmail = vj["meta"].value("user_email", "");
                                m_License.productName   = vj["meta"].value("product_name", "ProyecThor for Professionals");
                            }
                            m_LicenseError.clear();
                            success = true;
                            finalMsg = "¡Licencia vinculada con éxito! ProyecThor for Professionals está activo por 1 mes.";
                        } else {
                            std::string vErrMsg = vj.value("error", "");
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
            std::lock_guard<std::mutex> lock(m_Mutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
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
                if (!j.value("valid", false)) {
                    int sc2 = 0;
                    std::string re2;
                    std::string res2 = PerformHttpsPost(m_LemonHost, "/v1/licenses/validate", "license_key=" + UrlEncode(key),
                                                        "application/x-www-form-urlencoded", sc2, re2);
                    if (sc2 == 200) {
                        json j2 = json::parse(res2);
                        if (j2.value("valid", false)) {
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
                bool valid = j.value("valid", false);
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_License.lastValidatedAt = now;

                if (valid) {
                    // La suscripción está activa y pagada
                    m_License.isValid = true;
                    m_License.status  = "active";
                    if (j.contains("license_key") && j["license_key"].is_object()) {
                        std::string rawExp = j["license_key"].value("expires_at", "");
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
            std::lock_guard<std::mutex> lock(m_Mutex);
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
    std::string key;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_License.isValid || m_License.key.empty()) {
            return;
        }
        key = m_License.key;
    }

    // Consulta la licencia en el servidor como último paso tras cargar toda la app
    ValidateLicense([](bool /*success*/, const std::string& /*msg*/) {
        // Validación silenciosa en segundo plano
    });
}

void AuthLicenseManager::DeactivateLicense(std::function<void(bool success, const std::string& msg)> onComplete) {
    std::string key, instId;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        key = m_License.key;
        instId = m_License.instanceId;
    }
    if (key.empty() || instId.empty()) {
        std::lock_guard<std::mutex> lock(m_Mutex);
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
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_License = LicenseInfo();
            m_LicenseError.clear();
        }
        SaveState();
        m_LicenseBusy.store(false);

        if (onComplete) onComplete(true, "Licencia desactivada y liberada para otros equipos.");
    }).detach();
}

} // namespace ProyecThor::Core
