#include "IconRail.h"
#include "backend/settings/SettingsManager.h"
#include "DesignSystem.h"
#include <imgui_internal.h>
#include <cmath>
#include <string>
#include <algorithm>

namespace ProyecThor::UI {

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// ── Grosor animado del rail (ver IconRail.h) ────────────────────────────────
float IconRailThickness(bool vertical)
{
    bool wantLabels = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.showRailLabels;
    float target = vertical
        ? (wantLabels ? kIconRailVerticalSize   : kIconRailVerticalSizeCompact)
        : (wantLabels ? kIconRailHorizontalSize : kIconRailHorizontalSizeCompact);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID(vertical ? "##railThicknessV" : "##railThicknessH");
    float* cur = storage->GetFloatRef(id, target);
    *cur += (target - *cur) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    return *cur;
}

// Progreso animado (0..1) de "mostrar título" — compartido por Vertical/Horizontal
// para que el fade del texto y el recentrado del icono avancen sincronizados
// con el cambio de grosor de arriba.
static float RailLabelProgress()
{
    bool wantLabels = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.showRailLabels;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##railLabelT");
    float* cur = storage->GetFloatRef(id, wantLabels ? 1.0f : 0.0f);
    float target = wantLabels ? 1.0f : 0.0f;
    *cur += (target - *cur) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    return *cur;
}

static void RenderVertical(const IconRailItem* items, int count, int& currentIndex,
                            const float (*categoryColor)[4])
{
    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    const float  railW    = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();
    const float  lt       = RailLabelProgress();

    dl->AddRectFilled(winPos, { winPos.x + railW, winPos.y + winH },
                      DS::GlassFillTop);

    ImGui::Dummy({ railW, 4.0f });

    constexpr float btnGapY  = 1.0f;
    constexpr float rounding = 5.0f;
    const float     btnH    = 46.0f;
    const float     iconSz  = std::floor(btnH * 0.38f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, btnGapY));

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = cursor;
        ImVec2 bMax   = { cursor.x + railW, cursor.y + btnH };

        ImGuiID hovId = ImGui::GetID(item.label);
        float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
        bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
        *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
        float t = *pT;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.12f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        } else if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 14.f)), rounding);
        }

        // Barra lateral izquierda (indicador de seleccion)
        {
            float barH     = btnH * 0.60f * (active ? 1.0f : t);
            float barY0    = cursor.y + (btnH - barH) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.55f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ bMin.x, barY0 }, { bMin.x + 3.0f, barY0 + barH },
                              ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
        }

        ImGui::SetCursorScreenPos(bMin);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { railW, btnH });

        {
            // Base del icono/label = TextSecondary..TextPrimary del TEMA, no
            // un gris fijo que asumia fondo oscuro -- pedido explicito: al
            // seleccionar (active=true) esto quedaba en blanco puro (1.0f),
            // invisible contra un rail con fondo claro (ver DS::GlassFillTop
            // arriba, ya theme-aware). Se sigue mezclando 35%/25% hacia el
            // color de categoria cuando esta activo, igual que antes.
            ImVec4 textPriV = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
            ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
            float  brightT  = active ? 1.0f : t;
            ImVec4 icF = {
                Lerp(textDimV.x, textPriV.x, brightT),
                Lerp(textDimV.y, textPriV.y, brightT),
                Lerp(textDimV.z, textPriV.z, brightT),
                1.0f
            };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                icF.x = Lerp(icF.x, ac.x, 0.35f);
                icF.y = Lerp(icF.y, ac.y, 0.35f);
                icF.z = Lerp(icF.z, ac.z, 0.35f);
                icF.w = 1.0f;
            }

            ImVec2 lblDim       = ImGui::CalcTextSize(item.label);
            float  totalContent = iconSz + lt * (5.0f + lblDim.y);
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (railW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            if (lt > 0.01f) {
                ImVec4 lblF = {
                    Lerp(textDimV.x, textPriV.x, brightT),
                    Lerp(textDimV.y, textPriV.y, brightT),
                    Lerp(textDimV.z, textPriV.z, brightT),
                    lt
                };
                if (active) {
                    ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                    lblF.x = Lerp(lblF.x, ac.x, 0.25f);
                    lblF.y = Lerp(lblF.y, ac.y, 0.25f);
                    lblF.z = Lerp(lblF.z, ac.z, 0.25f);
                }

                float lblX = cursor.x + (railW - lblDim.x) * 0.5f;
                float lblY = startY + iconSz + 5.0f;
                dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) currentIndex = item.index;
    }

    ImGui::PopStyleVar();
}

