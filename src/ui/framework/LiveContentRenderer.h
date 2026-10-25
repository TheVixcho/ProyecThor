#pragma once
#include <imgui.h>

namespace ProyecThor::Core {
    struct PresentationState;
}

namespace ProyecThor::UI {

// Fondo + overlay + texto proyectado (o el placeholder "Sin proyección
// activa" si no hay nada al aire). Es exactamente lo que ve el operador en
// "Vista en Vivo" — extraído de ViewPanel::RenderContent para poder
// reutilizarlo desde Stage (ver DrawStageContent) sin duplicar la lógica.
void DrawPublicContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH, bool suppressLyrics = false, bool drawBorder = false);

// Calcula los rectángulos de pantalla de la caja de letras (outBoxMin..outBoxMax)
// y del texto real renderizado (outTextMin..outTextMax) para detección de clics y edición.
bool GetLyricsScreenBounds(const Core::PresentationState& state, ImVec2 p0, float drawW, float drawH,
                           ImVec2& outBoxMin, ImVec2& outBoxMax,
                           ImVec2& outTextMin, ImVec2& outTextMax);

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
