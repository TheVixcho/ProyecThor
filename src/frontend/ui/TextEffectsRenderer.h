#pragma once
#include <imgui.h>
#include "backend/core/PresentationCore.h"

namespace ProyecThor::UI {

// Dibuja un bloque/linea de texto con los efectos visuales configurados en
// TextEffectsData (fondo/borde/sombra/aberración cromática/glow "bloom"/
// neon/subrayado), en capas via ImDrawList -- sin FBO ni shader, mismo
// espiritu "dibujado a mano" que el resto de los widgets custom de este
// codebase (ver DrawPadButton en ViewPanel.cpp para el mismo truco de halo,
// ahora aplicado a texto). Reemplaza el shadowCol fijo que UIManager.cpp y
// LiveContentRenderer.cpp dibujaban antes de este helper.
//
// pos/wrapWidth/textCol: mismos parametros que dl->AddText de siempre
// (wrapWidth = 0.0f para "sin wrap", igual que ImGui). scale: factor de
// conversion de "unidades de referencia 1920px" a pixeles reales de este
// layer (mismo `scale` que ya calculan los callers), para que los
// offsets/grosores de los efectos escalen igual que margenes/texto.
// globalAlpha: multiplica el alpha de TODOS los efectos (no de textCol, que
// el caller ya trae con su propio alpha aplicado) -- para que las
// transiciones con fade (ver UIManager.cpp) desvanezcan tambien fondo/
// borde/glow/etc, no solo el texto principal.
void DrawStyledText(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                     ImU32 textCol, const char* text, float wrapWidth, float scale,
                     const Core::TextEffectsData& fx, float globalAlpha = 1.0f);

} // namespace ProyecThor::UI
