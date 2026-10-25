#include "IconRail.h"
#include "core/settings/SettingsManager.h"
#include "DesignSystem.h"
#include <imgui_internal.h>
#include <cmath>
#include <string>
#include <vector>
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

static bool RenderVertical(const IconRailItem* items, int count, int& currentIndex,
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

    bool clickedAny = false;

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = cursor;
        ImVec2 bMax   = { cursor.x + railW, cursor.y + btnH };

        ImGui::SetCursorScreenPos(bMin);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { railW, btnH });
        bool hovered = ImGui::IsItemHovered();
        float t = hovered ? 1.0f : 0.0f;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.14f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
            ac.w = 0.28f;
            dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
        } else if (t > 0.01f) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.12f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
            ac.w = 0.22f;
            dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
        }

        // Barra lateral izquierda (indicador de seleccion)
        if (active || t > 0.01f)
        {
            float barH     = btnH * 0.60f * (active ? 1.0f : t);
            float barY0    = cursor.y + (btnH - barH) * 0.5f;
            float barAlpha = active ? 1.0f : 0.65f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ bMin.x, barY0 }, { bMin.x + 3.0f, barY0 + barH },
                              ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
        }

        {
            ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
            ImVec4 ac       = ImGui::ColorConvertU32ToFloat4(accent);
            float  colorT   = active ? 1.0f : (t * 0.85f);
            ImVec4 icF = {
                Lerp(textDimV.x, ac.x, colorT),
                Lerp(textDimV.y, ac.y, colorT),
                Lerp(textDimV.z, ac.z, colorT),
                1.0f
            };

            ImVec2 lblDim       = ImGui::CalcTextSize(item.label);
            float  totalContent = iconSz + lt * (5.0f + lblDim.y);
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (railW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            if (lt > 0.01f) {
                ImVec4 lblF = {
                    Lerp(textDimV.x, ac.x, colorT),
                    Lerp(textDimV.y, ac.y, colorT),
                    Lerp(textDimV.z, ac.z, colorT),
                    lt
                };
                float lblX = cursor.x + (railW - lblDim.x) * 0.5f;
                float lblY = startY + iconSz + 5.0f;
                dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) {
            currentIndex = item.index;
            clickedAny = true;
        }
    }

    ImGui::PopStyleVar();
    return clickedAny;
}

static bool RenderHorizontal(const IconRailItem* items, int count, int& currentIndex,
                              const float (*categoryColor)[4], float reservedRightPx)
{
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const float  railH  = ImGui::GetContentRegionAvail().y;
    const float  winW   = ImGui::GetWindowWidth();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const float  lt     = RailLabelProgress();

    dl->AddRectFilled(winPos, { winPos.x + winW, winPos.y + railH },
                      DS::GlassFillTop);

    if (count <= 0) return false;

    const float leftPad  = 6.0f;
    const float usableW  = std::max(60.0f, winW - leftPad * 2.0f - reservedRightPx);

    std::vector<ImVec2> lblDims(count);
    std::vector<float>  btnWidths(count);
    float totalIdealW = 0.0f;
    for (int i = 0; i < count; i++) {
        lblDims[i]   = ImGui::CalcTextSize(items[i].label);
        btnWidths[i] = (lt > 0.01f) ? std::max(lblDims[i].x + 24.0f, 64.0f) : 46.0f;
        totalIdealW += btnWidths[i];
    }

    float btnGapX = 4.0f;
    float neededW = totalIdealW + btnGapX * static_cast<float>(count - 1);
    if (neededW > usableW && neededW > 1.0f) {
        const float scale = usableW / neededW;
        btnGapX = std::max(2.0f, std::floor(btnGapX * scale));
        const float availForBtns = std::max(40.0f, usableW - btnGapX * static_cast<float>(count - 1));
        const float btnScale = (totalIdealW > 1.0f) ? (availForBtns / totalIdealW) : 1.0f;
        for (int i = 0; i < count; i++) {
            btnWidths[i] = std::max(34.0f, std::floor(btnWidths[i] * btnScale));
        }
    }

    ImGui::Dummy({ leftPad, railH });
    ImGui::SameLine(0.0f, 0.0f);

    constexpr float rounding = 6.0f;
    const float     iconSz   = std::clamp(std::floor(railH * 0.36f), 15.0f, 18.0f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(btnGapX, 0.f));

    bool clickedAny = false;

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        ImVec2 lblDim = lblDims[i];
        float btnW    = btnWidths[i];

        if (i > 0) ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = { cursor.x, cursor.y + 3.0f };
        ImVec2 bMax   = { cursor.x + btnW, cursor.y + railH - 3.0f };

        ImGui::SetCursorScreenPos(cursor);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, railH });
        bool hovered = ImGui::IsItemHovered();
        float t = hovered ? 1.0f : 0.0f;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.14f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
            ac.w = 0.28f;
            dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
        } else if (t > 0.01f) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = t * 0.12f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
            ac.w = t * 0.22f;
            dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
        }

        // Barra inferior (indicador de seleccion)
        if (active || t > 0.01f)
        {
            float barW     = std::max(12.0f, btnW - 20.0f) * (active ? 1.0f : t);
            float barX0    = cursor.x + (btnW - barW) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.65f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ barX0, bMax.y - 2.5f }, { barX0 + barW, bMax.y },
                              ImGui::ColorConvertFloat4ToU32(ac), 1.5f);
        }

        {
            ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
            ImVec4 ac       = ImGui::ColorConvertU32ToFloat4(accent);
            float  colorT   = active ? 1.0f : (t * 0.85f);
            ImVec4 icF = {
                Lerp(textDimV.x, ac.x, colorT),
                Lerp(textDimV.y, ac.y, colorT),
                Lerp(textDimV.z, ac.z, colorT),
                1.0f
            };

            float  totalContent = iconSz + lt * (4.0f + lblDim.y);
            float  startY       = cursor.y + (railH - totalContent) * 0.5f - (lt > 0.01f ? 1.0f : 0.0f);
            float  iconX        = cursor.x + (btnW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            if (lt > 0.01f) {
                ImVec4 lblF = {
                    Lerp(textDimV.x, ac.x, colorT),
                    Lerp(textDimV.y, ac.y, colorT),
                    Lerp(textDimV.z, ac.z, colorT),
                    lt
                };
                float lblX = std::max(bMin.x + 2.0f, cursor.x + (btnW - lblDim.x) * 0.5f);
                float lblY = startY + iconSz + 4.0f;
                dl->PushClipRect(bMin, bMax, true);
                dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
                dl->PopClipRect();
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) {
            currentIndex = item.index;
            clickedAny = true;
        }
    }

    ImGui::PopStyleVar();
    return clickedAny;
}

bool RenderIconRail(const IconRailItem* items, int count, int& currentIndex,
                     IconRailOrientation orientation, const float (*categoryColor)[4],
                     float reservedRightPx)
{
    if (orientation == IconRailOrientation::Horizontal)
        return RenderHorizontal(items, count, currentIndex, categoryColor, reservedRightPx);
    else
        return RenderVertical(items, count, currentIndex, categoryColor);
}

} // namespace ProyecThor::UI
