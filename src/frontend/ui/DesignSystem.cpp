#include "DesignSystem.h"
#include "SettingsManager.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>
#include <cfloat>

namespace ProyecThor::UI::DS {

static ImU32 ToU32(const float* v) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v[0], v[1], v[2], v[3]));
}
static ImU32 ToU32Opaque(const float* v) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v[0], v[1], v[2], 1.0f));
}

// ── BlendOver ────────────────────────────────────────────────────────────
//  Pre-mezcla un tint semitransparente (tint, alpha) contra un color de
//  fondo base, y devuelve el resultado como color SOLIDO (alpha=255).
//  Esto es lo que reemplaza a "poner alpha 255 directo al tint": sin esto,
//  cualquier tint casi-blanco (como t.textPrimary usado al 6%) se veia
//  como blanco puro en vez de una insinuacion sutil sobre el fondo.
static ImU32 BlendOver(const float* tint, float alpha, const float* base)
{
    float r = tint[0] * alpha + base[0] * (1.0f - alpha);
    float g = tint[1] * alpha + base[1] * (1.0f - alpha);
    float b = tint[2] * alpha + base[2] * (1.0f - alpha);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
}

static ImU32 WithAlpha(ImU32 col, int /*a*/)
{
    return (col & 0x00FFFFFFu) | (255u << 24);
}

void SyncFromTheme(const ProyecThor::Settings::ThemeSettings& t) {
    TextPrimary   = ToU32(t.textPrimary);
    TextSecondary = ToU32(t.textDim);
    TextHint      = ToU32(t.textFaint);

    AccentColor    = ToU32(t.accent);
    AccentLight    = ToU32(t.accentLight);
    AccentPastel   = ToU32Opaque(t.accentLight);

    // Antes: tint low-alpha sobre lo que hubiera debajo. Ahora: pre-mezclado
    // contra t.base (el fondo real del panel), asi queda opaco pero con el
    // mismo aspecto visual aproximado.
    AccentColorDim = BlendOver(t.accent, 0.30f, t.base);
    AccentColorHov = BlendOver(t.accent, 0.78f, t.base);

    DangerColor    = ToU32(t.danger);
    DangerColorDim = BlendOver(t.danger, 0.25f, t.base);
    SuccessColor   = ToU32(t.success);

    GlassFillTop   = ToU32Opaque(t.surface1);
    GlassFillBot   = ToU32Opaque(t.base);
    GlassTint      = IM_COL32(0, 0, 0, 0); // sin uso, ya no hay overlay
    GlassBorder    = BlendOver(t.accent,      0.28f, t.base);
    GlassHighlight = BlendOver(t.textPrimary, 0.37f, t.base); // <- el culpable original
    GlassShadow    = IM_COL32(0, 0, 0, 255);

    RowSelectedFill = BlendOver(t.accent,      0.20f, t.base);
    RowSelectedBar  = ToU32(t.accentLight);
    RowHoverFill    = BlendOver(t.textPrimary, 0.06f, t.base); // <- y este tambien

    BtnDefaultFill  = BlendOver(t.textPrimary, 0.06f, t.base); // <- este era el peor: fondo de TODOS los botones "default"
    BtnDefaultBord  = BlendOver(t.accent,      0.22f, t.base);
    BtnHoverFill    = BlendOver(t.accent,      0.11f, t.base);
    BtnHoverBord    = BlendOver(t.accent,      0.35f, t.base);

    SepColor        = BlendOver(t.accent, 0.14f, t.base);
}

bool BeginGlassPanel(const char* name, GlassRenderer& /*glass*/, bool* open,
                     ImGuiWindowFlags flags, ImVec2 windowPadding)
{
    ImVec4 bgCol      = ImGui::ColorConvertU32ToFloat4(GlassFillTop);
    ImVec4 borderCol  = ImGui::ColorConvertU32ToFloat4(GlassBorder);
    ImVec4 titleCol   = ImGui::ColorConvertU32ToFloat4(GlassFillTop);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,          bgCol);
    ImGui::PushStyleColor(ImGuiCol_Border,            borderCol);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,           titleCol);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,     titleCol);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,  titleCol);
    ImGui::PushStyleColor(ImGuiCol_Button,            ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,     ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,      ImVec4(0.9f, 0.1f, 0.1f, 0.8f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,    RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,      windowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,      ImVec2(10.0f, 6.0f));

    bool visible = ImGui::Begin(name, open, flags);

    return visible;
}

