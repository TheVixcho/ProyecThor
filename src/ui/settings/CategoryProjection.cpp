#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>  // std::clamp
#include <vector>
#include <string>

namespace ProyecThor::UI::Settings {

    void SettingsPanel::RenderCategoryProjection() {
        auto& p   = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection;
        bool  changed = false;

        ImGui::TextDisabled("Ajustes que afectan directamente la ventana proyectada.");
        ImGui::Spacing();

        // ── Monitor ──────────────────────────────────────────────────────────
        SectionTitle("Monitor de Salida");
        {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

            if (monitors && monitorCount > 0) {
                // Preparamos los nombres con numeración [0], [1], etc.
                std::vector<std::string> itemLabels;
                std::vector<const char*> names;

                for (int i = 0; i < monitorCount; i++) {
                    std::string label = "[" + std::to_string(i) + "] " + glfwGetMonitorName(monitors[i]);
                    if (i == 0) label += " (Principal)";
                    
                    itemLabels.push_back(label);
                }

                // Convertimos a const char* para que ImGui lo procese
                for (const auto& label : itemLabels) {
                    names.push_back(label.c_str());
                }

                // Lógica de selección automática: 
                // Si el monitor guardado es el 0 (principal) pero hay más pantallas disponibles,
                // y es una "nueva sesión" o no se ha forzado el 0, saltamos a la 1.
                // Nota: Usamos una variable estática o validamos contra p.targetMonitor
                if (p.targetMonitor == 0 && monitorCount > 1) {
                    // Solo forzamos si el usuario no ha movido el selector antes o si quieres
                    // que la app siempre intente evitar el monitor principal por defecto.
                    // p.targetMonitor = 1; 
                    // changed = true;
                }

                int sel = std::clamp(p.targetMonitor, 0, monitorCount - 1);
                
                ImGui::SetNextItemWidth(350); // Un poco más ancho para nombres largos
                if (ImGui::Combo("Monitor de Proyección##mon", &sel, names.data(), (int)names.size())) {
                    p.targetMonitor = sel;
                    changed = true;
                }
                HelpTooltip("Elige en qué pantalla se mostrará la proyección.\nSe recomienda usar la pantalla secundaria (índice 1 o superior).");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No se detectaron monitores adicionales.");
            }
        }
    }

} // namespace ProyecThor::UI::Settings