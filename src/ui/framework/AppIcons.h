#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace ProyecThor::UI::AppIcons {

// Mismo helper que LibraryIcons.h::IcPt / HomeIcons.h::IcPt.
inline ImVec2 IcPt(ImVec2 o, float sz, float rx, float ry)
{
    return { o.x + rx * sz, o.y + ry * sz };
}

// Control — sliders de mezcla (mixer)
inline void DrawIcon_Mixer(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    const float xs[3]    = { 0.26f, 0.50f, 0.74f };
    const float knobY[3] = { 0.62f, 0.34f, 0.50f };
    for (int i = 0; i < 3; i++) {
        dl->AddLine(IcPt(o, sz, xs[i], 0.14f), IcPt(o, sz, xs[i], 0.86f), col, thick);
        dl->AddCircleFilled(IcPt(o, sz, xs[i], knobY[i]), sz * 0.09f, col, 12);
    }
}

// Pads — grilla 2x2 de botones (accesos rapidos programados, estilo launchpad)
inline void DrawIcon_Pads(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.065f;
    const float xs[2] = { 0.16f, 0.54f };
    const float ys[2] = { 0.16f, 0.54f };
    for (float x : xs)
        for (float y : ys)
            dl->AddRect(IcPt(o, sz, x, y), IcPt(o, sz, x + 0.30f, y + 0.30f),
                        col, sz * 0.04f, ImDrawFlags_RoundCornersAll, thick);
}

// Yggdrasil — arbol sin hojas (tronco + ramas desnudas), el "arbol del
// mundo": función fundamental de primer nivel, no un icono de red generico.
inline void DrawIcon_Yggdrasil(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.06f;

    // Tronco: de la base hasta donde arrancan las ramas.
    ImVec2 trunkBase = IcPt(o, sz, 0.5f, 0.90f);
    ImVec2 trunkTop  = IcPt(o, sz, 0.5f, 0.46f);
    dl->AddLine(trunkBase, trunkTop, col, thick);

    // Cada rama sale de un punto del tronco y se bifurca una vez, como
    // ramas desnudas de invierno (sin follaje).
    struct Branch { float trunkY, endX, endY, midX, midY; bool left; };
    const Branch branches[] = {
        { 0.46f, 0.14f, 0.14f, 0.28f, 0.28f, true  },
        { 0.46f, 0.86f, 0.14f, 0.72f, 0.28f, false },
        { 0.62f, 0.22f, 0.34f, 0.34f, 0.40f, true  },
        { 0.62f, 0.78f, 0.34f, 0.66f, 0.40f, false },
        { 0.76f, 0.30f, 0.58f, 0.38f, 0.60f, true  },
        { 0.76f, 0.70f, 0.58f, 0.62f, 0.60f, false },
    };

    for (const auto& b : branches) {
        ImVec2 start = IcPt(o, sz, 0.5f, b.trunkY);
        ImVec2 mid   = IcPt(o, sz, b.midX, b.midY);
        ImVec2 end   = IcPt(o, sz, b.endX, b.endY);
        dl->AddLine(start, mid, col, thick * 0.75f);
        dl->AddLine(mid, end, col, thick * 0.55f);

        // Segunda bifurcacion chica en la punta, para que se lea "ramas"
        // y no solo lineas rectas.
        float twigX = b.left ? b.endX + 0.10f : b.endX - 0.10f;
        ImVec2 twig = IcPt(o, sz, twigX, b.endY - 0.08f);
        dl->AddLine(mid, twig, col, thick * 0.45f);
    }
}

// Conexiones — antena con ondas de señal, reemplaza al arbol de ramas
// desnudas de DrawIcon_Yggdrasil (mucha gente lo confundia con un
// pentagrama/simbolo esoterico en vez de con "Conexiones").
inline void DrawIcon_Antenna(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;

    ImVec2 base = IcPt(o, sz, 0.5f, 0.90f);
    ImVec2 tip  = IcPt(o, sz, 0.5f, 0.42f);
    dl->AddLine(base, tip, col, thick);
    dl->AddCircleFilled(tip, sz * 0.06f, col, 12);

    // Ondas de señal en abanico arriba de la punta (mismo lenguaje visual
    // que un icono de WiFi/broadcast), dos arcos concentricos.
    for (int i = 1; i <= 2; i++) {
        float radius = sz * (0.08f + i * 0.07f);
        dl->PathArcTo(tip, radius, -2.356f, -0.785f, 16);
        dl->PathStroke(col, 0, thick * 0.8f);
    }
}

