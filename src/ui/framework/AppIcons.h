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

// Pads — grilla 3x3 de botones estilo launchpad con pad central iluminado
inline void DrawIcon_Pads(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.0f, sz * 0.055f);
    float padSz = sz * 0.20f;
    float gap   = sz * 0.07f;
    float start = (sz - (padSz * 3.0f + gap * 2.0f)) * 0.5f;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            ImVec2 p0 = { o.x + start + x * (padSz + gap), o.y + start + y * (padSz + gap) };
            ImVec2 p1 = { p0.x + padSz, p0.y + padSz };
            if (x == 1 && y == 1) {
                dl->AddRectFilled(p0, p1, col, sz * 0.04f);
            } else {
                dl->AddRect(p0, p1, col, sz * 0.04f, ImDrawFlags_RoundCornersAll, thick);
            }
        }
    }
}

// Tiles — Pantalla dividida en mosaico / ventanas divididas (Split Screen)
inline void DrawIcon_Tiles(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    ImVec2 p0 = IcPt(o, sz, 0.14f, 0.16f);
    ImVec2 p1 = IcPt(o, sz, 0.86f, 0.84f);
    dl->AddRect(p0, p1, col, sz * 0.06f, ImDrawFlags_RoundCornersAll, thick);

    float midX = p0.x + (p1.x - p0.x) * 0.50f;
    dl->AddLine(ImVec2(midX, p0.y), ImVec2(midX, p1.y), col, thick);

    float midY = p0.y + (p1.y - p0.y) * 0.50f;
    dl->AddLine(ImVec2(midX, midY), ImVec2(p1.x, midY), col, thick);
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

// Stage Display — monitor con base (legacy)
inline void DrawIcon_Monitor(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.66f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddLine(IcPt(o, sz, 0.5f, 0.66f), IcPt(o, sz, 0.5f, 0.80f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.30f, 0.86f), IcPt(o, sz, 0.70f, 0.86f), col, thick);
}

// Preview Multimedia — monitor con símbolo Play ▶ en la pantalla
inline void DrawIcon_MediaPreview(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    // Marco exterior de la pantalla
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.66f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
    // Base y cuello del monitor
    dl->AddLine(IcPt(o, sz, 0.50f, 0.66f), IcPt(o, sz, 0.50f, 0.80f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.32f, 0.84f), IcPt(o, sz, 0.68f, 0.84f), col, thick);

    // Glifo Play ▶ en el centro de la pantalla
    ImVec2 t0 = IcPt(o, sz, 0.44f, 0.28f);
    ImVec2 t1 = IcPt(o, sz, 0.44f, 0.52f);
    ImVec2 t2 = IcPt(o, sz, 0.62f, 0.40f);
    dl->AddTriangleFilled(t0, t1, t2, col);
}

// Stage Display — monitor de escenario con micrófono de púlpito/vocalista en pantalla
inline void DrawIcon_StageDisplay(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.065f);
    // Marco exterior de la pantalla
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.66f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
    // Base y cuello del monitor
    dl->AddLine(IcPt(o, sz, 0.50f, 0.66f), IcPt(o, sz, 0.50f, 0.80f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.32f, 0.84f), IcPt(o, sz, 0.68f, 0.84f), col, thick);

    // Micrófono en el centro de la pantalla
    ImVec2 m0 = IcPt(o, sz, 0.45f, 0.24f);
    ImVec2 m1 = IcPt(o, sz, 0.55f, 0.42f);
    dl->AddRectFilled(m0, m1, col, sz * 0.04f);

    dl->AddLine(IcPt(o, sz, 0.39f, 0.33f), IcPt(o, sz, 0.39f, 0.43f), col, thick * 0.9f);
    dl->AddLine(IcPt(o, sz, 0.61f, 0.33f), IcPt(o, sz, 0.61f, 0.43f), col, thick * 0.9f);
    dl->AddLine(IcPt(o, sz, 0.39f, 0.43f), IcPt(o, sz, 0.61f, 0.43f), col, thick * 0.9f);

    dl->AddLine(IcPt(o, sz, 0.50f, 0.43f), IcPt(o, sz, 0.50f, 0.54f), col, thick * 0.9f);
}

