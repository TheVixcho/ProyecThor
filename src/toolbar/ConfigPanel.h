#pragma once
#include "../panels/IPanel.h" // <-- ¡AQUÍ ESTABA EL ERROR! Subimos un nivel de carpeta
#include <imgui.h>

namespace ProyecThor::UI {

    class ConfigPanel : public IPanel {
    public:
        ConfigPanel() = default;
        
        // Usamos una referencia a un booleano para controlar la visibilidad desde el menú
        void RenderConfig(bool* p_open) {
            if (!*p_open) return;

            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Preferencias", p_open)) {
                
                if (ImGui::BeginTabBar("ConfigTabs")) {
                    if (ImGui::BeginTabItem("General")) {
                        ImGui::Text("Configuración General del Sistema");
                        static char appName[64] = "ProyecThor";
                        ImGui::InputText("Nombre de la App", appName, 64);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Proyección")) {
                        ImGui::Text("Ajustes de Pantalla Secundaria");
                        static int res[2] = { 1920, 1080 };
                        ImGui::InputInt2("Resolución", res);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                
                ImGui::Separator();
                if (ImGui::Button("Guardar cambios")) { /* Lógica de guardado */ }
            }
            ImGui::End();
        }

        // Implementación obligatoria de IPanel
        void Render() override {} 
        std::string GetName() const override { return "Configuración"; }
    };
}