// Stage Display — monitor con base
inline void DrawIcon_Monitor(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.66f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddLine(IcPt(o, sz, 0.5f, 0.66f), IcPt(o, sz, 0.5f, 0.80f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.30f, 0.86f), IcPt(o, sz, 0.70f, 0.86f), col, thick);
}

// Fondos — capas apiladas
inline void DrawIcon_Layers(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.065f;
    const float ys[3] = { 0.28f, 0.50f, 0.72f };
    for (float y : ys) {
        dl->AddRect(IcPt(o, sz, 0.16f, y), IcPt(o, sz, 0.84f, y + 0.16f),
                    col, sz * 0.03f, ImDrawFlags_RoundCornersAll, thick);
    }
}

// Estilos — paleta de pintor
inline void DrawIcon_Palette(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.06f;
    dl->PathArcTo(IcPt(o, sz, 0.5f, 0.55f), sz * 0.36f, IM_PI * 0.85f, IM_PI * 2.65f, 24);
    dl->PathStroke(col, ImDrawFlags_None, thick);
    // Hueco del pulgar
    dl->AddCircleFilled(IcPt(o, sz, 0.5f, 0.78f), sz * 0.08f, col, 10);
    // Puntos de color (mismo color, solo como marcas de la paleta)
    const float dotsX[3] = { 0.32f, 0.50f, 0.68f };
    for (float x : dotsX)
        dl->AddCircleFilled(IcPt(o, sz, x, 0.30f), sz * 0.06f, col, 10);
}

// Overlays — capas superpuestas (layout/overlay multi-nivel)
inline void DrawIcon_Overlay(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.075f;
    // Capa trasera
    dl->AddRect(IcPt(o, sz, 0.28f, 0.16f), IcPt(o, sz, 0.86f, 0.68f),
                col, sz * 0.07f, ImDrawFlags_RoundCornersAll, thick * 0.85f);
    // Capa frontal superpuesta con fondo opaco para dar profundidad
    dl->AddRectFilled(IcPt(o, sz, 0.14f, 0.32f), IcPt(o, sz, 0.72f, 0.84f),
                      IM_COL32(20, 20, 30, 230), sz * 0.07f);
    dl->AddRect(IcPt(o, sz, 0.14f, 0.32f), IcPt(o, sz, 0.72f, 0.84f),
                col, sz * 0.07f, ImDrawFlags_RoundCornersAll, thick);
    // Detalles internos en la capa frontal (texto / barra simulada)
    dl->AddLine(IcPt(o, sz, 0.24f, 0.48f), IcPt(o, sz, 0.62f, 0.48f), col, sz * 0.06f);
    dl->AddLine(IcPt(o, sz, 0.24f, 0.64f), IcPt(o, sz, 0.50f, 0.64f), col, sz * 0.06f);
}

// Web — globo (circulo + meridiano + paralelo), navegador embebido generico
inline void DrawIcon_Globe(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.065f;
    ImVec2 center = IcPt(o, sz, 0.5f, 0.5f);
    float  r = sz * 0.36f;
    dl->AddCircle(center, r, col, 24, thick);
    dl->AddEllipse(center, ImVec2(r * 0.42f, r), col, 0.0f, 24, thick);
    dl->AddLine(IcPt(o, sz, 0.14f, 0.5f), IcPt(o, sz, 0.86f, 0.5f), col, thick);
}

// Estilos — "Aa" (icono tipico de formato de texto/tipografia)
inline void DrawIcon_TextAa(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    const char* label     = "Aa";
    float       fontSize  = sz * 0.60f;
    ImFont*     font      = ImGui::GetFont();
    ImVec2      textSz    = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    ImVec2      pos       = IcPt(o, sz, 0.5f, 0.5f);
    pos.x -= textSz.x * 0.5f;
    pos.y -= textSz.y * 0.5f;
    dl->AddText(font, fontSize, pos, col, label);
}