// Proyector / Público — Proyector de video emitiendo haz de luz
inline void DrawIcon_Projector(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Cuerpo del proyector
    dl->AddRect(IcPt(o, sz, 0.12f, 0.34f), IcPt(o, sz, 0.68f, 0.68f),
                col, sz * 0.07f, ImDrawFlags_RoundCornersAll, thick);
    // Lente circular frontal
    dl->AddCircle(IcPt(o, sz, 0.32f, 0.51f), sz * 0.10f, col, 16, thick);
    // Haz de proyección de luz hacia la derecha
    dl->AddLine(IcPt(o, sz, 0.68f, 0.44f), IcPt(o, sz, 0.88f, 0.30f), col, thick * 0.9f);
    dl->AddLine(IcPt(o, sz, 0.68f, 0.58f), IcPt(o, sz, 0.88f, 0.72f), col, thick * 0.9f);
    // Patas de apoyo
    dl->AddLine(IcPt(o, sz, 0.22f, 0.68f), IcPt(o, sz, 0.22f, 0.78f), col, thick * 1.1f);
    dl->AddLine(IcPt(o, sz, 0.58f, 0.68f), IcPt(o, sz, 0.58f, 0.78f), col, thick * 1.1f);
}

// Borrar Todo — Escoba / Limpieza de capas
inline void DrawIcon_Broom(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.075f;
    // Mango de la escoba inclinado
    dl->AddLine(IcPt(o, sz, 0.74f, 0.16f), IcPt(o, sz, 0.46f, 0.52f), col, thick * 1.1f);
    // Base/casquillo del cepillo
    dl->AddLine(IcPt(o, sz, 0.38f, 0.46f), IcPt(o, sz, 0.54f, 0.58f), col, thick * 1.2f);
    // Cerdas del cepillo (abanico triangular)
    dl->AddTriangleFilled(IcPt(o, sz, 0.46f, 0.52f), IcPt(o, sz, 0.18f, 0.76f), IcPt(o, sz, 0.34f, 0.88f), col);
    // Chispas / destellos de limpieza
    dl->AddLine(IcPt(o, sz, 0.20f, 0.24f), IcPt(o, sz, 0.20f, 0.36f), col, thick * 0.85f);
    dl->AddLine(IcPt(o, sz, 0.14f, 0.30f), IcPt(o, sz, 0.26f, 0.30f), col, thick * 0.85f);
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

inline void DrawIcon_Account(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.075f);
    // Cabeza
    ImVec2 headC = IcPt(o, sz, 0.50f, 0.32f);
    dl->AddCircleFilled(headC, sz * 0.16f, col, 16);
    // Torso curvado
    dl->PathClear();
    dl->PathArcTo(IcPt(o, sz, 0.50f, 0.88f), sz * 0.34f, -3.14159f * 0.88f, -3.14159f * 0.12f, 16);
    dl->PathStroke(col, ImDrawFlags_None, thick * 1.4f);
}

// Nube (Cloud Storage / MuninCloud)
inline void DrawIcon_Cloud(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.3f, sz * 0.08f);
    // Base plana horizontal
    dl->AddLine(IcPt(o, sz, 0.22f, 0.74f), IcPt(o, sz, 0.78f, 0.74f), col, thick);
    // Círculos de nubes (rellenos con alpha o contorno sutil)
    dl->AddCircle(IcPt(o, sz, 0.34f, 0.60f), sz * 0.14f, col, 16, thick);
    dl->AddCircle(IcPt(o, sz, 0.52f, 0.44f), sz * 0.20f, col, 18, thick);
    dl->AddCircle(IcPt(o, sz, 0.70f, 0.56f), sz * 0.15f, col, 16, thick);
    // Relleno interno suave y nítido
    ImU32 fillCol = (col & 0x00FFFFFF) | 0x35000000;
    dl->AddRectFilled(IcPt(o, sz, 0.24f, 0.58f), IcPt(o, sz, 0.76f, 0.74f), fillCol);
    dl->AddCircleFilled(IcPt(o, sz, 0.34f, 0.60f), sz * 0.13f, fillCol, 16);
    dl->AddCircleFilled(IcPt(o, sz, 0.52f, 0.44f), sz * 0.19f, fillCol, 18);
    dl->AddCircleFilled(IcPt(o, sz, 0.70f, 0.56f), sz * 0.14f, fillCol, 16);
}