void EndGlassPanel()
{
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(8);
}

// ── GlassButton ────────────────────────────────────────────────────────────

bool ModernSlider(const char* id, float* value, float minVal, float maxVal,
                  float width, ImU32 accentOverride, ImU32 trackOverride)
{
    const float w       = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const float thumbR  = 7.0f;
    const float height  = thumbR * 2.0f + 6.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(w, height));

    ImGuiID imId   = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;

    float trackLeft  = pos.x + thumbR;
    float trackRight = pos.x + w - thumbR;
    if (trackRight < trackLeft) { trackLeft = pos.x; trackRight = pos.x + w; }
    float trackSpan  = std::max(1.0f, trackRight - trackLeft);

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && maxVal > minVal)
    {
        float mouseT = std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / trackSpan, 0.0f, 1.0f);
        float newVal = minVal + mouseT * (maxVal - minVal);
        if (newVal != *value) { *value = newVal; changed = true; }
    }

    // Animacion de hover/drag: el thumb crece un poco
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* pT = storage->GetFloatRef(imId ^ 0x4D534C44u, 0.0f); // salt "MSLD"
    float  target = (hovered || active) ? 1.0f : 0.0f;
    *pT += (target - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float t = *pT;

    float frac = (maxVal > minVal)
        ? std::clamp((*value - minVal) / (maxVal - minVal), 0.0f, 1.0f)
        : 0.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cy = pos.y + height * 0.5f;
    const float trackH = 4.0f;

    ImU32 trackCol = trackOverride ? trackOverride : IM_COL32(255, 255, 255, 26);
    ImU32 fillCol  = accentOverride ? accentOverride : AccentColor;

    dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { trackRight, cy + trackH * 0.5f },
                      trackCol, trackH * 0.5f);

    float thumbX = trackLeft + frac * trackSpan;
    if (thumbX > trackLeft)
        dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { thumbX, cy + trackH * 0.5f },
                          fillCol, trackH * 0.5f);

    float thumbRadius = thumbR * (1.0f + 0.2f * t);
    ImVec2 thumbCenter = { thumbX, cy };

    if (t > 0.01f) {
        ImU32 haloAlpha = ((ImU32)std::clamp((int)(50.0f * t), 0, 255)) << 24;
        ImU32 haloCol   = (fillCol & 0x00FFFFFFu) | haloAlpha;
        dl->AddCircleFilled(thumbCenter, thumbRadius + 5.0f * t, haloCol, 20);
    }

    dl->AddCircleFilled(thumbCenter, thumbRadius, fillCol, 20);
    dl->AddCircle(thumbCenter, thumbRadius, IM_COL32(0, 0, 0, 70), 20, 1.3f);

    return changed;
}

bool GlassButton(const char* label, const ImVec2& size, ImU32 accent)
{
    ImVec2 sz = size;
    ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

    if (sz.x <= 0.0f) sz.x = textSize.x + 32.0f;
    if (sz.y <= 0.0f) sz.y = ButtonHeight;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, sz);

    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bMin = cursor;
    ImVec2 bMax = ImVec2(cursor.x + sz.x, cursor.y + sz.y);

    ImU32 bg, border;

    if (active) {
        bg     = WithAlpha(accent, 255);
        border = WithAlpha(accent, 255);
    } else if (hovered) {
        bg     = BtnHoverFill;
        border = BtnHoverBord;
    } else {
        bg     = BtnDefaultFill;
        border = BtnDefaultBord;
    }

    // Plano: un solo tono de relleno y borde fino, sin la linea de brillo
    // superior (era parte del look "liquid glass" que se pidio sacar).
    dl->AddRectFilled(bMin, bMax, bg, RadiusMedium);
    dl->AddRect(bMin, bMax, border, RadiusMedium, 0, 1.0f);

    ImU32 textCol = active ? IM_COL32(255, 255, 255, 255) : TextPrimary;
    ImVec2 tp(
        bMin.x + std::floor((sz.x - textSize.x) * 0.5f),
        bMin.y + std::floor((sz.y - textSize.y) * 0.5f));
    dl->AddText(tp, textCol, label);

    return clicked;
}