static void RenderHorizontal(const IconRailItem* items, int count, int& currentIndex,
                              const float (*categoryColor)[4])
{
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const float  railH  = ImGui::GetContentRegionAvail().y;
    const float  winW   = ImGui::GetWindowWidth();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const float  lt     = RailLabelProgress();

    dl->AddRectFilled(winPos, { winPos.x + winW, winPos.y + railH },
                      DS::GlassFillTop);

    ImGui::Dummy({ 6.0f, railH });
    ImGui::SameLine(0.0f, 0.0f);

    constexpr float btnGapX  = 4.0f;
    constexpr float rounding = 6.0f;
    const float     iconSz   = std::clamp(std::floor(railH * 0.36f), 15.0f, 18.0f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(btnGapX, 0.f));

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        ImVec2 lblDim = ImGui::CalcTextSize(item.label);
        float btnW = (lt > 0.01f) ? std::max(lblDim.x + 28.0f, 68.0f) : 46.0f;

        if (i > 0) ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = { cursor.x, cursor.y + 3.0f };
        ImVec2 bMax   = { cursor.x + btnW, cursor.y + railH - 3.0f };

        ImGuiID hovId = ImGui::GetID(item.label);
        float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
        bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
        *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
        float t = *pT;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.14f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
            ac.w = 0.28f;
            dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
        } else if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 18.f)), rounding);
        }

        // Barra inferior (indicador de seleccion)
        {
            float barW     = (btnW - 20.0f) * (active ? 1.0f : t);
            float barX0    = cursor.x + (btnW - barW) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.60f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ barX0, bMax.y - 2.5f }, { barX0 + barW, bMax.y },
                              ImGui::ColorConvertFloat4ToU32(ac), 1.5f);
        }

        ImGui::SetCursorScreenPos(cursor);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, railH });

        {
            // Ver comentario equivalente en RenderVertical: base theme-aware
            // en vez de gris/blanco fijo (invisible en rail con fondo claro).
            ImVec4 textPriV = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
            ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
            float  brightT  = active ? 1.0f : t;
            ImVec4 icF = {
                Lerp(textDimV.x, textPriV.x, brightT),
                Lerp(textDimV.y, textPriV.y, brightT),
                Lerp(textDimV.z, textPriV.z, brightT),
                1.0f
            };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                icF.x = Lerp(icF.x, ac.x, 0.35f);
                icF.y = Lerp(icF.y, ac.y, 0.35f);
                icF.z = Lerp(icF.z, ac.z, 0.35f);
                icF.w = 1.0f;
            }

            float  totalContent = iconSz + lt * (4.0f + lblDim.y);
            float  startY       = cursor.y + (railH - totalContent) * 0.5f - (lt > 0.01f ? 1.0f : 0.0f);
            float  iconX        = cursor.x + (btnW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            if (lt > 0.01f) {
                ImVec4 lblF = {
                    Lerp(textDimV.x, textPriV.x, brightT),
                    Lerp(textDimV.y, textPriV.y, brightT),
                    Lerp(textDimV.z, textPriV.z, brightT),
                    lt
                };
                if (active) {
                    ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                    lblF.x = Lerp(lblF.x, ac.x, 0.25f);
                    lblF.y = Lerp(lblF.y, ac.y, 0.25f);
                    lblF.z = Lerp(lblF.z, ac.z, 0.25f);
                }

                float lblX = cursor.x + (btnW - lblDim.x) * 0.5f;
                float lblY = startY + iconSz + 4.0f;
                dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) currentIndex = item.index;
    }

    ImGui::PopStyleVar();
}

void RenderIconRail(const IconRailItem* items, int count, int& currentIndex,
                     IconRailOrientation orientation, const float (*categoryColor)[4])
{
    if (orientation == IconRailOrientation::Horizontal)
        RenderHorizontal(items, count, currentIndex, categoryColor);
    else
        RenderVertical(items, count, currentIndex, categoryColor);
}

} // namespace ProyecThor::UI