// Papel con Nube (Cancionero en Nube / Cloud Songbook)
inline void DrawIcon_CloudDocument(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);

    // Contorno del documento con esquina doblada
    ImVec2 docPts[6] = {
        IcPt(o, sz, 0.18f, 0.14f),
        IcPt(o, sz, 0.52f, 0.14f),
        IcPt(o, sz, 0.70f, 0.32f),
        IcPt(o, sz, 0.70f, 0.84f),
        IcPt(o, sz, 0.18f, 0.84f),
        IcPt(o, sz, 0.18f, 0.14f)
    };
    ImU32 paperFill = (col & 0x00FFFFFF) | 0x24000000;
    dl->AddConvexPolyFilled(docPts, 5, paperFill);
    dl->AddPolyline(docPts, 6, col, ImDrawFlags_None, thick);

    // Pliegue de la esquina superior derecha
    ImVec2 foldPts[3] = {
        IcPt(o, sz, 0.52f, 0.14f),
        IcPt(o, sz, 0.52f, 0.32f),
        IcPt(o, sz, 0.70f, 0.32f)
    };
    dl->AddPolyline(foldPts, 3, col, ImDrawFlags_None, thick);

    // Líneas de texto / acordes en el documento
    ImU32 lineCol = (col & 0x00FFFFFF) | 0x88000000;
    dl->AddLine(IcPt(o, sz, 0.26f, 0.30f), IcPt(o, sz, 0.44f, 0.30f), lineCol, thick * 0.9f);
    dl->AddLine(IcPt(o, sz, 0.26f, 0.42f), IcPt(o, sz, 0.58f, 0.42f), lineCol, thick * 0.9f);
    dl->AddLine(IcPt(o, sz, 0.26f, 0.54f), IcPt(o, sz, 0.42f, 0.54f), lineCol, thick * 0.9f);

    // Emblema de Nube en la esquina inferior derecha (superpuesta sobre el papel)
    ImU32 cloudMask = IM_COL32(18, 20, 26, 245);
    ImU32 cloudFill = (col & 0x00FFFFFF) | 0x45000000;

    // Máscara opaca para recortar las líneas del papel por debajo de la nube
    dl->AddRectFilled(IcPt(o, sz, 0.42f, 0.68f), IcPt(o, sz, 0.86f, 0.83f), cloudMask, 3.0f);
    dl->AddCircleFilled(IcPt(o, sz, 0.52f, 0.72f), sz * 0.11f, cloudMask, 14);
    dl->AddCircleFilled(IcPt(o, sz, 0.66f, 0.62f), sz * 0.15f, cloudMask, 16);
    dl->AddCircleFilled(IcPt(o, sz, 0.78f, 0.70f), sz * 0.11f, cloudMask, 14);

    // Relleno coloreado de la nube
    dl->AddRectFilled(IcPt(o, sz, 0.42f, 0.68f), IcPt(o, sz, 0.86f, 0.83f), cloudFill, 3.0f);
    dl->AddCircleFilled(IcPt(o, sz, 0.52f, 0.72f), sz * 0.11f, cloudFill, 14);
    dl->AddCircleFilled(IcPt(o, sz, 0.66f, 0.62f), sz * 0.15f, cloudFill, 16);
    dl->AddCircleFilled(IcPt(o, sz, 0.78f, 0.70f), sz * 0.11f, cloudFill, 14);

    // Contorno nítido de la nube
    dl->AddLine(IcPt(o, sz, 0.42f, 0.82f), IcPt(o, sz, 0.86f, 0.82f), col, thick);
    dl->AddCircle(IcPt(o, sz, 0.52f, 0.72f), sz * 0.11f, col, 14, thick);
    dl->AddCircle(IcPt(o, sz, 0.66f, 0.62f), sz * 0.15f, col, 16, thick);
    dl->AddCircle(IcPt(o, sz, 0.78f, 0.70f), sz * 0.11f, col, 14, thick);
}

// Destello / Sparkle IA (DeepSeek / Chat IA)
inline void DrawIcon_Sparkle(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    ImVec2 c = IcPt(o, sz, 0.48f, 0.52f);
    const float rOut = sz * 0.34f;
    const float rIn  = sz * 0.08f;

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

    // Pequeño destello secundario en la esquina superior derecha
    ImVec2 c2 = IcPt(o, sz, 0.78f, 0.26f);
    const float rOut2 = sz * 0.14f;
    const float rIn2  = sz * 0.04f;
    ImVec2 pts2[8] = {
        { c2.x,         c2.y - rOut2 },
        { c2.x + rIn2,  c2.y - rIn2  },
        { c2.x + rOut2, c2.y         },
        { c2.x + rIn2,  c2.y + rIn2  },
        { c2.x,         c2.y + rOut2 },
        { c2.x - rIn2,  c2.y + rIn2  },
        { c2.x - rOut2, c2.y         },
        { c2.x - rIn2,  c2.y - rIn2  }
    };
    dl->AddConvexPolyFilled(pts2, 8, col);
}

