#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace ProyecThor::UI::HomeIcons {

// =============================================================================
//  Punto auxiliar para iconos — mismo patron que LibraryIcons.h::IcPt
// =============================================================================
inline ImVec2 IcPt(ImVec2 o, float sz, float rx, float ry)
{
    return { o.x + rx * sz, o.y + ry * sz };
}

// =============================================================================
//  Iconos dibujados via ImDrawList (sin texturas), uno por seccion de Home
// =============================================================================
inline void DrawIcon_Home(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    // Techo (triangulo) + base (rectangulo)
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.50f, 0.10f),
        IcPt(o, sz, 0.14f, 0.46f),
        IcPt(o, sz, 0.86f, 0.46f), col);
    dl->AddRectFilled(IcPt(o, sz, 0.22f, 0.46f), IcPt(o, sz, 0.78f, 0.90f), col);
    // Puerta (recorte del color de fondo simulado con un rect mas chico
    // no es posible sin conocer el bg, asi que se deja como marco)
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.40f, 0.62f), IcPt(o, sz, 0.60f, 0.90f),
                IM_COL32(0, 0, 0, 90), 0.0f, ImDrawFlags_None, thick);
}

inline void DrawIcon_Clock(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    ImVec2 center = IcPt(o, sz, 0.5f, 0.5f);
    dl->AddCircle(center, sz * 0.40f, col, 20, thick);
    // Manecillas: hora (corta, hacia arriba-derecha) y minuto (larga, hacia arriba)
    dl->AddLine(center, IcPt(o, sz, 0.5f, 0.28f), col, thick * 0.85f);
    dl->AddLine(center, IcPt(o, sz, 0.68f, 0.5f), col, thick * 0.85f);
}

inline void DrawIcon_Megaphone(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    // Cuerpo tipo trompeta (trapecio) apuntando a la izquierda
    ImVec2 body[4] = {
        IcPt(o, sz, 0.14f, 0.42f), IcPt(o, sz, 0.14f, 0.58f),
        IcPt(o, sz, 0.72f, 0.82f), IcPt(o, sz, 0.72f, 0.18f),
    };
    dl->AddConvexPolyFilled(body, 4, col);
    // Boca (elipse) al final del cono
    dl->AddEllipseFilled(IcPt(o, sz, 0.74f, 0.5f),
                         ImVec2(sz * 0.10f, sz * 0.32f), col, 0.f, 12);
    // Mango
    dl->AddRectFilled(IcPt(o, sz, 0.10f, 0.46f), IcPt(o, sz, 0.16f, 0.72f), col, sz * 0.02f);
    // Ondas de sonido
    float thick = sz * 0.06f;
    dl->PathArcTo(IcPt(o, sz, 0.70f, 0.5f), sz * 0.28f, -IM_PI * 0.30f, IM_PI * 0.30f, 10);
    dl->PathStroke(col, ImDrawFlags_None, thick);
}

inline void DrawIcon_Notepad(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.16f, 0.12f), IcPt(o, sz, 0.84f, 0.88f),
                col, sz * 0.06f, ImDrawFlags_RoundCornersAll, thick);
    // Espiral superior
    for (int i = 0; i < 3; i++) {
        float x = 0.34f + i * 0.16f;
        dl->AddCircle(IcPt(o, sz, x, 0.12f), sz * 0.035f, col, 8, thick * 0.6f);
    }
    // Lineas de texto
    const float lx0 = 0.28f, lx1 = 0.72f, ly0 = 0.40f, lgap = 0.14f;
    for (int i = 0; i < 3; i++) {
        float y  = ly0 + i * lgap;
        float x1 = lx1 - (i == 2 ? 0.16f : 0.f);
        dl->AddLine(IcPt(o, sz, lx0, y), IcPt(o, sz, x1, y), col, thick * 0.75f);
    }
}

inline void DrawIcon_Camera(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Visor (bump superior)
    dl->AddRectFilled(IcPt(o, sz, 0.38f, 0.14f), IcPt(o, sz, 0.62f, 0.26f), col, sz * 0.02f);
    // Cuerpo
    dl->AddRect(IcPt(o, sz, 0.12f, 0.26f), IcPt(o, sz, 0.88f, 0.82f),
                col, sz * 0.08f, ImDrawFlags_RoundCornersAll, thick);
    // Lente
    dl->AddCircle(IcPt(o, sz, 0.5f, 0.54f), sz * 0.16f, col, 16, thick);
}

