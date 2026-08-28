#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/FilePicker.h"
#include "frontend/ui/DesignSystem.h"
#include <imgui.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI::Settings {

enum class DataMoveStatus { Idle, Running, Done };
static std::atomic<DataMoveStatus> s_DataMoveStatus{ DataMoveStatus::Idle };
static std::string                 s_DataMoveSummary;
static bool                        s_DataMoveHadError = false;
static std::thread                 s_DataMoveThread;
static std::string                 s_PendingNewDataDir;
static bool                        s_CopyExistingOnMove = true;

static void DoMoveDataFolder(std::string oldRoot, std::string newParentDir, bool copyExisting) {
    s_DataMoveStatus = DataMoveStatus::Running;

    try {
        fs::path src(oldRoot);
        fs::path dst = fs::path(newParentDir) / "ProyecThor";

        std::error_code ec;
        fs::create_directories(dst, ec);
        if (ec) throw std::runtime_error("No se pudo crear la carpeta de destino: " + ec.message());

        if (copyExisting) {
            fs::copy(src, dst,
                fs::copy_options::recursive |
                fs::copy_options::overwrite_existing, ec);
            if (ec) throw std::runtime_error("No se pudieron copiar los archivos: " + ec.message());
        }

        std::string redirectFile = ProyecThor::GetDataDirRedirectFilePath();
        if (redirectFile.empty()) throw std::runtime_error("No se pudo ubicar el archivo de redirección.");

        // Asegurar que el directorio del redirect exista
        fs::create_directories(fs::path(redirectFile).parent_path(), ec);

        std::ofstream f(redirectFile, std::ios::trunc);
        if (!f.is_open()) throw std::runtime_error("No se pudo escribir el archivo de redirección.");
        f << dst.string();
        f.close();

        // Actualizar configuración
        auto& mgr = ProyecThor::Settings::SettingsManager::Get();
        mgr.GetSettings().storage.customDataRoot = dst.string();
        mgr.SaveSettings();

        s_DataMoveSummary = copyExisting
            ? "Listo. Se copiaron todos tus datos a la nueva ubicación. Reinicia ProyecThor para que los cambios tomen efecto completo."
            : "Listo. Se configuró la nueva carpeta de datos. Reinicia ProyecThor para que los cambios tomen efecto completo.";
        s_DataMoveHadError = false;
    } catch (const std::exception& e) {
        s_DataMoveSummary  = std::string("Error al cambiar la carpeta de datos: ") + e.what();
        s_DataMoveHadError = true;
    }

    s_DataMoveStatus = DataMoveStatus::Done;
}

static void OpenFolderInExplorer(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath;
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len > 0) {
        wpath.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);
        ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
    system(cmd.c_str());
#endif
}

void SettingsPanel::RenderCategoryData()
{
    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    auto& settings = mgr.GetSettings();

    if (s_DataMoveStatus.load() == DataMoveStatus::Done && s_DataMoveThread.joinable()) {
        s_DataMoveThread.join();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Subcategoría 1: Ubicación de Datos de la Aplicación
    // ─────────────────────────────────────────────────────────────────────────
    if (SectionTitle("Ubicación de Datos", "Almacenamiento")) {
        std::string currentRoot = ProyecThor::GetAppDataRoot();
        std::string defaultRoot = ProyecThor::GetDefaultAppDataRoot();
        bool isCustom = (currentRoot != defaultRoot && !ProyecThor::ReadDataDirRedirect().empty());

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary),
            "Carpeta principal de datos de ProyecThor");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Aquí se almacenan de forma permanente tus canciones, videos, audios, imágenes, biblias, overlays, temas y ajustes de la aplicación.");

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Caja informativa con la ruta actual
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

        if (ImGui::BeginChild("##curDataDirBox", ImVec2(0.0f, 74.0f), true)) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(isCustom ? DS::AccentColor : DS::SuccessColor),
                isCustom ? "● Ubicación Personalizada:" : "● Ubicación Predeterminada (AppData):");
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::TextUnformatted(currentRoot.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        bool moveBusy = (s_DataMoveStatus.load() == DataMoveStatus::Running);
        if (moveBusy) ImGui::BeginDisabled();

        if (DS::GlassButton("Cambiar carpeta de datos...", ImVec2(220.0f, 32.0f))) {
            std::string picked = ProyecThor::UI::PickFolder("Elegir nueva carpeta de datos");
            if (!picked.empty()) {
                s_PendingNewDataDir  = picked;
                s_CopyExistingOnMove = true;
                ImGui::OpenPopup("Cambiar ubicación de datos##moveDataModal");
            }
        }

        ImGui::SameLine(0.0f, 10.0f);
        if (DS::GlassButton("Abrir en explorador", ImVec2(180.0f, 32.0f))) {
            OpenFolderInExplorer(currentRoot);
        }

        if (isCustom) {
            ImGui::SameLine(0.0f, 10.0f);
            if (DS::GlassButton("Restablecer a AppData", ImVec2(190.0f, 32.0f), DS::DangerColorDim)) {
                std::string redirectFile = ProyecThor::GetDataDirRedirectFilePath();
                if (!redirectFile.empty()) {
                    std::error_code ec;
                    fs::remove(redirectFile, ec);
                }
                settings.storage.customDataRoot.clear();
                mgr.SaveSettings();
                s_DataMoveSummary = "Se restableció la ubicación a AppData. Reinicia la aplicación para aplicar.";
                s_DataMoveHadError = false;
                s_DataMoveStatus = DataMoveStatus::Done;
            }
        }

        if (moveBusy) ImGui::EndDisabled();

        // Modal de confirmación para mover la carpeta
        ImVec2 mcenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(mcenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Cambiar ubicación de datos##moveDataModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Nueva ubicación seleccionada:");
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::AccentColor), "%s\\ProyecThor", s_PendingNewDataDir.c_str());
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            ImGui::Checkbox("Copiar todos los datos actuales a la nueva carpeta", &s_CopyExistingOnMove);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));

            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
                "La carpeta original no se borrará para proteger tus datos.\n"
                "Deberás reiniciar ProyecThor para que el cambio tome efecto.");

            ImGui::Dummy(ImVec2(0.0f, 16.0f));

            if (DS::GlassButton("Aplicar cambio de carpeta", ImVec2(220.0f, 32.0f), DS::SuccessColor)) {
                if (s_DataMoveThread.joinable()) s_DataMoveThread.join();
                s_DataMoveThread = std::thread(DoMoveDataFolder,
                    ProyecThor::GetAppDataRoot(), s_PendingNewDataDir, s_CopyExistingOnMove);
                s_DataMoveThread.detach();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0.0f, 10.0f);
            if (DS::GlassButton("Cancelar", ImVec2(100.0f, 32.0f), DS::DangerColorDim)) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (s_DataMoveStatus.load() == DataMoveStatus::Done && !s_DataMoveSummary.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(s_DataMoveHadError ? DS::DangerColor : DS::SuccessColor),
                "%s", s_DataMoveSummary.c_str());