// Imagen / Paisaje (Creador de Imágenes IA)
inline void DrawIcon_Image(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);
    // Marco exterior
    ImVec2 p0 = IcPt(o, sz, 0.16f, 0.20f);
    ImVec2 p1 = IcPt(o, sz, 0.84f, 0.80f);
    dl->AddRect(p0, p1, col, sz * 0.06f, ImDrawFlags_RoundCornersAll, thick);

    // Sol / Luna
    dl->AddCircleFilled(IcPt(o, sz, 0.35f, 0.38f), sz * 0.08f, col, 12);

    // Montañas (triángulos)
    ImVec2 m1[3] = {
        IcPt(o, sz, 0.22f, 0.74f),
        IcPt(o, sz, 0.46f, 0.48f),
        IcPt(o, sz, 0.68f, 0.74f)
    };
    dl->AddPolyline(m1, 3, col, ImDrawFlags_None, thick);

    ImVec2 m2[3] = {
        IcPt(o, sz, 0.54f, 0.74f),
        IcPt(o, sz, 0.70f, 0.56f),
        IcPt(o, sz, 0.80f, 0.74f)
    };
    dl->AddPolyline(m2, 3, col, ImDrawFlags_None, thick);
}

// Canva (Integración Oficial Canva Connect API)
inline void DrawIcon_Canva(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.3f, sz * 0.08f);
    ImVec2 c = IcPt(o, sz, 0.50f, 0.50f);
    float r = sz * 0.40f;

    // Círculo exterior elegante
    dl->AddCircle(c, r, col, 24, thick);

    // Letra 'C' estilizada de Canva
    const int n = 16;
    ImVec2 pts[n];
    float a0 = 0.85f * 3.14159f;
    float a1 = -0.85f * 3.14159f;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)(n - 1);
        float angle = a0 + (a1 - a0) * t;
        float curR = r * (0.60f + 0.08f * sinf(t * 3.14159f));
        pts[i] = ImVec2(c.x + curR * cosf(angle), c.y + curR * sinf(angle));
    }
    dl->AddPolyline(pts, n, col, ImDrawFlags_None, thick * 1.3f);

    // Punto creativo superior
    dl->AddCircleFilled(IcPt(o, sz, 0.63f, 0.38f), thick * 0.9f, col, 10);
}

// Cuervo de Odín (Munin & Hugin) — Silueta estilizada de cuervo nórdico
inline void DrawIcon_Raven(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);

    // Silueta de cuervo en perfil mirando a la derecha
    ImVec2 poly[10] = {
        IcPt(o, sz, 0.86f, 0.35f), // Punta del pico
        IcPt(o, sz, 0.64f, 0.22f), // Frente
        IcPt(o, sz, 0.48f, 0.18f), // Coronilla
        IcPt(o, sz, 0.36f, 0.28f), // Nuca
        IcPt(o, sz, 0.26f, 0.48f), // Lomo / parte superior del ala
        IcPt(o, sz, 0.16f, 0.82f), // Punta de las alas
        IcPt(o, sz, 0.32f, 0.78f), // Plumas inferiores
        IcPt(o, sz, 0.46f, 0.64f), // Vientre
        IcPt(o, sz, 0.60f, 0.48f), // Pecho orgulloso
        IcPt(o, sz, 0.66f, 0.36f)  // Garganta / base inferior del pico
    };

    // Relleno suave translúcido
    ImU32 fillCol = (col & 0x00FFFFFF) | 0x38000000;
    dl->AddConvexPolyFilled(poly, 10, fillCol);
    dl->AddPolyline(poly, 10, col, ImDrawFlags_Closed, thick);

    // Pluma interior del ala (detalle estilizado)
    dl->AddLine(IcPt(o, sz, 0.42f, 0.38f), IcPt(o, sz, 0.24f, 0.74f), col, thick * 0.8f);
    dl->AddLine(IcPt(o, sz, 0.50f, 0.44f), IcPt(o, sz, 0.32f, 0.70f), col, thick * 0.6f);

    // Ojo inteligente del cuervo
    dl->AddCircleFilled(IcPt(o, sz, 0.56f, 0.27f), sz * 0.05f, col, 8);
}

