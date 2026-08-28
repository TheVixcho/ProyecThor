#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include "LibraryHelpers.h"

namespace ProyecThor::Library {

// =============================================================================
//  Punto auxiliar para iconos
// =============================================================================
inline ImVec2 IcPt(ImVec2 o, float sz, float rx, float ry)
{
    return { o.x + rx * sz, o.y + ry * sz };
}

// =============================================================================
//  Iconos dibujados via ImDrawList
// =============================================================================
inline void DrawIcon_Music(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.08f;
    // Dos notas musicales enlazadas (corchea doble clásica)
    dl->AddEllipseFilled(IcPt(o, sz, 0.28f, 0.74f), ImVec2(sz * 0.15f, sz * 0.11f), col, -0.35f, 16);
    dl->AddEllipseFilled(IcPt(o, sz, 0.70f, 0.64f), ImVec2(sz * 0.15f, sz * 0.11f), col, -0.35f, 16);
    dl->AddLine(IcPt(o, sz, 0.40f, 0.72f), IcPt(o, sz, 0.40f, 0.24f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.82f, 0.62f), IcPt(o, sz, 0.82f, 0.14f), col, thick);
    // Viga superior conectora
    ImVec2 beam[4] = {
        IcPt(o, sz, 0.36f, 0.26f), IcPt(o, sz, 0.86f, 0.16f),
        IcPt(o, sz, 0.86f, 0.06f), IcPt(o, sz, 0.36f, 0.16f),
    };
    dl->AddConvexPolyFilled(beam, 4, col);
}

inline void DrawIcon_Play(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.24f, 0.18f),
        IcPt(o, sz, 0.24f, 0.82f),
        IcPt(o, sz, 0.84f, 0.50f), col);
}

inline void DrawIcon_Image(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    float r     = sz * 0.09f;
    dl->AddRect(IcPt(o, sz, 0.12f, 0.16f), IcPt(o, sz, 0.88f, 0.84f),
                col, r, ImDrawFlags_RoundCornersAll, thick);
    dl->AddCircleFilled(IcPt(o, sz, 0.34f, 0.36f), sz * 0.09f, col, 12);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.16f, 0.80f),
        IcPt(o, sz, 0.52f, 0.46f),
        IcPt(o, sz, 0.84f, 0.80f), col);
}

inline void DrawIcon_Multimedia(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.075f;
    // Marco de claqueta / pantalla multimedia con Play central
    dl->AddRect(IcPt(o, sz, 0.12f, 0.16f), IcPt(o, sz, 0.88f, 0.84f),
                col, sz * 0.10f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddLine(IcPt(o, sz, 0.14f, 0.36f), IcPt(o, sz, 0.86f, 0.36f), col, sz * 0.06f);
    dl->AddLine(IcPt(o, sz, 0.32f, 0.18f), IcPt(o, sz, 0.32f, 0.34f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.50f, 0.18f), IcPt(o, sz, 0.50f, 0.34f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.68f, 0.18f), IcPt(o, sz, 0.68f, 0.34f), col, sz * 0.05f);
    // Triángulo Play central
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.42f, 0.48f),
        IcPt(o, sz, 0.42f, 0.74f),
        IcPt(o, sz, 0.66f, 0.61f), col);
}