// Transiciones — flechas cruzadas (swap)
inline void DrawIcon_Swap(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Flecha superior (izquierda a derecha)
    dl->AddLine(IcPt(o, sz, 0.18f, 0.36f), IcPt(o, sz, 0.78f, 0.36f), col, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.70f, 0.22f), IcPt(o, sz, 0.70f, 0.50f), IcPt(o, sz, 0.88f, 0.36f), col);
    // Flecha inferior (derecha a izquierda)
    dl->AddLine(IcPt(o, sz, 0.82f, 0.64f), IcPt(o, sz, 0.22f, 0.64f), col, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.30f, 0.50f), IcPt(o, sz, 0.30f, 0.78f), IcPt(o, sz, 0.12f, 0.64f), col);
}

// Shaders / Efectos — Varita mágica con destellos (Magic Wand)
inline void DrawIcon_Shader(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    const float thick = std::max(1.8f, sz * 0.09f);

    // 1. Palo/vara de la varita (diagonal de abajo-izq a arriba-der)
    dl->AddLine(IcPt(o, sz, 0.16f, 0.84f), IcPt(o, sz, 0.46f, 0.54f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.46f, 0.54f), IcPt(o, sz, 0.62f, 0.38f), col, thick * 1.25f);

    // 2. Gran destello en la punta (estrella de 4 puntas)
    const ImVec2 c = IcPt(o, sz, 0.74f, 0.26f);
    const float rOut = sz * 0.18f;
    const float rIn  = sz * 0.045f;

    ImVec2 pts[8] = {
        { c.x,        c.y - rOut },
        { c.x + rIn,  c.y - rIn  },
        { c.x + rOut, c.y        },
        { c.x + rIn,  c.y + rIn  },
        { c.x,        c.y + rOut },
        { c.x - rIn,  c.y + rIn  },
        { c.x - rOut, c.y        },
        { c.x - rIn,  c.y - rIn  }
    };
    dl->AddConvexPolyFilled(pts, 8, col);

    // 3. Pequeños destellos secundarios (chispas mágicas alrededor)
    {
        const ImVec2 c2 = IcPt(o, sz, 0.50f, 0.18f);
        const float r2 = sz * 0.065f;
        dl->AddLine(ImVec2(c2.x, c2.y - r2), ImVec2(c2.x, c2.y + r2), col, std::max(1.0f, sz * 0.05f));
        dl->AddLine(ImVec2(c2.x - r2, c2.y), ImVec2(c2.x + r2, c2.y), col, std::max(1.0f, sz * 0.05f));
    }
    {
        const ImVec2 c3 = IcPt(o, sz, 0.86f, 0.48f);
        const float r3 = sz * 0.055f;
        dl->AddLine(ImVec2(c3.x, c3.y - r3), ImVec2(c3.x, c3.y + r3), col, std::max(1.0f, sz * 0.05f));
        dl->AddLine(ImVec2(c3.x - r3, c3.y), ImVec2(c3.x + r3, c3.y), col, std::max(1.0f, sz * 0.05f));
    }
    dl->AddCircleFilled(IcPt(o, sz, 0.36f, 0.36f), sz * 0.035f, col, 6);
}

// 3D — Cubo isométrico 3D
inline void DrawIcon_Cube3D(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    ImVec2 center = IcPt(o, sz, 0.5f, 0.5f);
    ImVec2 top    = IcPt(o, sz, 0.5f, 0.16f);
    ImVec2 bot    = IcPt(o, sz, 0.5f, 0.84f);
    ImVec2 midL   = IcPt(o, sz, 0.18f, 0.35f);
    ImVec2 midR   = IcPt(o, sz, 0.82f, 0.35f);
    ImVec2 botL   = IcPt(o, sz, 0.18f, 0.65f);
    ImVec2 botR   = IcPt(o, sz, 0.82f, 0.65f);

    // Cara superior
    dl->AddQuad(top, midR, center, midL, col, thick);
    // Cara izquierda
    dl->AddQuad(midL, center, bot, botL, col, thick);
    // Cara derecha
    dl->AddQuad(center, midR, botR, bot, col, thick);
}