inline void DrawIcon_Broadcast(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Punto central (fuente de la senal)
    dl->AddCircleFilled(IcPt(o, sz, 0.5f, 0.5f), sz * 0.09f, col, 10);
    // Arcos concentricos a ambos lados, como ondas de red/wifi
    float radii[2] = { sz * 0.22f, sz * 0.36f };
    for (float r : radii) {
        dl->PathArcTo(IcPt(o, sz, 0.5f, 0.5f), r, IM_PI * 1.25f, IM_PI * 1.75f, 10);
        dl->PathStroke(col, ImDrawFlags_None, thick * 0.8f);
        dl->PathArcTo(IcPt(o, sz, 0.5f, 0.5f), r, -IM_PI * 0.25f, IM_PI * 0.25f, 10);
        dl->PathStroke(col, ImDrawFlags_None, thick * 0.8f);
    }
}

inline void DrawIcon_Chat(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Globo de dialogo (rect redondeado) + colita apuntando abajo-izquierda
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.68f),
                col, sz * 0.10f, ImDrawFlags_RoundCornersAll, thick);
    ImVec2 tail[3] = {
        IcPt(o, sz, 0.24f, 0.64f), IcPt(o, sz, 0.20f, 0.86f), IcPt(o, sz, 0.40f, 0.64f),
    };
    dl->AddTriangleFilled(tail[0], tail[1], tail[2], col);
    // Puntitos de "escribiendo..." adentro del globo
    for (float x : { 0.34f, 0.50f, 0.66f })
        dl->AddCircleFilled(IcPt(o, sz, x, 0.41f), sz * 0.045f, col, 8);
}

// Estrella de 4 puntas ("sparkle") -- simbolo estandar de facto para "IA" en
// el resto de la industria (Copilot/Gemini/etc), usado para el boton
// "Asistente IA" de la toolbar (ver UIManager::RenderModeToolbar). Se dibuja
// como dos triangulos superpuestos (rombo alargado en cada eje) en vez de un
// poligono de 8 puntos para que los picos queden bien afilados a cualquier
// tamano de icono.
inline void DrawIcon_Sparkle(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    auto star4 = [&](float cx, float cy, float r) {
        // Rombo alargado eje vertical
        dl->AddQuadFilled(
            IcPt(o, sz, cx, cy - r), IcPt(o, sz, cx + r * 0.30f, cy),
            IcPt(o, sz, cx, cy + r), IcPt(o, sz, cx - r * 0.30f, cy), col);
        // Rombo alargado eje horizontal
        dl->AddQuadFilled(
            IcPt(o, sz, cx - r, cy), IcPt(o, sz, cx, cy - r * 0.30f),
            IcPt(o, sz, cx + r, cy), IcPt(o, sz, cx, cy + r * 0.30f), col);
    };
    star4(0.56f, 0.46f, 0.34f);
    star4(0.22f, 0.78f, 0.14f);
}

inline void DrawIcon_Sync(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.09f;
    ImVec2 center = IcPt(o, sz, 0.5f, 0.5f);
    float  r = sz * 0.30f;

    // Dos arcos opuestos (arriba-derecha / abajo-izquierda), como el
    // clasico icono de "sincronizar" de dos flechas circulares.
    dl->PathArcTo(center, r, -IM_PI * 0.85f, IM_PI * 0.05f, 16);
    dl->PathStroke(col, ImDrawFlags_None, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.80f, 0.14f), IcPt(o, sz, 0.80f, 0.34f), IcPt(o, sz, 0.98f, 0.24f), col);

    dl->PathArcTo(center, r, IM_PI * 0.15f, IM_PI * 1.05f, 16);
    dl->PathStroke(col, ImDrawFlags_None, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.20f, 0.86f), IcPt(o, sz, 0.20f, 0.66f), IcPt(o, sz, 0.02f, 0.76f), col);
}

} // namespace ProyecThor::UI::HomeIcons
