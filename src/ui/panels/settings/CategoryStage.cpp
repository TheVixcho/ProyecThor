#include "SettingsPanel.h"
#include <imgui.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Categoria "Stage" — antes era la seccion "Stage" del hub Control (ver
//  ControlPanel, eliminado: su otra mitad, enrutamiento/calidad de
//  proyeccion, ya vivia duplicada aca en Ajustes > Proyeccion). El contenido
//  real (activar/desactivar, plantilla, celdas) sigue viviendo en
//  StageDisplayPanel — esto solo lo monta como una categoria mas.
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI::Settings {

void SettingsPanel::RenderCategoryStage() {
    m_StageDisplay.RenderContent();
}

} // namespace ProyecThor::UI::Settings