#ifdef _WIN32
            if (!s_DataMoveHadError) {
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                if (DS::GlassButton("Reiniciar ProyecThor ahora", ImVec2(220.0f, 30.0f), DS::SuccessColor)) {
                    wchar_t exePath[MAX_PATH] = {};
                    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
                        ShellExecuteW(nullptr, L"open", exePath, nullptr, nullptr, SW_SHOWNORMAL);
                        exit(0);
                    }
                }
            }
#endif
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Subcategoría 2: Carpetas de Importe Automático y Vinculadas (Watched Folders)
    // ─────────────────────────────────────────────────────────────────────────
    if (SectionTitle("Carpetas Vinculadas", "Almacenamiento")) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary),
            "Carpetas de Importe Automático y Escaneo Continuo");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Vincula carpetas de tu equipo (como 'Descargas', 'Videos' o un disco externo). "
            "ProyecThor detectará automáticamente los videos, audios e imágenes para usarlos directamente en tu Biblioteca.");

        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        if (DS::GlassButton("+ Agregar carpeta vinculada...", ImVec2(240.0f, 32.0f), DS::AccentColor)) {
            std::string picked = ProyecThor::UI::PickFolder("Seleccionar carpeta para vincular");
            if (!picked.empty()) {
                bool exists = false;
                for (const auto& wf : settings.storage.watchedFolders) {
                    if (wf.path == picked) { exists = true; break; }
                }
                if (!exists) {
                    ProyecThor::Settings::WatchedFolder wf;
                    wf.path          = picked;
                    wf.copyToDataDir = false; // por defecto reproducir directo sin duplicar
                    wf.enabled       = true;
                    settings.storage.watchedFolders.push_back(wf);
                    mgr.SaveSettings();
                }
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        if (settings.storage.watchedFolders.empty()) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
                "No hay carpetas vinculadas actualmente. Toca '+ Agregar carpeta vinculada...' para vincular una.");
        } else {
            int deleteIndex = -1;

            for (size_t i = 0; i < settings.storage.watchedFolders.size(); ++i) {
                auto& wf = settings.storage.watchedFolders[i];
                ImGui::PushID(static_cast<int>(i));

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

                float cardH = 96.0f;
                if (ImGui::BeginChild("##wfCard", ImVec2(0.0f, cardH), true)) {
                    ImGui::Checkbox("##enableWf", &wf.enabled);
                    ImGui::SameLine(0.0f, 8.0f);

                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(wf.enabled ? DS::TextPrimary : DS::TextHint),
                        "%s", wf.path.c_str());

                    ImGui::Dummy(ImVec2(0.0f, 6.0f));

                    // Selector de Modo de Importación
                    ImGui::SetCursorPosX(34.0f);
                    if (ImGui::RadioButton("Cargar sin copiar (reproducir original)", !wf.copyToDataDir)) {
                        wf.copyToDataDir = false;
                        mgr.SaveSettings();
                    }
                    HelpTooltip("Los archivos se leen directamente desde esta carpeta sin duplicar espacio en disco.");

                    ImGui::SameLine(0.0f, 20.0f);
                    if (ImGui::RadioButton("Copiar a la carpeta de datos", wf.copyToDataDir)) {
                        wf.copyToDataDir = true;
                        mgr.SaveSettings();
                    }
                    HelpTooltip("Los archivos nuevos detectados se copiarán automáticamente a la carpeta de la aplicación.");

                    // Botones de acción en la derecha
                    float availW = ImGui::GetContentRegionAvail().x;
                    ImGui::SameLine(availW - 180.0f);

                    if (DS::GlassButton("Abrir", ImVec2(70.0f, 24.0f))) {
                        OpenFolderInExplorer(wf.path);
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (DS::GlassButton("Eliminar", ImVec2(90.0f, 24.0f), DS::DangerColorDim)) {
                        deleteIndex = static_cast<int>(i);
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);

                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                ImGui::PopID();
            }

            if (deleteIndex >= 0 && deleteIndex < (int)settings.storage.watchedFolders.size()) {
                settings.storage.watchedFolders.erase(settings.storage.watchedFolders.begin() + deleteIndex);
                mgr.SaveSettings();
            }
        }
    }
}

} // namespace ProyecThor::UI::Settings

