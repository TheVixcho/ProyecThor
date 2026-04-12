#include "SongView.h"
#include "../../../PresentationCore.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

    void SongView::Render() {
        auto& core = Core::PresentationCore::Get();
        
        // --- LA SOLUCIÓN VITAL ---
        // Usamos PeekSelection para que ImGui pueda redibujar la interfaz 
        // 60 veces por segundo sin que los datos se borren de la memoria.
        auto selection = core.PeekSelection();

        // Validar que efectivamente hay algo seleccionado y es una canción
        if (selection.title.empty() || selection.type != Core::ItemType::Song) {
            return;
        }

        // --- 1. HEADER PROFESIONAL ---
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.4f, 1.0f)); // Verde "Live Text"
        ImGui::TextUnformatted("LYRICS DECK");
        ImGui::PopStyleColor();
        
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(selection.title.c_str()).x - 5);
        ImGui::TextDisabled("%s", selection.title.c_str());
        
        ImGui::Separator();
        ImGui::Spacing();

        // --- 2. CONTROLES MAESTROS ---
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        
        if (ImGui::Button("⬛ LIMPIAR PANTALLA (CLEAR TEXT)", ImVec2(-1, 40))) {
            core.ClearLayer2();
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- 3. GRID DE ESTROFAS (SLIDES) ---
        if (selection.contentData.empty()) {
            ImGui::TextDisabled("No hay letras disponibles para esta canción.");
            return;
        }

        // Calculamos cuántas columnas caben según el ancho de la ventana
        float availWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(availWidth / 250.0f));

        if (ImGui::BeginTable("StanzasGrid", columns, ImGuiTableFlags_SizingStretchSame)) {
            for (size_t i = 0; i < selection.contentData.size(); ++i) {
                ImGui::TableNextColumn();

                const std::string& stanza = selection.contentData[i];
                
                ImGui::PushID(static_cast<int>(i));
                
                // Estilo de la "Tarjeta"
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                
                // Altura fija para mantener orden, con scroll interno si la estrofa es muy larga
                std::string childName = "SlideCard_" + std::to_string(i);
                if (ImGui::BeginChild(childName.c_str(), ImVec2(0, 110), true, ImGuiWindowFlags_NoScrollbar)) {
                    
                    // Selectable invisible que cubre toda la tarjeta para detectar el click
                    if (ImGui::Selectable("##click", false, ImGuiSelectableFlags_AllowOverlap, ImGui::GetContentRegionAvail())) {
                        core.SetLayer2_Text(stanza);
                        core.SetProjecting(true);
                    }
                    
                    // Dibujamos el texto sobre el Selectable
                    ImGui::SetCursorPos(ImVec2(8, 8)); 
                    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8);
                    ImGui::TextUnformatted(stanza.c_str());
                    ImGui::PopTextWrapPos();

                    // Pequeño indicador de número de estrofa en la esquina
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 20, ImGui::GetWindowHeight() - 20));
                    ImGui::TextDisabled("%d", (int)i + 1);
                }
                ImGui::EndChild();
                
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

} // namespace ProyecThor::UI