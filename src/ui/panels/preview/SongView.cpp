#include "SongView.h"
#include "../../../PresentationCore.h"
#include <imgui.h>

namespace ProyecThor::UI {

    void SongView::Render() {
        auto& core = Core::PresentationCore::Get();
        auto selection = core.GetSelection();

        // Validar que efectivamente estamos en una canción
        if (selection.type != Core::ItemType::Song) return;

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

        // Calculamos cuántas columnas caben (cada tarjeta de unos 250px mínimo)
        float availWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(availWidth / 250.0f));

        if (ImGui::BeginTable("StanzasGrid", columns, ImGuiTableFlags_SizingStretchSame)) {
            for (size_t i = 0; i < selection.contentData.size(); ++i) {
                ImGui::TableNextColumn();

                const std::string& stanza = selection.contentData[i];
                
                ImGui::PushID(static_cast<int>(i));
                
                // Creamos una "Tarjeta" visual para cada estrofa
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                
                // Altura fija para que la grilla se vea ordenada
                ImGui::BeginChild("SlideCard", ImVec2(0, 100), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                
                // Hacemos que todo el fondo de la tarjeta sea un botón invisible clickeable
                if (ImGui::Selectable("##click", false, ImGuiSelectableFlags_AllowOverlap, ImGui::GetContentRegionAvail())) {
                    core.SetLayer2_Text(stanza);
                }
                
                // Dibujamos el texto encima del botón
                ImGui::SetCursorPos(ImVec2(8, 8)); // Margen interior
                ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
                ImGui::TextUnformatted(stanza.c_str());
                ImGui::PopTextWrapPos();

                ImGui::EndChild();
                
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

} // namespace ProyecThor::UI