inline void DrawIcon_Cross(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Libro Sagrado / Biblia abierta con páginas y cinta de marcador
    dl->AddRect(IcPt(o, sz, 0.12f, 0.20f), IcPt(o, sz, 0.48f, 0.82f),
                col, sz * 0.06f, ImDrawFlags_RoundCornersLeft, thick);
    dl->AddRect(IcPt(o, sz, 0.52f, 0.20f), IcPt(o, sz, 0.88f, 0.82f),
                col, sz * 0.06f, ImDrawFlags_RoundCornersRight, thick);
    // Líneas de texto bíblico
    dl->AddLine(IcPt(o, sz, 0.20f, 0.36f), IcPt(o, sz, 0.40f, 0.36f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.20f, 0.50f), IcPt(o, sz, 0.40f, 0.50f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.20f, 0.64f), IcPt(o, sz, 0.36f, 0.64f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.60f, 0.36f), IcPt(o, sz, 0.80f, 0.36f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.60f, 0.50f), IcPt(o, sz, 0.80f, 0.50f), col, sz * 0.05f);
    dl->AddLine(IcPt(o, sz, 0.60f, 0.64f), IcPt(o, sz, 0.76f, 0.64f), col, sz * 0.05f);
    // Cinta central
    dl->AddLine(IcPt(o, sz, 0.50f, 0.16f), IcPt(o, sz, 0.50f, 0.88f), col, sz * 0.075f);
}

inline void DrawIcon_Document(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    float fold  = sz * 0.22f;
    ImVec2 pts[6] = {
        IcPt(o, sz, 0.16f, 0.10f), IcPt(o, sz, 0.62f, 0.10f),
        IcPt(o, sz, 0.84f, 0.10f + fold / sz), IcPt(o, sz, 0.84f, 0.90f),
        IcPt(o, sz, 0.16f, 0.90f), IcPt(o, sz, 0.16f, 0.10f),
    };
    dl->AddPolyline(pts, 6, col, ImDrawFlags_None, thick);
    dl->AddLine(IcPt(o, sz, 0.62f, 0.10f),
                IcPt(o, sz, 0.62f, 0.10f + fold / sz), col, thick);
    dl->AddLine(IcPt(o, sz, 0.62f, 0.10f + fold / sz),
                IcPt(o, sz, 0.84f, 0.10f + fold / sz), col, thick);
    const float lx0 = 0.28f, lx1 = 0.72f, ly0 = 0.44f, lgap = 0.14f;
    for (int i = 0; i < 3; i++) {
        float y  = ly0 + i * lgap;
        float x1 = lx1 - (i == 2 ? 0.18f : 0.f);
        dl->AddLine(IcPt(o, sz, lx0, y), IcPt(o, sz, x1, y), col, thick * 0.80f);
    }
}

inline void DrawIcon_Audio(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.075f;
    float cx    = o.x + sz * 0.50f;
    float cy    = o.y + sz * 0.50f;
    ImVec2 spk[4] = {
        IcPt(o, sz, 0.16f, 0.36f), IcPt(o, sz, 0.16f, 0.64f),
        IcPt(o, sz, 0.38f, 0.78f), IcPt(o, sz, 0.38f, 0.22f),
    };
    dl->AddConvexPolyFilled(spk, 4, col);
    float radii[2] = { sz * 0.18f, sz * 0.30f };
    for (float r : radii) {
        dl->PathArcTo({ cx - sz * 0.04f, cy }, r,
                      -IM_PI * 0.34f, IM_PI * 0.34f, 10);
        dl->PathStroke(col, ImDrawFlags_None, thick);
    }
}

inline void DrawIcon_Video(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.12f, 0.20f), IcPt(o, sz, 0.88f, 0.80f),
                col, sz * 0.08f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.42f, 0.36f), IcPt(o, sz, 0.42f, 0.64f), IcPt(o, sz, 0.66f, 0.50f), col);
}

