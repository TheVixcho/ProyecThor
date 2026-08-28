#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include "DesignSystem.h"

namespace ProyecThor::Settings { struct ThemeSettings; }

// ─────────────────────────────────────────────────────────────────────────────
//  LayersTheme — paleta de colores y widgets compartidos por todos los tabs
//  del panel de capas.  Incluir en cada .cpp que necesite dibujar UI.
//
//  Los campos no son constexpr: se recalculan en LP::Sync() a partir del
//  tema activo (ver ProyecThor::UI::DS::SyncFromTheme / MonitorTheme::Sync /
//  HubTheme::Sync, mismo patron), asi este panel deja de quedar fijo en un
//  look violeta-oscuro sin importar el preset elegido en Preferencias.
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI {

// ── Paleta ────────────────────────────────────────────────────────────────────
struct LP {  // "Layers Palette"
    // Surfaces
    static inline ImVec4 Base        = {0.07f, 0.08f, 0.10f, 1.0f};
    static inline ImVec4 Surface0    = {0.09f, 0.10f, 0.13f, 1.0f};
    static inline ImVec4 Surface1    = {0.12f, 0.13f, 0.17f, 1.0f};
    static inline ImVec4 Surface2    = {0.16f, 0.17f, 0.22f, 1.0f};
    static inline ImVec4 Surface3    = {0.20f, 0.21f, 0.28f, 1.0f};

    // Borders
    static inline ImVec4 Border      = {0.22f, 0.24f, 0.32f, 0.6f};
    static inline ImVec4 BorderHov   = {0.35f, 0.38f, 0.55f, 0.8f};

    // Accent — sigue el acento del tema activo (antes fijo violeta-azul)
    static inline ImVec4 Accent      = {0.42f, 0.48f, 1.00f, 1.0f};
    static inline ImVec4 AccentHov   = {0.52f, 0.58f, 1.00f, 1.0f};
    static inline ImVec4 AccentDim   = {0.42f, 0.48f, 1.00f, 0.18f};
    static inline ImVec4 AccentActive= {0.32f, 0.38f, 0.90f, 1.0f};

    // Semantic — Green/Red siguen success/danger del tema; Gold no tiene
    // token equivalente en ThemeSettings y queda fijo a proposito (badge de
    // highlight, ya legible sobre cualquier fondo claro u oscuro).
    static inline ImVec4 Gold        = {0.95f, 0.75f, 0.20f, 1.0f};
    static inline ImVec4 GoldDim     = {0.95f, 0.75f, 0.20f, 0.15f};
    static inline ImVec4 Green       = {0.30f, 0.85f, 0.55f, 1.0f};
    static inline ImVec4 GreenDim    = {0.30f, 0.85f, 0.55f, 0.15f};
    static inline ImVec4 Red         = {0.95f, 0.35f, 0.35f, 1.0f};
    static inline ImVec4 RedDim      = {0.95f, 0.35f, 0.35f, 0.15f};

    // Text
    static inline ImVec4 Text        = {0.92f, 0.93f, 0.96f, 1.0f};
    static inline ImVec4 TextSub     = {0.65f, 0.67f, 0.75f, 1.0f};
    static inline ImVec4 TextMuted   = {0.42f, 0.44f, 0.52f, 1.0f};

    // Tab bar sticky
    static inline ImVec4 TabBar      = {0.08f, 0.09f, 0.12f, 1.0f};
    static inline ImVec4 TabActive   = {0.12f, 0.13f, 0.17f, 1.0f};
    static inline ImVec4 TabInactive = {0.09f, 0.10f, 0.13f, 1.0f};

    // Recalcula toda la paleta de arriba a partir del tema activo. Se llama
    // desde SettingsManager::ApplyTheme(), junto a DS::SyncFromTheme().
    static void Sync(const ProyecThor::Settings::ThemeSettings& theme);
};

// ── Conversion helpers ─────────────────────────────────────────────────────
inline ImU32 LPU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ── Shared button widgets ──────────────────────────────────────────────────

inline bool LPPrimaryBtn(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button,        LP::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::AccentHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  LP::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 7.0f));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

inline bool LPGhostBtn(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button,        LP::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface3);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          LP::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 7.0f));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

