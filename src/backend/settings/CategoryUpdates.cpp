#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/FilePicker.h"
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
#endif

static std::string FetchURL(const std::wstring& host, const std::wstring& path) {
#if defined(_WIN32)
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
#else
    return "";
#endif
}

static void DoCheckUpdate(const std::string& currentVersion,
                           const std::string& channel,
                           bool showPopupIfAvailable) {
    s_Status = UpdateStatus::Checking;

    std::wstring host = L"api.github.com";
    std::wstring path = (channel == "beta")
        ? L"/repos/TheVixcho/ProyecThor/releases?per_page=1"
        : L"/repos/TheVixcho/ProyecThor/releases/latest";

    std::string body = FetchURL(host, path);
    if (body.empty()) {
        s_ErrorMsg = "No se pudo conectar al servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }

    std::string tag = ExtractJsonString(body, "tag_name");
    if (tag.empty()) {
        s_ErrorMsg = "Respuesta inesperada del servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }
    if (tag[0] == 'v') tag = tag.substr(1);

    s_DownloadUrl    = ExtractAssetDownloadUrl(body, "ProyecThor_Setup.exe");
    s_LatestVersion  = tag;

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", localtime(&now));
    ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

    if (IsNewer(currentVersion, tag)) {
        s_Status = UpdateStatus::Available;
        if (showPopupIfAvailable && !s_DownloadUrl.empty())
            s_ShowModal = true;
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
        s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel, true);
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
    auto& u   = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;
    auto  st  = s_Status.load();

    // Version instalada + Estado + Configuracion fusionadas en una sola
    // subcategoria (pedido explicito): antes eran 3 entradas de sidebar
    // para algo que se lee de punta a punta como una sola pagina.
    if (SectionTitle("Versión instalada", "Actualizaciones")) {
        // Badge de versión
        {
            ImDrawList* dl  = ImGui::GetWindowDrawList();
            ImVec2      p   = ImGui::GetCursorScreenPos();
            char        vtxt[32];
            snprintf(vtxt, sizeof(vtxt), "  v%s  ", u.currentVersion.c_str());
            ImVec2 tsz = ImGui::CalcTextSize(vtxt);

            dl->AddRectFilled(p, ImVec2(p.x + tsz.x, p.y + tsz.y + 8.0f),
                              IM_COL32(30, 56, 110, 180), 6.0f);
            dl->AddRect(p, ImVec2(p.x + tsz.x, p.y + tsz.y + 8.0f),
                        IM_COL32(61, 127, 245, 100), 6.0f, 0, 1.0f);
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.78f, 1.00f, 1.0f));
            ImGui::TextUnformatted(vtxt);
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + tsz.y + 8.0f + 8.0f));
        }

        // Fecha de última comprobación
        if (!u.lastChecked.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.40f, 0.54f, 1.0f));
            ImGui::Text("Ultima comprobación: %s", u.lastChecked.c_str());
            ImGui::PopStyleColor();
        }

        // El instalador (ver packaging/windows/ProyecThor.iss) ya borra
        // automaticamente cualquier version anterior detectada al instalar
        // una nueva -- este boton hace lo mismo pero on-demand, DESDE la app
        // ya corriendo, para instalaciones viejas sueltas que quedaron antes
        // de que existiera esa limpieza (tipicamente .msi de WiX de mas de
        // una version atras). Solo toca versiones ESTRICTAMENTE mas viejas
        // que la actual -- nunca la actual ni ninguna mas nueva.
#ifdef _WIN32
        {
            auto cst         = s_CleanupStatus.load();
            bool cleanupBusy = (cst == CleanupStatus::Running);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.72f, 0.86f, 1.0f));

            if (cleanupBusy) ImGui::BeginDisabled();
            if (ImGui::Button(cleanupBusy ? "Buscando..." : "Desinstalar versiones anteriores",
                              ImVec2(240.0f, 30.0f)))
                ImGui::OpenPopup("Desinstalar versiones anteriores?##cleanupOld");
            if (cleanupBusy) ImGui::EndDisabled();

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);

            char tip[192];
            snprintf(tip, sizeof(tip),
                "Busca en el registro de Windows cualquier instalacion de ProyecThor "
                "(Inno o un .msi viejo de WiX) mas vieja que la version actual (v%s) "
                "y la desinstala en silencio. Nunca toca la version actual ni ninguna mas nueva.",
                u.currentVersion.c_str());
            HelpTooltip(tip);

            ImVec2 mcenter = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(mcenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Desinstalar versiones anteriores?##cleanupOld", nullptr,
                                        ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped(
                    "Se buscaran instalaciones de ProyecThor mas viejas que v%s (incluye "
                    "instalaciones .msi antiguas) y se desinstalaran en silencio. Esto no "
                    "afecta la version que estas usando ahora.\n\n"
                    "Si alguna instalacion vieja es de tipo .msi, Windows puede pedir "
                    "permisos de administrador para quitarla -- es normal, aceptalo para "
                    "que se complete.", u.currentVersion.c_str());
                ImGui::Spacing();
                if (ImGui::Button("Desinstalar", ImVec2(120, 0))) {
                    if (s_CleanupThread.joinable()) s_CleanupThread.join();
                    s_CleanupStatus = CleanupStatus::Running;
                    s_CleanupThread = std::thread(DoCleanupOldInstalls, u.currentVersion);
                    s_CleanupThread.detach();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            if (cst == CleanupStatus::Done && !s_CleanupSummary.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, s_CleanupHadIssues
                    ? ImVec4(0.90f, 0.70f, 0.35f, 1.0f)   // amarillo/naranja: hubo cancelaciones o fallas
                    : ImVec4(0.50f, 0.80f, 0.55f, 1.0f)); // verde: todo bien
                ImGui::TextWrapped("%s", s_CleanupSummary.c_str());
                ImGui::PopStyleColor();
            }
        }
