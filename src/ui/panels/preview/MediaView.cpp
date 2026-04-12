#include "MediaView.h"
#include "../../../PresentationCore.h"
#include "../../../VLCBasePlayer.h"
#include <imgui.h>
#include <iomanip>
#include <sstream>

namespace ProyecThor::UI {

    std::string MediaView::FormatTime(int64_t ms) {
        if (ms < 0) ms = 0;
        int totalSeconds = ms / 1000;
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << minutes << ":" 
            << std::setfill('0') << std::setw(2) << seconds;
        return oss.str();
    }

    void MediaView::Render(Core::VLCBasePlayer* previewPlayer) {
        // En lugar de GetSelection(), hacemos Peek porque PreviewPanel ya lo consumió
        auto selection = Core::PresentationCore::Get().PeekSelection();

        // 1. Lógica de Autoplay separada correctamente
        if (selection.title != m_LastSelectedFile) {
            m_LastSelectedFile = selection.title;
            // ... (el resto de tu código se mantiene EXACTAMENTE igual) ...
            
            // SOLO reproducir en VLC si es estrictamente un VIDEO
            if (selection.type == Core::ItemType::Video) {
                if (previewPlayer) {
                    previewPlayer->Play("assets/videos/" + selection.title);
                    m_IsPlayingPreview = true;
                }
            } 
            // Si es imagen, canción o nada, detenemos el reproductor de video
            else {
                if (previewPlayer) {
                    previewPlayer->Stop();
                    m_IsPlayingPreview = false;
                }
            }
        }

        // 2. Renderizado de la Interfaz según el tipo
        if (selection.type == Core::ItemType::None) {
            ImGui::TextDisabled("Seleccione un elemento de la biblioteca.");
        } 
        else if (selection.type == Core::ItemType::Song) {
            ImGui::TextDisabled("DOCUMENTO: %s", selection.title.c_str());
            ImGui::BeginChild("VerseList", ImVec2(0, 0), true);
            for (size_t i = 0; i < selection.contentData.size(); i++) {
                ImGui::PushID((int)i);
                if (ImGui::Button(selection.contentData[i].c_str(), ImVec2(-1, 60))) {
                    Core::PresentationCore::Get().SetLayer2_Text(selection.contentData[i]);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        } 
        // --- NUEVA SECCIÓN EXCLUSIVA PARA IMÁGENES ---
        else if (selection.type == Core::ItemType::Image) {
            ImGui::TextDisabled("IMAGEN: %s", selection.title.c_str());
            ImGui::BeginChild("ImageControls", ImVec2(0, 0), true);
            
            ImGui::TextWrapped("Vista previa de imagen (No requiere reproductor)");
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("PROYECTAR IMAGEN (LIVE)", ImVec2(-1, 50))) {
                Core::PresentationCore::Get().SetBackgroundMedia("assets/images/" + selection.title, false);
                Core::PresentationCore::Get().SetProjecting(true); 
            }
            ImGui::PopStyleColor(2);
            ImGui::EndChild();
        }
        // --- SECCIÓN EXCLUSIVA PARA VIDEOS ---
        else if (selection.type == Core::ItemType::Video) {
            ImGui::TextDisabled("VIDEO: %s", selection.title.c_str());
            ImGui::BeginChild("MediaControls", ImVec2(0, 0), true);
            
            if (previewPlayer) {
                float currentPos = previewPlayer->GetPosition();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##timeline", &currentPos, 0.0f, 1.0f, "")) {
                    previewPlayer->SetPosition(currentPos);
                }
                int64_t currentTimeMs = previewPlayer->GetTime();
                int64_t totalTimeMs = previewPlayer->GetLength();
                ImGui::TextDisabled("%s", (FormatTime(currentTimeMs) + " / " + FormatTime(totalTimeMs)).c_str());
                
                ImGui::SameLine(ImGui::GetContentRegionAvail().x / 2 - 40);
                if (m_IsPlayingPreview) {
                    if (ImGui::Button("Pausa", ImVec2(80, 30))) { 
                        previewPlayer->SetPause(true); 
                        m_IsPlayingPreview = false; 
                    }
                } else {
                    if (ImGui::Button("Play", ImVec2(80, 30))) { 
                        previewPlayer->SetPause(false); 
                        m_IsPlayingPreview = true; 
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Detener", ImVec2(80, 30))) { 
                    previewPlayer->SetPosition(0.0f); 
                    previewPlayer->SetPause(true); 
                    m_IsPlayingPreview = false; 
                }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("PROYECTAR VIDEO (LIVE)", ImVec2(-1, 50))) {
                Core::PresentationCore::Get().SetBackgroundMedia("assets/videos/" + selection.title, true);
                Core::PresentationCore::Get().SetProjecting(true); 
            }
            ImGui::PopStyleColor(2);
            ImGui::EndChild();
        }
    }

} // namespace ProyecThor::UI