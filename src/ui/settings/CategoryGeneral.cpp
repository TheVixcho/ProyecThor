#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

    void SettingsPanel::RenderCategoryGeneral() {
        auto& g = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;

        ImGui::TextDisabled("Opciones generales del comportamiento de la aplicación.");
        ImGui::Spacing();

        SectionTitle("Inicio");
        ImGui::Checkbox("Iniciar minimizado", &g.startMinimized);
        HelpTooltip("Inicia ProyecThor en la barra de tareas sin mostrar la ventana.");
        ImGui::Checkbox("Recordar layout de paneles", &g.rememberLayout);
        HelpTooltip("Guarda la posición y tamaño de cada panel entre sesiones.");
        ImGui::Checkbox("Confirmar al cerrar", &g.confirmOnExit);
        HelpTooltip("Muestra un diálogo de confirmación antes de cerrar la app.");

        SectionTitle("Guardado Automático");
        ImGui::Checkbox("Guardado automático activo", &g.autoSave);
        if (g.autoSave) {
            ImGui::SetNextItemWidth(180);
            ImGui::SliderInt("Intervalo (segundos)", &g.autoSaveIntervalSec, 30, 600);
            HelpTooltip("Cada cuántos segundos se guardan los cambios automáticamente.");
        }

        SectionTitle("Carpetas por Defecto");
        {
            static char bibleBuf[512];
            static bool initBible = false;
            if (!initBible) {
                strncpy(bibleBuf, g.defaultBiblesFolder.c_str(), sizeof(bibleBuf)-1);
                initBible = true;
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("Carpeta de Biblias##bf", bibleBuf, sizeof(bibleBuf)))
                g.defaultBiblesFolder = bibleBuf;
            HelpTooltip("Ruta donde ProyecThor busca archivos XML de Biblia.");
        }
        {
            static char mediaBuf[512];
            static bool initMedia = false;
            if (!initMedia) {
                strncpy(mediaBuf, g.defaultMediaFolder.c_str(), sizeof(mediaBuf)-1);
                initMedia = true;
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("Carpeta de Medios##mf", mediaBuf, sizeof(mediaBuf)))
                g.defaultMediaFolder = mediaBuf;
            HelpTooltip("Ruta donde ProyecThor busca videos y fondos.");
        }
    }

} // namespace ProyecThor::UI::Settings