// ── GlassIconButton ─────────────────────────────────────────────────────────
// Extraida de LibrarySongs.cpp (ver comentario en DesignSystem.h) — misma
// implementacion exacta, sin cambios de comportamiento.
bool GlassIconButton(const char* id,
                      const char* iconKey,
                      const char* fallbackGlyph,
                      const char* tooltip,
                      ImVec2      size,
                      ImVec4      tint)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(AccentColor));
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon) {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();

        const float minSide  = std::min(size.x, size.y);
        const float iconSide = minSide * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        ImGui::GetWindowDrawList()->AddImage(
            it->second.textureID,
            pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// ── GlassListRow ───────────────────────────────────────────────────────────

bool GlassListRow(const char* label, bool selected, float indent, float height)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float  rowW   = ImGui::GetContentRegionAvail().x;

    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(rowW, height));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rMin = cursor;
    ImVec2 rMax = ImVec2(cursor.x + rowW, cursor.y + height);

    if (selected) {
        // Plano: mismo tratamiento sutil que SongListRow (un solo tono
        // translucido en vez del relleno solido "liquid glass" anterior).
        dl->AddRectFilled(rMin, rMax, IM_COL32(99, 112, 255, 42));

        dl->AddRectFilled(
            rMin,
            ImVec2(rMin.x + 3.0f, rMax.y),
            RowSelectedBar,
            1.5f);

        dl->AddLine(
            ImVec2(rMin.x + 4.0f, rMax.y - 0.5f),
            ImVec2(rMax.x,        rMax.y - 0.5f),
            IM_COL32(99, 112, 255, 40), 1.0f);

    } else if (hovered) {
        dl->AddRectFilled(rMin, rMax, RowHoverFill, RadiusSmall * 0.5f);
        dl->AddRect(rMin, rMax, IM_COL32(255, 255, 255, 18), RadiusSmall * 0.5f, 0, 0.5f);
    }

    ImFont* font = ImGui::GetFont();
    float fontSize     = ImGui::GetFontSize();
    ImVec2 textSz      = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);

    float textX = rMin.x + indent;
    float textY = rMin.y + std::floor((height - textSz.y) * 0.5f);

    ImU32 textCol = selected ? TextPrimary : TextSecondary;
    dl->AddText(ImVec2(textX, textY), textCol, label);

    return clicked;
}

// ── GlassSeparator ─────────────────────────────────────────────────────────

void GlassSeparator(float thickness)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Antes tenia un degrade a transparente en los extremos; ahora es una
    // linea solida pareja.
    dl->AddRectFilled(
        ImVec2(p.x,   p.y),
        ImVec2(p.x+w, p.y + thickness),
        SepColor);

    ImGui::Dummy(ImVec2(w, thickness + 2.0f));
}

// ── GlassSectionHeader ─────────────────────────────────────────────────────

void GlassSectionHeader(const char* label)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float lineY   = p.y + textSz.y * 0.5f;

    dl->AddText(p, TextHint, label);

    float lx0 = p.x + textSz.x + 8.0f;
    float lx1 = p.x + w;
    if (lx1 > lx0)
        dl->AddRectFilled(
            ImVec2(lx0, lineY),
            ImVec2(lx1, lineY + 1.0f),
            SepColor);

    ImGui::Dummy(ImVec2(w, textSz.y + 6.0f));
}

} // namespace ProyecThor::UI::DS