#else
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.40f, 0.54f, 1.0f));
        ImGui::TextUnformatted("Verificar versiones antiguas instaladas");
        ImGui::PopStyleColor();
        HelpTooltip("Revisa el gestor de paquetes de tu distro (apt/dnf/etc.) y "
                    "desinstala a mano cualquier version vieja de ProyecThor si te "
                    "quedo mas de una instalada.");
#endif

    ImGui::Spacing();
    ImGui::SeparatorText("Estado");
    ImGui::Spacing();

    // Dimensiones y colores según estado
    struct StatusStyle {
        ImVec4 borderCol;
        ImVec4 bgCol;
        ImVec4 textCol;
    };

    StatusStyle ss;
    const char* statusTitle  = "";
    const char* statusDetail = "";
    bool        showSpinner  = false;

    switch (st) {
        case UpdateStatus::Idle:
            ss = { ImVec4(0.24f,0.26f,0.38f,1), ImVec4(0.08f,0.09f,0.14f,1), ImVec4(0.50f,0.52f,0.66f,1) };
            statusTitle  = "Sin comprobar";
            statusDetail = "Haz clic en Comprobar ahora para buscar actualizaciones en GitHub.";
            break;
        case UpdateStatus::Checking:
            ss = { ImVec4(0.22f,0.40f,0.88f,0.60f), ImVec4(0.06f,0.10f,0.22f,1), ImVec4(0.55f,0.75f,1.00f,1) };
            statusTitle  = "Comprobando...";
            statusDetail = "Conectando con el servidor de actualizaciones.";
            showSpinner  = true;
            break;
        case UpdateStatus::UpToDate:
            ss = { ImVec4(0.20f,0.66f,0.40f,0.55f), ImVec4(0.05f,0.14f,0.09f,1), ImVec4(0.30f,0.86f,0.56f,1) };
            statusTitle  = "Estas al dia";
            statusDetail = "No hay actualizaciones disponibles en este momento.";
            break;
        case UpdateStatus::Available:
            ss = { ImVec4(0.88f,0.58f,0.10f,0.55f), ImVec4(0.14f,0.10f,0.03f,1), ImVec4(0.96f,0.75f,0.30f,1) };
            statusTitle  = "Nueva versión disponible";
            statusDetail = "";
            break;
        case UpdateStatus::Error:
            ss = { ImVec4(0.88f,0.28f,0.28f,0.55f), ImVec4(0.14f,0.04f,0.04f,1), ImVec4(0.96f,0.50f,0.50f,1) };
            statusTitle  = "Error de conexion";
            statusDetail = s_ErrorMsg.c_str();
            break;
    }

    // Dibujar la caja de estado
    ImVec2 boxPos = ImGui::GetCursorScreenPos();
    float  boxW   = ImGui::GetContentRegionAvail().x;
    float  boxH   = (st == UpdateStatus::Available) ? 80.0f : 64.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
                      ImGui::ColorConvertFloat4ToU32(ss.bgCol), 8.0f);
    dl->AddRect(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
                ImGui::ColorConvertFloat4ToU32(ss.borderCol), 8.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 14.0f));

    if (showSpinner) {
        SpinnerWidget(8.0f, 2.0f, ss.textCol);
        ImGui::SameLine(0, 10.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ss.textCol);
    ImGui::TextUnformatted(statusTitle);
    ImGui::PopStyleColor();

    if (statusDetail[0] != '\0') {
        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ss.textCol.x, ss.textCol.y, ss.textCol.z, 0.72f));
        ImGui::TextUnformatted(statusDetail);
        ImGui::PopStyleColor();
    }

    if (st == UpdateStatus::Available && !s_LatestVersion.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 34.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.75f, 0.30f, 0.80f));
        ImGui::Text("Versión %s disponible", s_LatestVersion.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorScreenPos(ImVec2(boxPos.x, boxPos.y + boxH + 14.0f));

    // ── Sección de descarga activa ─────────────────────────────────────────────
    // No es un SectionTitle propio a proposito: es parte de la MISMA pagina
    // "Estado" (aparece/desaparece segun s_IsDownloading), no una
    // subcategoria navegable aparte que vaya y venga del sidebar.
    if (s_IsDownloading) {
        float pct   = s_DownloadProgress.load();
        float dlMB  = s_DownloadedMB.load();
        float totMB = s_TotalMB.load();
        float speed = s_DownloadSpeedMBs.load();

        ImGui::SeparatorText("Descargando actualizacion");

        // Barra de progreso animada personalizada (helper del panel)
        AnimatedProgressBar(pct, ImVec2(ImGui::GetContentRegionAvail().x - 56.0f, 8.0f),
                             ImVec4(0.350f, 0.500f, 0.970f, 1.0f));

        ImGui::Spacing();

        // Metadatos de descarga
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.42f, 0.58f, 1.0f));
        if (totMB > 0.0f)
            ImGui::Text("%.1f MB / %.1f MB   %.1f MB/s", dlMB, totMB, speed);
        else
            ImGui::Text("Descargando...");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.38f, 0.38f, 1.0f));
        ImGui::TextUnformatted("No cierres ProyecThor durante la descarga.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Botones de acción ──────────────────────────────────────────────────────
    ImGui::Spacing();

    bool busy = (st == UpdateStatus::Checking) || s_IsDownloading;
    if (busy) ImGui::BeginDisabled();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 6.0f));

    // Botón comprobar
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.72f, 0.86f, 1.0f));

    if (ImGui::Button("Comprobar ahora", ImVec2(170.0f, 30.0f))) {
        if (s_TaskThread.joinable()) s_TaskThread.join();
        s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel, true);
        s_TaskThread.detach();
    }
    ImGui::PopStyleColor(4);

    // Botón descargar (solo si hay actualización disponible)
    if (st == UpdateStatus::Available && !s_DownloadUrl.empty() && !s_IsDownloading) {
        ImGui::SameLine(0, 10.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.24f, 0.18f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.32f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.56f, 0.42f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.96f, 0.75f, 0.30f, 1.0f));

        if (ImGui::Button("Descargar ahora", ImVec2(170.0f, 30.0f))) {
            if (s_TaskThread.joinable()) s_TaskThread.join();
            s_TaskThread = std::thread(DoDownloadAndInstall, s_DownloadUrl);
            s_TaskThread.detach();
        }
        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar(2);
    if (busy) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Configuracion");

    {
        ImGui::Checkbox("Comprobar al iniciar", &u.checkOnStartup);
        HelpTooltip("Comprueba actualizaciones automaticamente al abrir ProyecThor.");

        ImGui::Checkbox("Descarga automatica", &u.autoDownload);
        HelpTooltip("Descarga la nueva versión en segundo plano sin pedir confirmación.");

        ImGui::Spacing();

        // Canal
        const char* channels[] = { "stable", "beta" };
        const char* labels[]   = { "Estable", "Beta" };
        int         chIdx      = (u.updateChannel == "beta") ? 1 : 0;

        ImGui::TextUnformatted("Canal de actualizacion:");
        HelpTooltip("'Estable': versiones probadas y recomendadas.\n'Beta': acceso anticipado, puede contener errores.");

        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 5.0f));

        for (int i = 0; i < 2; i++) {
            bool isActive = (chIdx == i);

            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    i == 0 ? ImVec4(0.10f,0.28f,0.14f,1.0f) : ImVec4(0.28f,0.18f,0.04f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    i == 0 ? ImVec4(0.14f,0.36f,0.18f,1.0f) : ImVec4(0.36f,0.24f,0.06f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                    i == 0 ? ImVec4(0.18f,0.44f,0.22f,1.0f) : ImVec4(0.44f,0.30f,0.08f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    i == 0 ? ImVec4(0.30f,0.86f,0.48f,1.0f) : ImVec4(0.96f,0.65f,0.14f,1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f,0.09f,0.14f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.11f,0.12f,0.18f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f,0.15f,0.22f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.40f,0.42f,0.56f,1.0f));
            }

            if (i > 0) ImGui::SameLine(0, 6.0f);

            if (ImGui::Button(labels[i], ImVec2(100.0f, 28.0f)))
                u.updateChannel = channels[i];

            ImGui::PopStyleColor(4);
        }

        ImGui::PopStyleVar(2);
    }

    } // if (SectionTitle("Versión instalada", "Actualizaciones"))
}

} // namespace ProyecThor::UI::Settings