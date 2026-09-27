#pragma once

namespace ProyecThor::UI::Wiki {

// Un tema por panel -- agregar uno nuevo acá y su entrada en kEntries
// (ver WikiHelp.cpp) es todo lo que hace falta para sumar ayuda a un panel.
enum class Topic {
    ShadersRender,
    OClock,
};

// Dibuja un botón chico "(i)" en la posición actual del cursor (misma línea
// que lo que se esté dibujando antes, ver ImGui::SameLine en el llamador).
// Al hacer clic abre un popup con el título/cuerpo de 'topic' en el idioma
// activo de la interfaz (ver GetUIStrings()) -- reemplaza a los párrafos de
// descripción que antes vivían siempre visibles arriba de cada panel.
void InfoButton(Topic topic);

} // namespace ProyecThor::UI::Wiki
