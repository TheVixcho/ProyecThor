#include "SettingsPanel.h"
#include <imgui.h>
#include <vector>

namespace ProyecThor::UI::Settings {

namespace {

struct ShortcutRow { const char* keys; const char* desc; };

// Dibuja una fila con la combinacion de teclas en un "chip" y su
// descripcion al lado, alineada en una columna fija. Se dibuja a mano con
// ImDrawList (en vez de Columns/Table de ImGui) para tener control total
// del look del chip sin pelear con el padding de las columnas nativas.
void DrawShortcutRow(const char* keys, const char* desc)
{
    ImDrawList* dl       = ImGui::GetWindowDrawList();
    ImVec2      rowStart = ImGui::GetCursorScreenPos();
    const float keyColW  = 190.0f;
    const float rowH     = 30.0f;
    const float padX     = 10.0f;
    const float padY     = 5.0f;

    ImVec2 textSz  = ImGui::CalcTextSize(keys);
    ImVec2 badgeMax(rowStart.x + textSz.x + padX * 2.0f,
                    rowStart.y + textSz.y + padY * 2.0f);

    dl->AddRectFilled(rowStart, badgeMax, IM_COL32(255, 255, 255, 18), 6.0f);
    dl->AddRect(rowStart, badgeMax, IM_COL32(255, 255, 255, 45), 6.0f, 0, 1.0f);
    dl->AddText(ImVec2(rowStart.x + padX, rowStart.y + padY),
                IM_COL32(217, 224, 250, 255), keys);

    dl->AddText(ImVec2(rowStart.x + keyColW, rowStart.y + padY),
                IM_COL32(170, 175, 200, 255), desc);

    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, rowH));
}

} // namespace

void SettingsPanel::RenderCategoryShortcuts() {
    ImGui::TextDisabled("Atajos de teclado disponibles en toda la aplicacion.");
    ImGui::Spacing();

    if (SectionTitle("Navegación en Biblioteca")) {
        DrawShortcutRow("Flecha arriba / abajo", "Mover la seleccion en la lista de la biblioteca");
        DrawShortcutRow("Click derecho",         "Abrir menu contextual (renombrar, eliminar, etiquetas)");
    }

    if (SectionTitle("Navegación en Biblia")) {
        DrawShortcutRow("Flecha izquierda / derecha", "Ir al versiculo anterior / siguiente");
        DrawShortcutRow("Ctrl (toque rapido)",         "Saltar a un capitulo por numero");
        DrawShortcutRow("Alt (toque rapido)",          "Saltar a un versiculo por numero");
        DrawShortcutRow("Enter",                       "Confirmar el salto de capitulo o versiculo");
        DrawShortcutRow("Ctrl + F",                    "Abrir el buscador rapido de la biblia");
    }

    if (SectionTitle("Categorias de Biblioteca")) {
        DrawShortcutRow("Shift + 1", "Ir a Canciones");
        DrawShortcutRow("Shift + 2", "Ir a Video");
        DrawShortcutRow("Shift + 3", "Ir a Imagen");
        DrawShortcutRow("Shift + 4", "Ir a Biblia");
        DrawShortcutRow("Shift + 5", "Ir a Documentos");
        DrawShortcutRow("Shift + 6", "Ir a Audio");
    }

    if (SectionTitle("General")) {
        DrawShortcutRow("Ctrl + P", "Abrir Configuraciones");
        DrawShortcutRow("F1",       "Abrir documentación");
        DrawShortcutRow("Shift + Z", "Abrir / cerrar Notas Rápidas");
        DrawShortcutRow("Alt + F4", "Cerrar ProyecThor");
    }

    if (SectionTitle("Colapsar paneles (Alt Gr)")) {
        DrawShortcutRow("Alt Gr + 1", "Colapsar / mostrar Biblioteca");
        DrawShortcutRow("Alt Gr + 2", "Colapsar / mostrar Diseño");
        DrawShortcutRow("Alt Gr + 3", "Colapsar / mostrar Vista en Vivo");
        DrawShortcutRow("Alt Gr + 4", "Colapsar / mostrar Home");
        DrawShortcutRow("Alt Gr + 0", "Restablecer el entorno de paneles");
    }
}

} // namespace ProyecThor::UI::Settings