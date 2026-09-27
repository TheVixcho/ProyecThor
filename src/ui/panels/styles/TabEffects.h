#pragma once
#include <imgui.h>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

// TabEffects — tab "Efectos" del editor de estilos.
class TabEffects {
public:
    TabEffects() = default;

    void Render(ProyecThor::Core::TextEffectsData& fx, float colWidth);

private:
    // color4B/angleDegrees/angleLabel: agregados para Degradado de color
    // (colorA=color4, colorB=color4B, angulo) y Transparencia con angulo
    // (sin color, fuerza=intensity, angulo) -- opcionales (nullptr por
    // defecto) para no romper los 7 llamados existentes de un solo color.
    void RenderEffectCard(const char* title, const ImVec4& accent, float colWidth,
                           bool& enabled, float* color4,
                           float* intensity, const char* intensityLabel,
                           float* color4B = nullptr,
                           float* angleDegrees = nullptr, const char* angleLabel = nullptr);
};

} // namespace ProyecThor::UI
