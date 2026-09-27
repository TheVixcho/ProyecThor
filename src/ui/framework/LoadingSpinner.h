#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

// Indicador de carga "estilo Discord" (gira, desacelera hasta quedar
// derecho, pausa, y vuelve a girar) usando el icono de la app como sprite.
// Pensado para overlays chicos sobre el PREVIEW del operador — nunca sobre
// la salida real al publico (ver ViewPanel::RenderContent, gateado en
// PresentationCore::IsBackgroundSwapPending()).
void DrawLoadingSpinner(ImDrawList* dl, ImVec2 center, float radius);

} // namespace ProyecThor::UI