// Lab — Laboratorio Matemático, gráficas y fórmulas f(x)
inline void DrawIcon_Formula(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);
    // Ejes cartesianos (X e Y)
    dl->AddLine(IcPt(o, sz, 0.16f, 0.84f), IcPt(o, sz, 0.88f, 0.84f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.20f, 0.16f), IcPt(o, sz, 0.20f, 0.88f), col, thick);

    // Curva de función suave f(x) estilo sin/parábola
    const int pts = 14;
    ImVec2 prevPt;
    for (int i = 0; i <= pts; ++i) {
        float t = (float)i / (float)pts;
        float px = 0.20f + t * 0.64f;
        float py = 0.72f - 0.44f * std::sin(t * 3.14159f * 0.9f);
        ImVec2 pt = IcPt(o, sz, px, py);
        if (i > 0) {
            dl->AddLine(prevPt, pt, col, thick * 1.3f);
        }
        prevPt = pt;
    }
    // Punto de vértice / evaluación
    dl->AddCircleFilled(IcPt(o, sz, 0.52f, 0.28f), sz * 0.07f, col, 10);
}

inline void DrawIcon_Grid(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    float boxSz = sz * 0.28f;
    float gap = sz * 0.12f;
    float start = (sz - (boxSz * 2.0f + gap)) * 0.5f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            ImVec2 p0 = { o.x + start + x * (boxSz + gap), o.y + start + y * (boxSz + gap) };
            ImVec2 p1 = { p0.x + boxSz, p0.y + boxSz };
            dl->AddRect(p0, p1, col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
        }
    }
}

inline void DrawIcon_Plugin(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    ImVec2 p0 = IcPt(o, sz, 0.20f, 0.20f);
    ImVec2 p1 = IcPt(o, sz, 0.80f, 0.80f);
    dl->AddRect(p0, p1, col, sz * 0.08f, ImDrawFlags_RoundCornersAll, thick);
    
    // Conector superior (notch hacia afuera)
    dl->AddCircleFilled(IcPt(o, sz, 0.50f, 0.20f), sz * 0.11f, col, 12);
    // Conector derecho (notch hacia afuera)
    dl->AddCircleFilled(IcPt(o, sz, 0.80f, 0.50f), sz * 0.11f, col, 12);
    // Agujero central decorativo
    dl->AddCircle(IcPt(o, sz, 0.50f, 0.50f), sz * 0.09f, col, 12, thick);
}

inline void DrawIcon_Settings(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.08f);
    ImVec2 c = IcPt(o, sz, 0.5f, 0.5f);
    float rHole = sz * 0.13f;
    float rRing = sz * 0.28f;
    dl->AddCircle(c, rHole, col, 16, thick);
    dl->AddCircle(c, rRing, col, 20, thick);

    // Dientes radiales del engranaje
    const int teeth = 6;
    for (int i = 0; i < teeth; ++i) {
        float a = (float)i * (2.0f * 3.14159265f / (float)teeth);
        float ca = std::cos(a);
        float sa = std::sin(a);
        ImVec2 p0 = { c.x + ca * (rRing - 0.5f), c.y + sa * (rRing - 0.5f) };
        ImVec2 p1 = { c.x + ca * (rRing + sz * 0.11f), c.y + sa * (rRing + sz * 0.11f) };
        dl->AddLine(p0, p1, col, thick * 1.8f);
    }
}

inline void DrawIcon_Help(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.075f);
    ImVec2 c = IcPt(o, sz, 0.5f, 0.5f);
    dl->AddCircle(c, sz * 0.38f, col, 22, thick);

    // Arco superior del signo ?
    dl->PathArcTo(IcPt(o, sz, 0.50f, 0.38f), sz * 0.13f, -3.14159265f * 0.95f, 3.14159265f * 0.15f, 10);
    dl->PathStroke(col, ImDrawFlags_None, thick);

    // Barra hacia abajo
    dl->AddLine(IcPt(o, sz, 0.50f, 0.51f), IcPt(o, sz, 0.50f, 0.61f), col, thick);

    // Punto inferior
    dl->AddCircleFilled(IcPt(o, sz, 0.50f, 0.73f), sz * 0.045f, col, 8);
}

} // namespace ProyecThor::UI::AppIcons
