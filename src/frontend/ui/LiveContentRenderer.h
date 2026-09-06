#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

// Fondo + overlay + texto proyectado (o el placeholder "Sin proyección
// activa" si no hay nada al aire). Es exactamente lo que ve el operador en
// "Vista en Vivo" — extraído de ViewPanel::RenderContent para poder
// reutilizarlo desde Stage (ver DrawStageContent) sin duplicar la lógica.
void DrawPublicContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH);

// Lo que el Monitor de Control (Stage) muestra ahora mismo: si
// StageDisplaySettings::mirrorPublicOutput está activo, delega en
// DrawPublicContent; si no, dibuja la grilla de celdas configurada
// (Reloj/Texto en vivo/Próxima línea). Único punto de verdad, usado tanto
// por la ventana real de Stage como por el preview de ViewPanel en modo Stage.
void DrawStageContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1);

// Dibuja el fondo activo y el entrante (standby) aplicando la transición elegida
// (Fade, ZoomIn, ZoomOut, Slide, Cover, Uncover, Iris/Teatro, etc.)
void RenderBackgroundWithTransition(ImDrawList* dl,
                                    void* activeTex, void* standbyTex,
                                    ImVec2 pMin, ImVec2 pMax,
                                    int transitionType, float progress,
                                    bool isTransitionActive);

} // namespace ProyecThor::UI