inline bool LPIconToggle(const char* id, bool active, ImVec2 size = {26, 22}) {
    ImVec4 bg  = active ? ImVec4(0.18f, 0.20f, 0.35f, 1.0f) : LP::Surface1;
    ImVec4 col = active ? LP::Accent : LP::TextMuted;
    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  LP::Surface3);
    ImGui::PushStyleColor(ImGuiCol_Text,          col);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(3.0f, 3.0f));
    bool r = ImGui::Button(id, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

// Dibuja el icono de cuadricula dentro del ultimo boton renderizado
inline void DrawGridIcon(bool active) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 ic = active ? LPU32(LP::Accent) : LPU32(LP::TextMuted);
    float bx = min.x + 5.0f, by = min.y + 4.0f;
    float cs = 4.5f, gap = 2.5f;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 2; c++) {
            float rx = bx + c * (cs + gap);
            float ry = by + r * (cs + gap);
            dl->AddRectFilled({rx, ry}, {rx + cs, ry + cs}, ic, 1.5f);
        }
}

// Dibuja el icono de lista dentro del ultimo boton renderizado
inline void DrawListIcon(bool active) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 ic = active ? LPU32(LP::Accent) : LPU32(LP::TextMuted);
    float bx = min.x + 4.0f, by = min.y + 5.0f;
    for (int i = 0; i < 3; i++) {
        float ry = by + i * 4.2f;
        dl->AddRectFilled({bx, ry}, {bx + 17.0f, ry + 2.3f}, ic, 1.0f);
    }
}

// ── Decoracion: linea separadora degradada ─────────────────────────────────
inline void LPSeparatorLine() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        p, {p.x + w, p.y + 1},
        LPU32({0,0,0,0}),
        LPU32(LP::Accent),
        LPU32({LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.3f}),
        LPU32({0,0,0,0}));
    ImGui::Dummy({0, 5});
}

