#include "MediaView.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/AppPaths.h"
#include "UIStrings.h"
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace ProyecThor::UI {

    MediaView::~MediaView() {}

    std::string MediaView::FormatTime(int64_t ms) {
        if (ms < 0) ms = 0;
        int totalSeconds = static_cast<int>(ms / 1000);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
        return oss.str();
    }

    void MediaView::Render(Core::VLCBasePlayer* previewPlayer) {
        const auto& str = ProyecThor::UI::GetUIStrings();

        auto selection = Core::PresentationCore::Get().PeekSelection();

        if (selection.title != m_LastSelectedFile) {
            m_LastSelectedFile = selection.title;

            // FIX (crash en Windows/Wine): la cola del Monitor pasa por
            // SetSelection(..., fromQueue=true) al arrancar/avanzar cada
            // item, solo para que el titulo se muestre — no es una eleccion
            // manual del operador en la Biblioteca. Antes esto no se
            // distinguia, asi que cada avance de la cola disparaba TAMBIEN
            // una carga en el reproductor de Preview del mismo archivo que
            // la cola ya esta reproduciendo/precargando (a la vez que
            // MonitorView::Render() hacia lo mismo sobre el mismo preview
            // player) — dos/tres instancias de VLC abriendo el mismo
            // archivo al mismo tiempo, lo que crasheaba en Windows.
            bool fromQueue = Core::PresentationCore::Get().IsSelectionFromQueue();

            if (selection.type == Core::ItemType::Video && fromQueue) {
                // No tocar el preview para nada: ni cargarlo (evita el
                // choque de instancias de VLC descripto arriba) ni
                // detenerlo (si el operador tenia otra cosa en preview,
                // que un avance interno de la cola no se lo pise).
            } else if (selection.type == Core::ItemType::Video) {
                if (previewPlayer) {
                    std::string previewPath = selection.title;

                    // Si NO es un enlace de internet ni ruta absoluta, armamos la ruta local
                    if (previewPath.rfind("http", 0) != 0 && !std::filesystem::path(previewPath).is_absolute()) {
                        previewPath = VideosPath() + previewPath;
                    }

                    // Carga en un hilo aparte (ver
                    // PresentationCore::RequestPreviewLoad): el video en
                    // vivo al publico nunca debe esperar a que el Preview
                    // termine de abrir un archivo.
                    Core::PresentationCore::Get().RequestPreviewLoad(previewPath, /*loop=*/true, /*startMuted=*/true);
                    m_IsPlayingPreview = true;
                }
            } else if (selection.type == Core::ItemType::Image) {
                if (previewPlayer) {
                    Core::PresentationCore::Get().RequestPreviewStop();
                    m_IsPlayingPreview = false;
                }
                std::string imgPath = selection.title;
                if (!std::filesystem::path(imgPath).is_absolute()) {
                    imgPath = ImagesPath() + imgPath;
                }
                m_ImageView.LoadImageFromFile(imgPath);
                m_ImageView.ResetAdjustments();
            } else {
                if (previewPlayer) {
                    Core::PresentationCore::Get().RequestPreviewStop();
                    m_IsPlayingPreview = false;
                }
                m_ImageView.Clear();
            }
        }

        if (selection.type == Core::ItemType::None) {
            ImGui::TextDisabled("%s", str.mediaNoSelection);
        } else if (selection.type == Core::ItemType::Song) {
            ImGui::TextDisabled("%s", selection.title.c_str());
            ImGui::BeginChild("VerseList", ImVec2(0, 0), true);
            for (size_t i = 0; i < selection.contentData.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Button(selection.contentData[i].c_str(), ImVec2(-1, 60))) {
                    Core::PresentationCore::Get().SetLayer2_Text(selection.contentData[i]);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else if (selection.type == Core::ItemType::Image) {
            // (Mantenemos la lógica de la imagen como la tienes)
            std::string dispTitle = std::filesystem::path(selection.title).filename().string();
            ImGui::TextDisabled("%s", dispTitle.c_str());
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
            if (ImGui::Button(str.mediaProjectImage, ImVec2(-1, 40))) {
                std::string imgPath = selection.title;
                if (!std::filesystem::path(imgPath).is_absolute()) {
                    imgPath = ImagesPath() + imgPath;
                }
                Core::PresentationCore::Get().SetBackgroundMedia(imgPath, true);
            }
            ImGui::PopStyleColor(2);

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Editor de imagen")) {
                m_ImageView.RenderAdjustmentsPanel();
            }

            ImGui::Spacing();

            ImVec2 availSize = ImGui::GetContentRegionAvail();
            m_ImageView.Render(availSize.x, availSize.y - 10.0f);

        } else if (selection.type == Core::ItemType::Video) {
            // --- AQUÍ ESTABA EL CAMBIO ---
            // Simplemente dejamos este bloque vacío o añadimos un espaciado mínimo 
            // si quieres que no se pegue al borde, pero ya no habrá textos.
        }
    }

} // namespace ProyecThor::UI