#include "ControlPanel.h"
#include "UIManager.h" 
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "../../PresentationCore.h"

namespace ProyecThor::UI {

    ControlPanel::ControlPanel(UIManager* uiManager) : m_UIManager(uiManager) {}

    void ControlPanel::Render() {
        ImGui::Begin(GetName().c_str());

        // ==========================================
        // 1. CONTROLES RÁPIDOS DE LIMPIEZA (CLEAR)
        // ==========================================
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Controles Rápidos (Clear)");
        ImGui::Spacing();

        // Calculamos el ancho para poner dos botones en la misma línea
        ImVec2 halfButtonSize = ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2, 35);

        // BOTÓN: QUITAR LETRA
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f)); // Azul
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        if (ImGui::Button("QUITAR LETRA", halfButtonSize)) {
            Core::PresentationCore::Get().ClearLayer2();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        // BOTÓN: DETENER VIDEO/FONDO
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f)); // Rojo
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("DETENER VIDEO", halfButtonSize)) {
            Core::PresentationCore::Get().StopBackgroundMedia();
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ==========================================
        // 2. CONFIGURACIÓN DE PANTALLA PRINCIPAL
        // ==========================================
        ImGui::Text("Configuración de Salida");
        ImGui::Spacing();

        int monitorCount;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        if (monitorCount < 2) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped(u8"No se detectó una pantalla secundaria.");
            ImGui::PopStyleColor();
            ImGui::BeginDisabled(); 
        } else {
            ImGui::Text("Pantallas detectadas: %d", monitorCount);
        }

        // Botón Principal de Proyección
        ImVec2 mainButtonSize = ImVec2(-1, 40); 
        
        if (!m_isProjecting) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); // Verde
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button(u8"EMPEZAR PROYECCIÓN", mainButtonSize)) {
                m_isProjecting = true;
                ToggleSecondaryDisplay(true);
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f)); // Rojo intenso
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
            
            if (ImGui::Button(u8"APAGAR PROYECTOR", mainButtonSize)) {
                m_isProjecting = false;
                ToggleSecondaryDisplay(false);
                
                // Opcional: También limpiaremos todo si apagan el proyector
                Core::PresentationCore::Get().ClearLayer2();
                Core::PresentationCore::Get().StopBackgroundMedia();
            }
            ImGui::PopStyleColor(3); 
        }

        if (monitorCount < 2) {
            ImGui::EndDisabled();
        }

        ImGui::End();
    }

    void ControlPanel::ToggleSecondaryDisplay(bool active) {
        Core::PresentationCore::Get().SetProjecting(active);
        if (active) {
            std::cout << "Proyección iniciada." << std::endl;
        } else {
            std::cout << "Proyección detenida." << std::endl;
        }
    }

} // namespace ProyecThor::UI