// =============================================================================
//  PillButton
// =============================================================================
inline bool PillButton(const char* label, ImVec2 size,
                       ImVec4 colBase, ImVec4 colHov, ImVec4 colAct,
                       ImVec4 colText = { 1.f, 1.f, 1.f, 1.f },
                       float rounding = 7.0f)
{
    ImGuiIO&    io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiID     id = ImGui::GetID(label);
    float* pHover  = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);

    ImVec2 sz = size;
    if (sz.x <= 0.f) sz.x = ImGui::GetContentRegionAvail().x;
    if (sz.y <= 0.f) sz.y = ImGui::GetFrameHeight();

    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 pMax = { pos.x + sz.x, pos.y + sz.y };

    ImGui::InvisibleButton(label, sz);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    *pHover = Lerp(*pHover, hovered ? 1.0f : 0.0f, io.DeltaTime * 12.0f);
    float t = *pHover;

    ImVec4 bgColor   = active ? colAct : LerpColor(colBase, colHov, t);
    float  elevation = active ? 1.0f : Lerp(2.0f, 5.0f, t);

    ImVec4 shadowCol = { 0.f, 0.f, 0.f, Lerp(0.25f, 0.45f, t) };
    dl->AddRectFilled(
        { pos.x + elevation * 0.5f, pos.y + elevation },
        { pMax.x + elevation * 0.5f, pMax.y + elevation },
        ImGui::ColorConvertFloat4ToU32(shadowCol), rounding + 1.0f);

    dl->AddRectFilled(pos, pMax, ImGui::ColorConvertFloat4ToU32(bgColor), rounding);

    float hlAlpha = active ? 0.0f : Lerp(0.10f, 0.22f, t);
    dl->AddRectFilled(pos, { pMax.x, pos.y + sz.y * 0.38f },
                      ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, hlAlpha }),
                      rounding, ImDrawFlags_RoundCornersTop);

    dl->AddRect(pos, pMax,
                IM_COL32(255, 255, 255, (int)(Lerp(0.12f, 0.28f, t) * 255)),
                rounding, ImDrawFlags_None, 0.8f);

    ImVec2 textSz  = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 textPos = { pos.x + (sz.x - textSz.x) * 0.5f,
                       pos.y + (sz.y - textSz.y) * 0.5f };
    dl->AddText({ textPos.x + 1.f, textPos.y + 1.f },
                IM_COL32(0, 0, 0, 140), label);
    dl->AddText(textPos, ImGui::ColorConvertFloat4ToU32(colText), label);

    return clicked;
}

// =============================================================================
//  AccentSep — linea separadora con gradiente horizontal
// =============================================================================
inline void AccentSep(ImVec4 col, float width = -1.f)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = (width < 0.f) ? ImGui::GetContentRegionAvail().x : width;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 c0 = ImGui::ColorConvertFloat4ToU32({ col.x, col.y, col.z, 0.0f });
    ImU32 c1 = ImGui::ColorConvertFloat4ToU32(col);
    dl->AddRectFilledMultiColor(p, { p.x + w, p.y + 1.0f }, c0, c1, c1, c0);
    ImGui::Dummy({ w, 3.0f });
}

// =============================================================================
//  DrawSelectionBars — barras animadas de seleccion activa
// =============================================================================
inline void DrawSelectionBars(ImDrawList* dl, ImVec2 rowMin, float rowH, float baseX)
{
    const float t       = static_cast<float>(ImGui::GetTime());
    const float centerY = rowMin.y + rowH * 0.5f;
    const float minH    = 3.0f;
    const float maxH    = rowH * 0.72f;
    const float barW    = 2.5f;
    const float gap     = 3.0f;
    const int   nBars   = 3;
    const float speed   = 2.2f;
    const float delay   = 0.28f;

    for (int b = 0; b < nBars; b++) {
        float s     = std::sin(t * speed + b * delay * speed);
        float barH  = minH + (s * s) * (maxH - minH);
        float bright = 0.65f + 0.35f * (float)(nBars - 1 - b) / (float)(nBars - 1);
        ImVec4 col  = { 0.30f * bright, 0.62f * bright, 1.0f * bright, 0.95f };
        float bx    = baseX + b * (barW + gap);
        dl->AddRectFilled({ bx, centerY - barH * 0.5f },
                          { bx + barW, centerY + barH * 0.5f },
                          ImGui::ColorConvertFloat4ToU32(col), 1.5f);
    }
}

} // namespace ProyecThor::Library