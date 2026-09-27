#pragma once
#include "OverlayTypes.h"
#include <imgui.h>

namespace ProyecThor::UI {

// Dibuja el bloque de texto de una capa Text/Clock (sombra/contorno/fondo +
// texto) con el mismo criterio que usa el editor de Overlays
// (OverlayCanvasEditor::RenderCanvas) -- factorizado aca para que el reloj en
// vivo (LiveContentRenderer.cpp/UIManager.cpp) dibuje exactamente igual que
// el preview del editor, sin duplicar la logica de estilo tres veces.
//
// blockSz: tamano del bloque de texto (ImFont::CalcTextSizeA con displaySize).
// colorOverrideRGBA: si no es null, reemplaza layer.color para el texto
// principal (ej. rojo de "tiempo excedido"); sombra/contorno/fondo siempre
// usan los colores configurados en la capa.
void DrawOverlayLayerStyledText(ImDrawList* dl, ImFont* font, float displaySize,
                                 ImVec2 tl, ImVec2 blockSz, const OverlayLayer& layer,
                                 const char* text, float scale,
                                 const float* colorOverrideRGBA = nullptr);

} // namespace ProyecThor::UI