// Estadísticas / Telemetría (Hugin) — Gráfico de barras con tendencia ascendente
inline void DrawIcon_Stats(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.3f, sz * 0.08f);
    float round = sz * 0.04f;

    // Base horizontal del gráfico
    dl->AddLine(IcPt(o, sz, 0.14f, 0.84f), IcPt(o, sz, 0.86f, 0.84f), col, thick);

    // Barra 1 (izquierda - baja)
    ImVec2 b1_0 = IcPt(o, sz, 0.20f, 0.58f);
    ImVec2 b1_1 = IcPt(o, sz, 0.34f, 0.84f);
    dl->AddRectFilled(b1_0, b1_1, (col & 0x00FFFFFF) | 0x45000000, round, ImDrawFlags_RoundCornersTop);
    dl->AddRect(b1_0, b1_1, col, round, ImDrawFlags_RoundCornersTop, thick * 0.85f);

    // Barra 2 (centro - media)
    ImVec2 b2_0 = IcPt(o, sz, 0.43f, 0.40f);
    ImVec2 b2_1 = IcPt(o, sz, 0.57f, 0.84f);
    dl->AddRectFilled(b2_0, b2_1, (col & 0x00FFFFFF) | 0x55000000, round, ImDrawFlags_RoundCornersTop);
    dl->AddRect(b2_0, b2_1, col, round, ImDrawFlags_RoundCornersTop, thick * 0.85f);

    // Barra 3 (derecha - alta)
    ImVec2 b3_0 = IcPt(o, sz, 0.66f, 0.22f);
    ImVec2 b3_1 = IcPt(o, sz, 0.80f, 0.84f);
    dl->AddRectFilled(b3_0, b3_1, (col & 0x00FFFFFF) | 0x65000000, round, ImDrawFlags_RoundCornersTop);
    dl->AddRect(b3_0, b3_1, col, round, ImDrawFlags_RoundCornersTop, thick * 0.85f);

    // Línea de tendencia ascendente con flecha
    ImVec2 pA = IcPt(o, sz, 0.18f, 0.50f);
    ImVec2 pB = IcPt(o, sz, 0.48f, 0.34f);
    ImVec2 pC = IcPt(o, sz, 0.82f, 0.16f);
    dl->AddLine(pA, pB, col, thick);
    dl->AddLine(pB, pC, col, thick);

    // Punta de flecha superior derecha
    dl->AddLine(IcPt(o, sz, 0.68f, 0.16f), pC, col, thick * 1.1f);
    dl->AddLine(IcPt(o, sz, 0.82f, 0.30f), pC, col, thick * 1.1f);
}

// Disco de Vinilo / Audio en Vivo
inline void DrawIcon_Disc(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);
    ImVec2 c = IcPt(o, sz, 0.50f, 0.50f);
    // Borde exterior
    dl->AddCircle(c, sz * 0.38f, col, 24, thick);
    // Ranura intermedia
    dl->AddCircle(c, sz * 0.24f, col, 20, 1.0f);
    // Agujero central
    dl->AddCircle(c, sz * 0.10f, col, 16, thick);
    // Punto central
    dl->AddCircleFilled(c, sz * 0.04f, col, 10);
}

// Ventana Externa / Desacoplar (Popout)
inline void DrawIcon_Popout(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);
    // Marco de ventana inferior izquierda
    ImVec2 p0 = IcPt(o, sz, 0.18f, 0.36f);
    ImVec2 p1 = IcPt(o, sz, 0.64f, 0.82f);
    dl->AddRect(p0, p1, col, sz * 0.06f, ImDrawFlags_RoundCornersAll, thick);

    // Flecha diagonal saliente hacia arriba-derecha
    ImVec2 arrowStart = IcPt(o, sz, 0.44f, 0.56f);
    ImVec2 arrowEnd   = IcPt(o, sz, 0.82f, 0.18f);
    dl->AddLine(arrowStart, arrowEnd, col, thick);
    dl->AddLine(IcPt(o, sz, 0.62f, 0.18f), arrowEnd, col, thick);
    dl->AddLine(IcPt(o, sz, 0.82f, 0.38f), arrowEnd, col, thick);
}

} // namespace ProyecThor::UI::AppIcons
