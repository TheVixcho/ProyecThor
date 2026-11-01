#include "ProCloudSongsPanel.h"
#include "core/ProAIManager.h"
#include "core/AuthLicenseManager.h"
#include "core/AppPaths.h"
#include "ui/framework/DesignSystem.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ProyecThor::UI {

void ProCloudSongsPanel::Render(bool* pOpen, GlassRenderer& glass) {
    if (!pOpen || !*pOpen) return;

    auto& proMgr  = ProyecThor::Core::ProAIManager::Get();
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 920.0f, 620.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 650.0f, 450.0f }, { 10000.0f, 10000.0f });

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Gestor de Cancionero en Nube (Firebase)", glass, pOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(18.0f, 16.0f));

    if (open) {
        const auto& songs = proMgr.GetCloudSongs();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float availW = ImGui::GetContentRegionAvail().x;

        // ── Cabecera: Almacenamiento Pro y Botones de Acción ─────────────────
        ImVec2 barP = ImGui::GetCursorScreenPos();
        float barH = 46.0f;
        dl->AddRectFilled(barP, ImVec2(barP.x + availW, barP.y + barH), IM_COL32(20, 28, 42, 220), DS::RadiusMedium);
        dl->AddRect(barP, ImVec2(barP.x + availW, barP.y + barH), IM_COL32(56, 189, 248, 120), DS::RadiusMedium);

        ImGui::SetCursorScreenPos(ImVec2(barP.x + 14.0f, barP.y + 12.0f));

        // Contador de canciones (hasta 5,000 en la nube)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 1.0f, 1.0f));
        ImGui::Text("Almacenamiento Cloud: %zu / 5,000 canciones", songs.size());
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 24.0f);
        if (DS::GlassButton("Subir canción local", ImVec2(165.0f, 26.0f))) {
            m_ShowLocalPickerModal = true;
        }

        ImGui::SameLine(0, 10.0f);
        if (DS::GlassButton("Crear en la nube", ImVec2(150.0f, 26.0f))) {
            m_ShowNewSongModal = true;
            m_NewTitleBuf[0] = '\0';
            m_NewContentBuf[0] = '\0';
        }

        ImGui::SameLine(0, 10.0f);
        if (DS::GlassButton("Actualizar", ImVec2(100.0f, 26.0f))) {
            proMgr.RefreshCloudSongs([this](bool ok, const std::string& msg) {
                m_StatusFeedback = msg;
                m_StatusIsSuccess = ok;
            });
        }

        ImGui::SetCursorScreenPos(ImVec2(barP.x, barP.y + barH + 12.0f));

        // Mensaje de estado temporal si lo hay
        if (!m_StatusFeedback.empty()) {
            ImVec4 col = m_StatusIsSuccess ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(m_StatusFeedback.c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // Buscador
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputTextWithHint("##cloud_search", "Buscar en la nube...", m_SearchFilter, sizeof(m_SearchFilter));
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // ── Layout de 2 columnas: Lista a la izquierda, Vista previa a la derecha ──
        float leftW = 320.0f;
        float rightW = ImGui::GetContentRegionAvail().x - leftW - 14.0f;
        float bodyH  = ImGui::GetContentRegionAvail().y - 10.0f;

        // Panel Izquierdo: Lista de Canciones
        ImGui::BeginChild("##cloud_song_list", ImVec2(leftW, bodyH), true);

        std::string query = m_SearchFilter;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        if (songs.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextWrapped("No hay canciones guardadas en la nube aún. Sube o crea tu primera canción con los botones superiores.");
            ImGui::PopStyleColor();
        } else {
            for (size_t i = 0; i < songs.size(); ++i) {
                const auto& s = songs[i];
                std::string sTitleLower = s.title;
                std::transform(sTitleLower.begin(), sTitleLower.end(), sTitleLower.begin(), ::tolower);

                if (!query.empty() && sTitleLower.find(query) == std::string::npos) {
                    continue;
                }

                bool selected = (m_SelectedSongIndex == (int)i);
                std::string itemLabel = s.title + "##item" + std::to_string(i);

                if (ImGui::Selectable(itemLabel.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0f, 32.0f))) {
                    m_SelectedSongIndex = (int)i;
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Última actualización: %s", s.updatedAt.c_str());
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 14.0f);

        // Panel Derecho: Vista previa y Acciones de la Canción Seleccionada
        ImGui::BeginChild("##cloud_song_view", ImVec2(rightW, bodyH), true);

        if (m_SelectedSongIndex >= 0 && m_SelectedSongIndex < (int)songs.size()) {
            const auto& curSong = songs[m_SelectedSongIndex];

            // Título
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextUnformatted(curSong.title.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            // Barra de botones de acción
            if (DS::GlassButton("Copiar TXT", ImVec2(120.0f, 30.0f))) {
                ImGui::SetClipboardText(curSong.content.c_str());
                m_StatusFeedback = "Letra copiada al portapapeles.";
                m_StatusIsSuccess = true;
            }

            ImGui::SameLine(0, 10.0f);
            if (DS::GlassButton("Descargar a Biblioteca", ImVec2(180.0f, 30.0f))) {
                ProyecThor::Library::CreateNewSongFromText(curSong.title, curSong.content);
                m_StatusFeedback = "Canción importada a la biblioteca local de ProyecThor.";
                m_StatusIsSuccess = true;
            }

            ImGui::SameLine(0, 10.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.15f, 0.15f, 0.8f));
            if (ImGui::Button("Eliminar de la nube", ImVec2(160.0f, 30.0f))) {
                std::string delId = curSong.id;
                proMgr.DeleteCloudSong(delId, [this](bool ok, const std::string& msg) {
                    m_StatusFeedback = msg;
                    m_StatusIsSuccess = ok;
                    m_SelectedSongIndex = -1;
                });
            }
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // Visualización de la letra (formato TXT estructurado)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.88f, 0.95f, 1.0f));
            ImGui::TextWrapped("%s", curSong.content.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextWrapped("Selecciona una canción de la lista a la izquierda para ver su contenido, copiarla en formato TXT o descargarla a tu equipo.");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();

        // ── Modal: Crear Canción Nueva en la Nube ────────────────────────────
        if (m_ShowNewSongModal) {
            ImGui::OpenPopup("Nueva Canción en la Nube##modalNewSong");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 480.0f));

        if (ImGui::BeginPopupModal("Nueva Canción en la Nube##modalNewSong", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("Título de la canción:");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##new_title_inp", m_NewTitleBuf, sizeof(m_NewTitleBuf));

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextUnformatted("Letra en formato TXT (separa las estrofas con una línea en blanco):");
            ImGui::InputTextMultiline("##new_content_inp", m_NewContentBuf, sizeof(m_NewContentBuf),
                                      ImVec2(-1.0f, 300.0f));

            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            if (ImGui::Button("Guardar en la Nube", ImVec2(160.0f, 32.0f))) {
                std::string t = m_NewTitleBuf;
                std::string c = m_NewContentBuf;
                if (!t.empty() && !c.empty()) {
                    proMgr.UploadCloudSong(t, c, [this](bool ok, const std::string& msg) {
                        m_StatusFeedback = msg;
                        m_StatusIsSuccess = ok;
                    });
                    m_ShowNewSongModal = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(100.0f, 32.0f))) {
                m_ShowNewSongModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // ── Modal: Selector de Canciones Locales para Subir ──────────────────
        if (m_ShowLocalPickerModal) {
            ImGui::OpenPopup("Subir Canción Local a la Nube##modalPicker");
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500.0f, 440.0f));

        if (ImGui::BeginPopupModal("Subir Canción Local a la Nube##modalPicker", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("Selecciona una canción de tu biblioteca local para respaldar en la nube:");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            ImGui::BeginChild("##local_songs_scroll", ImVec2(-1.0f, 320.0f), true);

            std::string songsDir = ProyecThor::SongsPath();
            if (std::filesystem::exists(songsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(songsDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        std::string stem = entry.path().stem().string();
                        if (ImGui::Selectable(stem.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 26.0f))) {
                            // Leer contenido del archivo
                            std::ifstream f(entry.path());
                            if (f.is_open()) {
                                std::stringstream ss;
                                ss << f.rdbuf();
                                proMgr.UploadCloudSong(stem, ss.str(), [this](bool ok, const std::string& msg) {
                                    m_StatusFeedback = msg;
                                    m_StatusIsSuccess = ok;
                                });
                            }
                            m_ShowLocalPickerModal = false;
                            ImGui::CloseCurrentPopup();
                            break;
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No se encontró la carpeta de canciones locales.");
            }

            ImGui::EndChild();

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (ImGui::Button("Cerrar", ImVec2(100.0f, 30.0f))) {
                m_ShowLocalPickerModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI
