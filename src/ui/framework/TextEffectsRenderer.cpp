#include "TextEffectsRenderer.h"
#include <cmath>
#include <algorithm>

namespace ProyecThor::UI {

using Core::TextEffectsData;

static ImU32 ColU32(const float c[4], float alphaMul = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * alphaMul));
}

// Anillo de N copias del texto a un radio dado, alpha decreciente hacia
// afuera -- aproxima un glow/blur sin necesitar renderizar el texto a una
// textura aparte (ver TextEffectsRenderer.h).
static void DrawTextGlowRing(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                             const char* text, float wrapWidth,
                             const float color[4], float intensity, float maxRadiusPx)
{
    if (intensity <= 0.0f || maxRadiusPx <= 0.0f) return;

    const int kRings       = 3;
    const int kPtsPerRing  = 8;
    for (int r = 1; r <= kRings; r++) {
        float radius = maxRadiusPx * ((float)r / (float)kRings);
        float alpha  = intensity * (1.0f - (float)(r - 1) / (float)kRings) * 0.35f;
        ImU32 col    = ColU32(color, alpha);

        for (int i = 0; i < kPtsPerRing; i++) {
            float a = (6.2831853f / (float)kPtsPerRing) * (float)i;
            ImVec2 off(cosf(a) * radius, sinf(a) * radius);
            dl->AddText(font, fontSize, { pos.x + off.x, pos.y + off.y }, col, text, nullptr, wrapWidth);
        }
    }
}

// Extrusion "solida": copias del texto escalonadas en diagonal, de lejos a
// cerca, todas del mismo color -- forma una especie de bloque/slab detras
// del texto principal (pseudo-3D sin shader/depth buffer).
static void DrawText3D(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                        const char* text, float wrapWidth,
                        const float color[4], float depth01, float globalAlpha, float scale)
{
    if (depth01 <= 0.0f) return;

    int   steps       = 4 + (int)(depth01 * 20.0f); // 4..24 pasos
    float totalOffset = (2.0f + depth01 * 18.0f) * scale;
    ImU32 col = ColU32(color, globalAlpha);

    for (int i = steps; i >= 1; i--) {
        float  t = (float)i / (float)steps;
        ImVec2 off(totalOffset * t, totalOffset * t);
        dl->AddText(font, fontSize, { pos.x + off.x, pos.y + off.y }, col, text, nullptr, wrapWidth);
    }
}

// Angulo (grados) -> vector direccion -> rango de proyeccion de las 4
// esquinas del bloque de texto -- mismo esquema que ya usa la herramienta
// "Degradado" del editor de Overlays (ver OverlayCanvasEditor::
// ApplyGradientPreview), aca aplicado a franjas del bloque de texto en vez
// de pixeles de un bitmap.
static void ProjectRange(float angleDeg, ImVec2 blockSz,
                          float& outMin, float& outMax, float& outDirX, float& outDirY)
{
    float rad = angleDeg * 3.14159265f / 180.0f;
    outDirX = cosf(rad);
    outDirY = sinf(rad);
    float corners[4][2] = { {0.0f,0.0f}, {blockSz.x,0.0f}, {0.0f,blockSz.y}, {blockSz.x,blockSz.y} };
    outMin = FLT_MAX; outMax = -FLT_MAX;
    for (auto& c : corners) {
        float p = c[0] * outDirX + c[1] * outDirY;
        outMin = std::min(outMin, p);
        outMax = std::max(outMax, p);
    }
}

// Dibuja el texto principal en franjas recortadas (clip rects), cada una
// con su propio color/alpha interpolado -- aproxima un degradado de color
// y/o de opacidad sin soporte nativo de ImGui para gradientes por-glifo.
// Redibuja el texto completo por franja (AddText re-wrapea igual cada vez,
// asi que el multi-linea/word-wrap sale correcto sin reimplementarlo).
static void DrawBandedText(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                            const char* text, float wrapWidth, ImU32 baseTextCol,
                            const TextEffectsData& fx, float globalAlpha)
{
    ImVec2 blockSz = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text);
    if (blockSz.x <= 0.0f || blockSz.y <= 0.0f) return;

    float colMin = 0, colMax = 0, colDirX = 0, colDirY = 0;
    float opMin  = 0, opMax  = 0, opDirX  = 0, opDirY  = 0;
    if (fx.gradientEnabled)        ProjectRange(fx.gradientAngle, blockSz, colMin, colMax, colDirX, colDirY);
    if (fx.opacityGradientEnabled) ProjectRange(fx.opacityGradientAngle, blockSz, opMin, opMax, opDirX, opDirY);

    // Orientacion de las franjas de recorte (columnas o filas, siempre
    // ejes-alineadas -- ImGui no soporta clip rects rotados): eje
    // dominante del angulo de color, o del de opacidad si el de color esta
    // apagado. A 0/90/180/270 el resultado es exacto; en angulos
    // intermedios es una aproximacion "escalonada".
    float refAngle    = fx.gradientEnabled ? fx.gradientAngle : fx.opacityGradientAngle;
    float refRad      = refAngle * 3.14159265f / 180.0f;
    bool  verticalCut = std::fabs(cosf(refRad)) >= std::fabs(sinf(refRad));

    constexpr int kBands = 16;
    for (int i = 0; i < kBands; i++) {
        float t0 = (float)i / (float)kBands;
        float t1 = (float)(i + 1) / (float)kBands;

        ImVec2 clipMin, clipMax;
        float  centerX, centerY;
        if (verticalCut) {
            clipMin = { pos.x + t0 * blockSz.x, pos.y - 4.0f };
            clipMax = { pos.x + t1 * blockSz.x, pos.y + blockSz.y + 4.0f };
            centerX = (t0 + t1) * 0.5f * blockSz.x;
            centerY = blockSz.y * 0.5f;
        } else {
            clipMin = { pos.x - 4.0f, pos.y + t0 * blockSz.y };
            clipMax = { pos.x + blockSz.x + 4.0f, pos.y + t1 * blockSz.y };
            centerX = blockSz.x * 0.5f;
            centerY = (t0 + t1) * 0.5f * blockSz.y;
        }

        ImU32 col = baseTextCol;
        if (fx.gradientEnabled) {
            float proj = centerX * colDirX + centerY * colDirY;
            float ct   = std::clamp((proj - colMin) / std::max(1.0f, colMax - colMin), 0.0f, 1.0f);
            float rgba[4];
            for (int c = 0; c < 4; c++)
                rgba[c] = fx.gradientColorA[c] + (fx.gradientColorB[c] - fx.gradientColorA[c]) * ct;
            col = ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3] * globalAlpha));
        }
        if (fx.opacityGradientEnabled) {
            float proj     = centerX * opDirX + centerY * opDirY;
            float ot       = std::clamp((proj - opMin) / std::max(1.0f, opMax - opMin), 0.0f, 1.0f);
            float alphaMul = 1.0f - ot * std::clamp(fx.opacityGradientStrength, 0.0f, 1.0f);
            ImVec4 c4      = ImGui::ColorConvertU32ToFloat4(col);
            col = ImGui::ColorConvertFloat4ToU32(ImVec4(c4.x, c4.y, c4.z, c4.w * alphaMul));
        }

        dl->PushClipRect(clipMin, clipMax, true);
        dl->AddText(font, fontSize, pos, col, text, nullptr, wrapWidth);
        dl->PopClipRect();
    }
}

void DrawStyledText(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                     ImU32 textCol, const char* text, float wrapWidth, float scale,
                     const TextEffectsData& fx, float globalAlpha)
{
    if (!text || !*text || !font) return;
    if (globalAlpha <= 0.001f) return;

    // 1) Fondo -- rectangulo detras de todo el bloque. Padding proporcional
    // al tamaño de fuente, no fijo, para que se vea bien en cualquier
    // resolucion/tamaño de texto.
    if (fx.bgEnabled) {
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text);
        float  pad    = fontSize * 0.22f;
        dl->AddRectFilled(
            { pos.x - pad, pos.y - pad },
            { pos.x + textSz.x + pad, pos.y + textSz.y + pad },
            ColU32(fx.bgColor, globalAlpha), 6.0f * scale);
    }

    // 2) Glow ("bloom") y Neon -- anillos concentricos de copias del texto,
    // mismo truco que el halo de DrawPadButton (ViewPanel.cpp) pero aplicado
    // a glifos. Neon usa un radio mas ajustado (glow "pegado" al texto, mas
    // saturado) en vez del glow difuso y amplio de Bloom.
    if (fx.glowEnabled)
        DrawTextGlowRing(dl, font, fontSize, pos, text, wrapWidth, fx.glowColor, fx.glowIntensity * globalAlpha, fontSize * 0.35f);
    if (fx.neonEnabled)
        DrawTextGlowRing(dl, font, fontSize, pos, text, wrapWidth, fx.neonColor, fx.neonIntensity * globalAlpha, fontSize * 0.20f);

    // 3) Borde -- 8 copias del texto en anillo cerrado detras del texto
    // principal (outline clasico sin shader).
    if (fx.borderEnabled) {
        float radius = (1.0f + fx.borderWidth * 3.0f) * scale;
        ImU32 col    = ColU32(fx.borderColor, globalAlpha);
        static const float kAngles[8] = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };
        for (float degrees : kAngles) {
            float  a = degrees * 3.14159265f / 180.0f;
            ImVec2 off(cosf(a) * radius, sinf(a) * radius);
            dl->AddText(font, fontSize, { pos.x + off.x, pos.y + off.y }, col, text, nullptr, wrapWidth);
        }
    }

    // 3.5) Texto 3D -- extrusion solida, copias escalonadas en diagonal
    // detras del texto principal (ver DrawText3D).
    if (fx.text3dEnabled)
        DrawText3D(dl, font, fontSize, pos, text, wrapWidth, fx.text3dColor, fx.text3dDepth, globalAlpha, scale);

    // 4) Aberracion cromatica -- copias rojo/azul desfasadas horizontalmente
    // detras del texto (verde lo aporta el texto principal, blanco = RGB).
    if (fx.chromaticAberrationEnabled) {
        float shift = fx.chromaticAberrationIntensity * 4.0f * scale;
        ImU32 redCol  = IM_COL32(235, 60, 60, (int)(140 * globalAlpha));
        ImU32 blueCol = IM_COL32(60, 120, 235, (int)(140 * globalAlpha));
        dl->AddText(font, fontSize, { pos.x - shift, pos.y }, redCol, text, nullptr, wrapWidth);
        dl->AddText(font, fontSize, { pos.x + shift, pos.y }, blueCol, text, nullptr, wrapWidth);
    }

    // 5) Sombra -- una copia offset (mismo comportamiento que antes tenia
    // fijo este codebase, ahora configurable).
    if (fx.shadowEnabled) {
        float off = (1.0f + fx.shadowIntensity * 4.0f) * scale;
        dl->AddText(font, fontSize, { pos.x + off, pos.y + off }, ColU32(fx.shadowColor, globalAlpha), text, nullptr, wrapWidth);
    }

    // 6) Texto principal -- degradado de color y/o de opacidad si estan
    // activos (ver DrawBandedText), si no el AddText solido de siempre.
    if (fx.gradientEnabled || fx.opacityGradientEnabled)
        DrawBandedText(dl, font, fontSize, pos, text, wrapWidth, textCol, fx, globalAlpha);
    else
        dl->AddText(font, fontSize, pos, textCol, text, nullptr, wrapWidth);

    // 7) Subrayado.
    if (fx.underlineEnabled) {
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text);
        float  thick  = (1.0f + fx.underlineThickness * 5.0f) * scale;
        float  y      = pos.y + textSz.y + 2.0f * scale;
        dl->AddLine({ pos.x, y }, { pos.x + textSz.x, y }, ColU32(fx.underlineColor, globalAlpha), thick);
    }
}

} // namespace ProyecThor::UI