// ── Badge de texto pequeño ─────────────────────────────────────────────────
inline void LPBadge(ImDrawList* dl, ImVec2 pos, const char* text,
                    ImVec4 bgColor, ImVec4 fgColor,
                    float padX = 5.0f, float padY = 2.0f) {
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled(
        {pos.x - padX, pos.y - padY},
        {pos.x + ts.x + padX, pos.y + ts.y + padY},
        LPU32(bgColor), 5.0f);
    dl->AddText(pos, LPU32(fgColor), text);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Animacion — hover suavizado (mismo patron que IconRail/LibrarySidebar).
//  Guarda el valor entre frames en el ImGuiStorage del contexto actual.
// ─────────────────────────────────────────────────────────────────────────────
inline float LPHoverLerp(ImGuiID id, bool hovered, float speed = 14.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* pT = storage->GetFloatRef(id ^ 0x4C50484Cu, 0.0f); // salt "LPHL"
    float target = hovered ? 1.0f : 0.0f;
    *pT += (target - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *pT;
}

// Version por puntero (para animar cualquier float propio, ej. fade de contenido)
inline float LPApproach(float current, float target, float speed) {
    return current + (target - current) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbars compactas — boton plano solo-icono, sin titulo (ProPresenter-like).
// ─────────────────────────────────────────────────────────────────────────────
using LPDrawIconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

inline bool LPCornerIconBtn(const char* id, LPDrawIconFn drawIcon, const char* tooltip,
                            ImVec2 size = {26.0f, 26.0f}, bool active = false) {
    ImVec4 bg = active ? ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorHov));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    bool clicked = ImGui::Button(id, size);

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImVec2 center = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
    ImU32 col = active ? DS::AccentLight : (ImGui::IsItemHovered() ? DS::TextPrimary : DS::TextSecondary);
    float r = size.x * 0.44f;
    drawIcon(ImGui::GetWindowDrawList(), center, r, col);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

inline void LPDrawAll(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float len = r * 0.52f;
    float th  = 1.5f;
    dl->AddLine({c.x, c.y - len}, {c.x, c.y + len}, col, th);
    dl->AddLine({c.x - len, c.y}, {c.x + len, c.y}, col, th);
    float diag = len * 0.70f;
    dl->AddLine({c.x - diag, c.y - diag}, {c.x + diag, c.y + diag}, col, th * 0.85f);
    dl->AddLine({c.x - diag, c.y + diag}, {c.x + diag, c.y - diag}, col, th * 0.85f);
}

inline void LPDrawPlay(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float w = r * 0.56f;
    float h = r * 0.66f;
    dl->AddTriangleFilled(
        {c.x - w * 0.45f, c.y - h * 0.50f},
        {c.x - w * 0.45f, c.y + h * 0.50f},
        {c.x + w * 0.55f, c.y}, col);
}

inline void LPDrawAudio(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float s = r * 0.52f;
    ImVec2 spk[4] = {
        {c.x - s * 0.70f, c.y - s * 0.32f},
        {c.x - s * 0.70f, c.y + s * 0.32f},
        {c.x - s * 0.15f, c.y + s * 0.65f},
        {c.x - s * 0.15f, c.y - s * 0.65f},
    };
    dl->AddConvexPolyFilled(spk, 4, col);
    dl->PathArcTo({c.x - s * 0.15f, c.y}, s * 0.55f, -IM_PI * 0.30f, IM_PI * 0.30f, 8);
    dl->PathStroke(col, ImDrawFlags_None, 1.4f);
    dl->PathArcTo({c.x - s * 0.15f, c.y}, s * 0.95f, -IM_PI * 0.30f, IM_PI * 0.30f, 8);
    dl->PathStroke(col, ImDrawFlags_None, 1.4f);
}

inline void LPDrawImage(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float w = r * 0.62f, h = r * 0.50f;
    dl->AddRect({c.x - w, c.y - h}, {c.x + w, c.y + h}, col, 1.5f, 0, 1.3f);
    dl->AddCircleFilled({c.x - w * 0.40f, c.y - h * 0.25f}, r * 0.14f, col, 8);
    dl->AddTriangleFilled(
        {c.x - w * 0.75f, c.y + h * 0.70f},
        {c.x - w * 0.10f, c.y - h * 0.05f},
        {c.x + w * 0.45f, c.y + h * 0.70f}, col);
    dl->AddTriangleFilled(
        {c.x + w * 0.10f, c.y + h * 0.70f},
        {c.x + w * 0.45f, c.y + h * 0.20f},
        {c.x + w * 0.85f, c.y + h * 0.70f}, col);
}

inline void LPDrawPlus(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float len = r * 0.52f;
    float th  = 1.5f;
    dl->AddLine({c.x - len, c.y}, {c.x + len, c.y}, col, th);
    dl->AddLine({c.x, c.y - len}, {c.x, c.y + len}, col, th);
}

inline void LPDrawFolderGlyph(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float w = r * 1.3f, h = r * 0.95f;
    ImVec2 tl = {c.x - w * 0.5f, c.y - h * 0.32f};
    dl->AddRectFilled({tl.x, tl.y - h * 0.30f}, {tl.x + w * 0.46f, tl.y + h*0.02f}, col, r * 0.10f);
    dl->AddRectFilled(tl, {tl.x + w, tl.y + h}, col, r * 0.14f);
}

inline void LPDrawFolderPlus(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    LPDrawFolderGlyph(dl, {c.x, c.y + r * 0.12f}, r * 0.65f, col);
    LPDrawPlus(dl, {c.x + r * 0.58f, c.y - r * 0.50f}, r * 0.30f, col);
}

inline void LPDrawRefresh(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float rad = r * 0.50f;
    float th  = 1.4f;
    dl->PathArcTo(c, rad, -IM_PI * 0.55f, IM_PI * 0.90f, 16);
    dl->PathStroke(col, ImDrawFlags_None, th);
    float ang = IM_PI * 0.90f;
    ImVec2 tip = {c.x + rad * std::cos(ang), c.y + rad * std::sin(ang)};
    float asz = r * 0.30f;
    dl->AddTriangleFilled(
        {tip.x, tip.y - asz * 0.3f},
        {tip.x + asz * 0.8f, tip.y + asz * 0.6f},
        {tip.x - asz * 0.6f, tip.y + asz * 0.8f}, col);
}

inline void LPDrawGrid(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float cs = r * 0.34f, g = r * 0.18f;
    for (int rI = 0; rI < 2; rI++) {
        for (int cI = 0; cI < 2; cI++) {
            ImVec2 o = { c.x - cs - g * 0.5f + cI * (cs + g), c.y - cs - g * 0.5f + rI * (cs + g) };
            dl->AddRectFilled(o, {o.x + cs, o.y + cs}, col, 1.2f);
        }
    }
}

inline void LPDrawList(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float w = r * 1.0f, h = r * 0.18f, g = r * 0.18f;
    float totalH = h * 3.0f + g * 2.0f;
    float startY = c.y - totalH * 0.5f;
    for (int i = 0; i < 3; i++) {
        float y = startY + i * (h + g);
        dl->AddRectFilled({c.x - w * 0.5f, y}, {c.x + w * 0.5f, y + h}, col, 1.0f);
    }
}

// ── Slider compacto para controlar el zoom de las miniaturas (grid) ────────
inline bool LPZoomSlider(const char* id, float* zoom, float minZ, float maxZ, float width) {
    bool changed = ProyecThor::UI::DS::ModernSlider(id, zoom, minZ, maxZ, width);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Tamaño de las miniaturas");
    return changed;
}

} // namespace ProyecThor::UI
