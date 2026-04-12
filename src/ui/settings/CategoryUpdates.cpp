#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>

// ── HTTP: usa WinHTTP en Windows (ya disponible sin deps extra) ───────────────
#if defined(_WIN32)
  #include <windows.h>
  #include <winhttp.h>
  #pragma comment(lib, "winhttp.lib")
#endif

namespace ProyecThor::UI::Settings {

    // ─────────────────────────────────────────────────────────────────────────
    // Estado de la comprobación de actualizaciones (estático al módulo)
    // ─────────────────────────────────────────────────────────────────────────
    enum class UpdateStatus { Idle, Checking, UpToDate, Available, Error };

    static std::atomic<UpdateStatus> s_Status{ UpdateStatus::Idle };
    static std::string               s_LatestVersion = "";
    static std::string               s_ErrorMsg      = "";
    static std::thread               s_CheckThread;

    // Versión semántica: devuelve true si latest > current
    static bool IsNewer(const std::string& current, const std::string& latest) {
        auto parse = [](const std::string& v) {
            int ma=0, mi=0, pa=0;
            sscanf(v.c_str(), "%d.%d.%d", &ma, &mi, &pa);
            return ma * 10000 + mi * 100 + pa;
        };
        return parse(latest) > parse(current);
    }

    // Descarga una URL simple y devuelve el body como string (Windows only)
    static std::string FetchURL(const std::wstring& host, const std::wstring& path) {
#if defined(_WIN32)
        std::string result;
        HINTERNET hSession = WinHttpOpen(L"ProyecThor/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
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

    // Extrae el valor de un campo JSON simple: "key": "value"
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

    static void DoCheckUpdate(const std::string& currentVersion, const std::string& channel) {
        s_Status = UpdateStatus::Checking;
        s_LatestVersion = "";
        s_ErrorMsg = "";

        // Apunta a tu repositorio GitHub releases API
        // Reemplaza "TheVixcho/ProyecThor" con tu repo real
        std::wstring host = L"api.github.com";
        std::wstring path = channel == "beta"
            ? L"/repos/TheVixcho/ProyecThor/releases?per_page=1"
            : L"/repos/TheVixcho/ProyecThor/releases/latest";

        std::string body = FetchURL(host, path);
        if (body.empty()) {
            s_ErrorMsg = "No se pudo conectar al servidor.";
            s_Status = UpdateStatus::Error;
            return;
        }

        // El campo "tag_name" contiene la versión, ej: "v0.2.0"
        std::string tag = ExtractJsonString(body, "tag_name");
        if (tag.empty()) {
            s_ErrorMsg = "Respuesta inesperada del servidor.";
            s_Status = UpdateStatus::Error;
            return;
        }
        // Quitar 'v' prefijo si existe
        if (!tag.empty() && tag[0] == 'v') tag = tag.substr(1);

        s_LatestVersion = tag;

        // Guardar fecha de última comprobación
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char timeBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", localtime(&now));
        ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

        s_Status = IsNewer(currentVersion, tag)
            ? UpdateStatus::Available
            : UpdateStatus::UpToDate;
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::RenderCategoryUpdates() {
        auto& u = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;

        ImGui::TextDisabled("Mantén ProyecThor actualizado con las últimas mejoras.");
        ImGui::Spacing();

        SectionTitle("Estado Actual");
        ImGui::Text("Versión instalada:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
        ImGui::Text("%s", u.currentVersion.c_str());
        ImGui::PopStyleColor();

        if (!u.lastChecked.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.48f, 1.0f));
            ImGui::Text("Última comprobación: %s", u.lastChecked.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // ── Resultado de la comprobación ──
        switch (s_Status.load()) {
            case UpdateStatus::Idle:
                ImGui::TextDisabled("No se ha comprobado aún.");
                break;
            case UpdateStatus::Checking:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.80f, 1.0f, 1.0f));
                ImGui::Text("⏳ Comprobando...");
                ImGui::PopStyleColor();
                break;
            case UpdateStatus::UpToDate:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.88f, 0.50f, 1.0f));
                ImGui::Text("✓  Estás usando la versión más reciente.");
                ImGui::PopStyleColor();
                break;
            case UpdateStatus::Available:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.30f, 1.0f));
                ImGui::Text("⬆  Nueva versión disponible: %s", s_LatestVersion.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.24f,0.20f,0.085f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f,0.415f,0.18f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.97f,0.87f,0.60f,1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                if (ImGui::Button("Abrir página de descargas", ImVec2(220, 30))) {
#if defined(_WIN32)
                    ShellExecuteA(NULL, "open",
                        "https://github.com/TheVixcho/ProyecThor/releases/latest",
                        NULL, NULL, SW_SHOWNORMAL);
#endif
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                break;
            case UpdateStatus::Error:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
                ImGui::Text("✗  Error: %s", s_ErrorMsg.c_str());
                ImGui::PopStyleColor();
                break;
        }

        ImGui::Spacing();

        // ── Botón Comprobar ahora ──
        bool checking = (s_Status.load() == UpdateStatus::Checking);
        if (checking) ImGui::BeginDisabled();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("Comprobar ahora", ImVec2(160, 28))) {
            if (s_CheckThread.joinable()) s_CheckThread.join();
            s_CheckThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel);
            s_CheckThread.detach();
        }
        if (checking) ImGui::EndDisabled();
        ImGui::PopStyleVar();

        SectionTitle("Configuración de Actualizaciones");
        ImGui::Checkbox("Comprobar al iniciar", &u.checkOnStartup);
        HelpTooltip("Comprueba automáticamente si hay actualizaciones al abrir ProyecThor.");
        ImGui::Checkbox("Descargar automáticamente", &u.autoDownload);
        HelpTooltip("Descarga la nueva versión en segundo plano (requiere permiso de escritura).");

        ImGui::Spacing();
        const char* channels[] = { "stable", "beta" };
        int chIdx = (u.updateChannel == "beta") ? 1 : 0;
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("Canal##chan", &chIdx, channels, 2))
            u.updateChannel = channels[chIdx];
        HelpTooltip("'stable' = versiones probadas.\n'beta' = acceso anticipado con posibles errores.");
    }

} // namespace ProyecThor::UI::Settings
