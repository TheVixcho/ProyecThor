#include "ShadersPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/ui/WikiHelp.h"
#include "frontend/panels/layers/LayersTheme.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/SystemStats.h"
#include <imgui.h>
#include <cmath>
#include <functional>
#include <algorithm>
#include <vector>
#include <string>

namespace ProyecThor::UI {

namespace {

// ─────────────────────────────────────────────────────────────────────────
//  Iconos Vectoriales a Mano (Sin dependencias externas, alta resolución)
// ─────────────────────────────────────────────────────────────────────────
void IconUpscale(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), col, 3.0f, 0, 1.1f);
    dl->AddLine(ImVec2(c.x - r * 0.35f, c.y + r * 0.35f), ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), col, 1.5f);
    dl->AddLine(ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), ImVec2(c.x - r * 0.05f, c.y - r * 0.35f), col, 1.5f);
    dl->AddLine(ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), ImVec2(c.x + r * 0.35f, c.y + r * 0.05f), col, 1.5f);
}

void IconCRT(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.8f, c.y - r * 0.6f), mx(c.x + r * 0.8f, c.y + r * 0.6f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.3f);
    for (int i = -1; i <= 1; i++) {
        float y = c.y + i * r * 0.35f;
        dl->AddLine(ImVec2(mn.x + 3.0f, y), ImVec2(mx.x - 3.0f, y), col, 1.0f);
    }
}

void IconGrain(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    static const float ox[] = { -0.5f, 0.1f, 0.4f, -0.3f, 0.55f, -0.6f, 0.15f };
    static const float oy[] = { -0.4f, -0.55f, 0.1f, 0.45f, -0.15f, 0.3f, 0.55f };
    for (int i = 0; i < 7; i++)
        dl->AddCircleFilled(ImVec2(c.x + ox[i] * r, c.y + oy[i] * r), r * 0.09f, col);
}

void IconFXAA(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x - r * 0.8f, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y + r * 0.05f));
    dl->PathLineTo(ImVec2(c.x + r * 0.15f, c.y + r * 0.05f));
    dl->PathLineTo(ImVec2(c.x + r * 0.15f, c.y - r * 0.45f));
    dl->PathLineTo(ImVec2(c.x + r * 0.65f, c.y - r * 0.45f));
    dl->PathStroke(col, false, 1.4f);
}

void IconSaturation(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.85f, col, 20, 1.3f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.42f), r * 0.17f, col);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.40f, c.y + r * 0.22f), r * 0.17f, col);
    dl->AddCircleFilled(ImVec2(c.x - r * 0.40f, c.y + r * 0.22f), r * 0.17f, col);
}

void IconVignette(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.6f), mx(c.x + r * 0.85f, c.y + r * 0.6f);
    dl->AddRect(mn, mx, col, 3.0f, 0, 1.2f);
    float cr = r * 0.24f;
    ImU32 corner = IM_COL32(0, 0, 0, 110);
    dl->AddCircleFilled(mn, cr, corner);
    dl->AddCircleFilled(ImVec2(mx.x, mn.y), cr, corner);
    dl->AddCircleFilled(ImVec2(mn.x, mx.y), cr, corner);
    dl->AddCircleFilled(mx, cr, corner);
}

void IconBlur(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    dl->AddCircleFilled(c, r * 0.85f, ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.18f)), 20);
    dl->AddCircleFilled(c, r * 0.55f, ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.40f)), 20);
    dl->AddCircleFilled(c, r * 0.28f, col, 16);
}

void IconSharpen(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x, c.y + r * 0.85f), col, 1.6f);
    dl->AddLine(ImVec2(c.x - r * 0.85f, c.y), ImVec2(c.x + r * 0.85f, c.y), col, 1.6f);
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x, c.y - r * 0.55f));
    dl->PathLineTo(ImVec2(c.x + r * 0.55f, c.y));
    dl->PathLineTo(ImVec2(c.x, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.55f, c.y));
    dl->PathStroke(col, true, 1.6f);
}

void IconBloom(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(c, r * 0.32f, col, 16);
    for (int i = 0; i < 8; i++) {
        float a = (6.28318530f / 8.0f) * (float)i;
        ImVec2 dir(cosf(a), sinf(a));
        ImVec2 p0(c.x + dir.x * r * 0.45f, c.y + dir.y * r * 0.45f);
        ImVec2 p1(c.x + dir.x * r * 0.85f, c.y + dir.y * r * 0.85f);
        dl->AddLine(p0, p1, col, 1.4f);
    }
}

void IconChromaticAberration(ImDrawList* dl, ImVec2 c, float r, ImU32 /*col*/) {
    float off = r * 0.20f;
    dl->AddCircle(ImVec2(c.x - off, c.y), r * 0.5f, IM_COL32(235, 90, 90, 200), 16, 1.4f);
    dl->AddCircle(ImVec2(c.x, c.y),       r * 0.5f, IM_COL32(90, 235, 120, 200), 16, 1.4f);
    dl->AddCircle(ImVec2(c.x + off, c.y), r * 0.5f, IM_COL32(90, 150, 235, 200), 16, 1.4f);
}

void IconFill(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    ImU32  faint1 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.35f));
    ImU32  faint2 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.16f));
    dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), faint2, 3.0f, 0, 3.0f);
    dl->AddRect(ImVec2(c.x - r * 0.62f, c.y - r * 0.62f), ImVec2(c.x + r * 0.62f, c.y + r * 0.62f), faint1, 3.0f, 0, 2.0f);
    dl->AddRectFilled(ImVec2(c.x - r * 0.38f, c.y - r * 0.38f), ImVec2(c.x + r * 0.38f, c.y + r * 0.38f), col, 2.0f);
}

void IconVHS(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.55f), mx(c.x + r * 0.85f, c.y + r * 0.55f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.2f);
    dl->AddCircle(ImVec2(c.x - r * 0.38f, c.y), r * 0.28f, col, 16, 1.3f);
    dl->AddCircle(ImVec2(c.x + r * 0.38f, c.y), r * 0.28f, col, 16, 1.3f);
    dl->AddLine(ImVec2(mn.x + r * 0.15f, mx.y - r * 0.12f), ImVec2(mx.x - r * 0.15f, mx.y - r * 0.12f), col, 1.2f);
}

void IconCine(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.55f, c.y - r * 0.85f), mx(c.x + r * 0.55f, c.y + r * 0.85f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.2f);
    for (int i = -1; i <= 1; i++) {
        float y = c.y + i * r * 0.55f;
        dl->AddRectFilled(ImVec2(mn.x - r * 0.16f, y - r * 0.09f), ImVec2(mn.x, y + r * 0.09f), col, 1.0f);
        dl->AddRectFilled(ImVec2(mx.x, y - r * 0.09f), ImVec2(mx.x + r * 0.16f, y + r * 0.09f), col, 1.0f);
    }
}

void IconContrast(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.75f, col, 24, 1.3f);
    dl->PathArcTo(c, r * 0.75f, -1.5708f, 1.5708f, 16);
    dl->PathFillConvex(col);
}

void IconLuminosity(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.42f, col, 20, 1.4f);
    for (int i = 0; i < 8; i++) {
        float a = (6.28318f / 8.0f) * (float)i;
        ImVec2 dir(cosf(a), sinf(a));
        dl->AddLine(ImVec2(c.x + dir.x * r * 0.62f, c.y + dir.y * r * 0.62f),
                    ImVec2(c.x + dir.x * r * 0.88f, c.y + dir.y * r * 0.88f), col, 1.4f);
    }
}

void IconTAA(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    for (int i = 2; i >= 0; i--) {
        float off = (float)i * r * 0.22f;
        ImU32 c2 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 1.0f - (float)i * 0.3f));
        dl->AddRect(ImVec2(c.x - r * 0.5f + off, c.y - r * 0.5f - off),
                    ImVec2(c.x + r * 0.5f + off, c.y + r * 0.5f - off), c2, 2.0f, 0, 1.3f);
    }
}

void IconGlitch(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Franjas de corte horizontal desfasadas
    dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.65f), ImVec2(c.x + r * 0.4f, c.y - r * 0.25f), col, 1.5f);
    dl->AddRectFilled(ImVec2(c.x - r * 0.3f, c.y - r * 0.20f), ImVec2(c.x + r * 0.7f, c.y + r * 0.20f), IM_COL32(235, 90, 150, 240), 1.5f);
    dl->AddRectFilled(ImVec2(c.x - r * 0.6f, c.y + r * 0.25f), ImVec2(c.x + r * 0.2f, c.y + r * 0.65f), IM_COL32(90, 210, 245, 240), 1.5f);
    dl->AddLine(ImVec2(c.x - r * 0.85f, c.y - r * 0.05f), ImVec2(c.x + r * 0.85f, c.y - r * 0.05f), col, 1.0f);
}

void IconColorGrading(ImDrawList* dl, ImVec2 c, float r, ImU32 /*col*/) {
    // Paleta de gradientes multicolor dividida
    dl->AddCircleFilled(ImVec2(c.x - r * 0.28f, c.y - r * 0.28f), r * 0.38f, IM_COL32(245, 120, 60, 220));
    dl->AddCircleFilled(ImVec2(c.x + r * 0.28f, c.y - r * 0.28f), r * 0.38f, IM_COL32(60, 200, 245, 220));
    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.30f), r * 0.38f, IM_COL32(235, 80, 160, 220));
}

void IconPixelate(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Grid 3x3 de pixeles retro
    float s = r * 0.45f;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            if ((x + y) % 2 == 0) {
                dl->AddRectFilled(ImVec2(c.x + x * s - s * 0.4f, c.y + y * s - s * 0.4f),
                                  ImVec2(c.x + x * s + s * 0.4f, c.y + y * s + s * 0.4f), col, 1.0f);
            } else {
                dl->AddRect(ImVec2(c.x + x * s - s * 0.4f, c.y + y * s - s * 0.4f),
                            ImVec2(c.x + x * s + s * 0.4f, c.y + y * s + s * 0.4f), col, 1.0f, 0, 1.0f);
            }
        }
    }
}

void IconRadialBlur(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.25f, col, 16, 1.5f);
    dl->AddCircle(c, r * 0.55f, col, 16, 1.2f);
    dl->AddCircle(c, r * 0.85f, col, 16, 1.0f);
    for (int i = 0; i < 4; ++i) {
        float a = 0.785398f + (float)i * 1.570796f;
        ImVec2 d(cosf(a), sinf(a));
        dl->AddLine(ImVec2(c.x + d.x * r * 0.3f, c.y + d.y * r * 0.3f),
                    ImVec2(c.x + d.x * r * 0.9f, c.y + d.y * r * 0.9f), col, 1.4f);
    }
}

void IconWaves(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    for (int i = -1; i <= 1; ++i) {
        float yOff = (float)i * r * 0.35f;
        dl->PathClear();
        for (int step = 0; step <= 16; ++step) {
            float xNorm = (float)step / 16.0f;
            float x = c.x - r * 0.8f + xNorm * r * 1.6f;
            float y = c.y + yOff + sinf(xNorm * 6.28318f + (float)i * 0.8f) * r * 0.18f;
            dl->PathLineTo(ImVec2(x, y));
        }
        dl->PathStroke(col, false, 1.4f);
    }
}

void IconMirror(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x - r * 0.8f, c.y - r * 0.6f), ImVec2(c.x + r * 0.8f, c.y + r * 0.6f), col, 2.0f, 0, 1.2f);
    dl->AddLine(ImVec2(c.x, c.y - r * 0.6f), ImVec2(c.x, c.y + r * 0.6f), col, 1.5f);
    dl->AddTriangleFilled(ImVec2(c.x - r * 0.55f, c.y), ImVec2(c.x - r * 0.2f, c.y - r * 0.35f), ImVec2(c.x - r * 0.2f, c.y + r * 0.35f), col);
    dl->AddTriangleFilled(ImVec2(c.x + r * 0.55f, c.y), ImVec2(c.x + r * 0.2f, c.y - r * 0.35f), ImVec2(c.x + r * 0.2f, c.y + r * 0.35f), col);
}

void IconThermal(ImDrawList* dl, ImVec2 c, float r, ImU32 /*col*/) {
    dl->AddCircleFilled(c, r * 0.80f, IM_COL32(30, 20, 120, 255), 20);
    dl->AddCircleFilled(c, r * 0.55f, IM_COL32(230, 60, 60, 255), 20);
    dl->AddCircleFilled(c, r * 0.30f, IM_COL32(255, 230, 70, 255), 20);
}

void IconHalftone(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float radii[3] = { r * 0.26f, r * 0.18f, r * 0.10f };
    float offs[3]  = { -r * 0.5f, 0.0f, r * 0.5f };
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            dl->AddCircleFilled(ImVec2(c.x + offs[x], c.y + offs[y]), radii[(x + y) % 3], col, 12);
        }
    }
}

void IconVolumetricFog(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    for (int i = -1; i <= 1; ++i) {
        float xOff = (float)i * r * 0.38f;
        dl->PathClear();
        dl->PathLineTo(ImVec2(c.x + xOff, c.y + r * 0.65f));
        dl->PathBezierCubicCurveTo(
            ImVec2(c.x + xOff + r * 0.30f, c.y + r * 0.2f),
            ImVec2(c.x + xOff - r * 0.30f, c.y - r * 0.2f),
            ImVec2(c.x + xOff + r * 0.15f, c.y - r * 0.65f)
        );
        dl->PathStroke(col, false, 1.6f);
    }
}

void IconVolumetricClouds(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x + r * 0.40f, c.y - r * 0.30f), r * 0.28f, IM_COL32(255, 215, 80, 220));
    dl->AddCircleFilled(ImVec2(c.x - r * 0.32f, c.y + r * 0.15f), r * 0.32f, col);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.05f, c.y - r * 0.05f), r * 0.40f, col);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.42f, c.y + r * 0.15f), r * 0.30f, col);
    dl->AddRectFilled(ImVec2(c.x - r * 0.32f, c.y + r * 0.10f), ImVec2(c.x + r * 0.42f, c.y + r * 0.45f), col);
}

void IconZonedDistortion(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x - r * 0.8f, c.y - r * 0.6f), ImVec2(c.x + r * 0.8f, c.y + r * 0.6f), col, 2.0f, 0, 1.2f);
    dl->PathClear();
    for (int step = 0; step <= 12; ++step) {
        float xNorm = (float)step / 12.0f;
        float x = c.x - r * 0.65f + xNorm * r * 1.3f;
        float y = c.y + r * 0.22f + sinf(xNorm * 6.28318f) * r * 0.14f;
        dl->PathLineTo(ImVec2(x, y));
    }
    dl->PathStroke(col, false, 1.8f);
    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.22f), r * 0.12f, col);
}

using IconFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

constexpr float kCardHeaderH = 64.0f;
constexpr float kCardDescH   = 8.0f;
constexpr float kCardSliderH = 40.0f;
constexpr float kCardPadding = 16.0f;

float ComputeCardHeight(bool hasSlider, bool hasSecSlider, bool enabled, bool hasMode = false, int modeCount = 0) {
    float sliderH  = (hasSlider && enabled) ? kCardSliderH : 0.0f;
    float secH     = (hasSecSlider && enabled) ? kCardSliderH : 0.0f;
    int   modeRows = (hasMode && enabled) ? ((modeCount > 4) ? 2 : 1) : 0;
    float modeH    = modeRows * (24.0f + 5.0f);
    return kCardHeaderH + kCardDescH + sliderH + secH + modeH + kCardPadding;
}

// ─────────────────────────────────────────────────────────────────────────
//  Tarjeta de Efecto con Diseño Glassmorphism
// ─────────────────────────────────────────────────────────────────────────
bool ShaderCard(ImVec2 origin, const char* id, IconFn icon, ImU32 accent,
                const char* title, const char* desc,
                bool* enabled, const char* sliderLabel,
                float* sliderVal, float sliderMin, float sliderMax, float cardW,
                int* modeVal = nullptr, const char* const* modeLabels = nullptr, int modeCount = 0,
                const char* secSliderLabel = nullptr, float* secSliderVal = nullptr, float secMin = 0.0f, float secMax = 1.0f)
{
    ImGui::PushID(id);
    bool changed = false;

    const bool  hasSlider    = sliderLabel != nullptr;
    const bool  hasSecSlider = secSliderLabel != nullptr && secSliderVal != nullptr;
    const bool  hasMode      = modeVal != nullptr && modeCount > 0;
    const float cardH        = ComputeCardHeight(hasSlider, hasSecSlider, *enabled, hasMode, modeCount);

    ImVec2 p0 = origin;
    ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 accentV = ImGui::ColorConvertU32ToFloat4(accent);
    ImU32 bgCol = *enabled
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.12f))
        : ImGui::ColorConvertFloat4ToU32(ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImU32 borderCol = *enabled
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.70f))
        : DS::BtnDefaultBord;

    // Sombra sutil de acento si está activo
    if (*enabled) {
        dl->AddRectFilled(ImVec2(p0.x - 1.0f, p0.y - 1.0f), ImVec2(p1.x + 1.0f, p1.y + 1.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.08f)), DS::RadiusLarge + 1.0f);
    }

    dl->AddRectFilled(p0, p1, bgCol, DS::RadiusLarge);
    dl->AddRect(p0, p1, borderCol, DS::RadiusLarge, 0, *enabled ? 1.5f : 1.1f);

    // Insignia circular de icono
    ImVec2 iconCenter(p0.x + 34.0f, p0.y + 32.0f);
    dl->AddCircleFilled(iconCenter, 20.0f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, *enabled ? 0.25f : 0.12f)));
    icon(dl, iconCenter, 11.0f, accent);

    // Título
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 62.0f, p0.y + 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    // Estado chico debajo del título
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 62.0f, p0.y + 34.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, *enabled ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
    ImGui::TextUnformatted(*enabled ? "Activo" : "Inactivo");
    ImGui::PopStyleColor();

    // Toggle switch interactivo
    float togR = 12.0f;
    ImVec2 togC(p1.x - 26.0f, p0.y + 26.0f);
    ImGui::SetCursorScreenPos(ImVec2(togC.x - togR, togC.y - togR));
    ImGui::InvisibleButton("##toggle", ImVec2(togR * 2.0f, togR * 2.0f));
    bool togHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        *enabled = !*enabled;
        changed  = true;
    }
    ImU32 togBg = *enabled ? accent : DS::BtnDefaultFill;
    dl->AddCircleFilled(togC, togR, togBg);
    dl->AddCircle(togC, togR, togHovered ? DS::AccentColorHov : DS::BtnDefaultBord, 20, 1.2f);
    if (*enabled) {
        dl->AddLine(ImVec2(togC.x - 4.5f, togC.y), ImVec2(togC.x - 1.0f, togC.y + 4.0f), IM_COL32(20, 20, 24, 255), 1.8f);
        dl->AddLine(ImVec2(togC.x - 1.0f, togC.y + 4.0f), ImVec2(togC.x + 5.5f, togC.y - 4.5f), IM_COL32(20, 20, 24, 255), 1.8f);
    }

    // Tooltip en hover de la tarjeta
    if (ImGui::IsMouseHoveringRect(p0, ImVec2(p1.x - 50.0f, p0.y + kCardHeaderH))) {
        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, accentV);
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", desc);
        ImGui::EndTooltip();
    }

    float curContentY = p0.y + kCardHeaderH + kCardDescH;

    // Primer Slider
    if (hasSlider && *enabled) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted(sliderLabel);
        ImGui::PopStyleColor();

        // Valor numérico formateado
        char valBuf[32];
        if (sliderMax > 10.0f) std::snprintf(valBuf, sizeof(valBuf), "%.0f px", *sliderVal);
        else if (sliderMax > 2.5f) std::snprintf(valBuf, sizeof(valBuf), "%.1f", *sliderVal);
        else std::snprintf(valBuf, sizeof(valBuf), "%.0f%%", *sliderVal * 100.0f);
        ImVec2 valSz = ImGui::CalcTextSize(valBuf);
        ImGui::SameLine(p1.x - 16.0f - valSz.x);
        ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 0.9f), "%s", valBuf);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY + 16.0f));
        if (DS::ModernSlider("##val1", sliderVal, sliderMin, sliderMax, cardW - 32.0f, accent)) {
            changed = true;
        }
        curContentY += kCardSliderH;
    }

    // Segundo Slider (opcional, ej. velocidad u ondas)
    if (hasSecSlider && *enabled) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted(secSliderLabel);
        ImGui::PopStyleColor();

        char valBuf[32];
        std::snprintf(valBuf, sizeof(valBuf), "%.1fx", *secSliderVal);
        ImVec2 valSz = ImGui::CalcTextSize(valBuf);
        ImGui::SameLine(p1.x - 16.0f - valSz.x);
        ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 0.9f), "%s", valBuf);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY + 16.0f));
        if (DS::ModernSlider("##val2", secSliderVal, secMin, secMax, cardW - 32.0f, accent)) {
            changed = true;
        }
        curContentY += kCardSliderH;
    }

    // Selector de modo / presets segmentado (con soporte para 2 filas limpias si hay muchos presets)
    if (hasMode && *enabled) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY + 4.0f));

        const float modeW = cardW - 32.0f;
        const float gap   = 3.0f;

        if (modeCount > 4) {
            int row1Count = (modeCount + 1) / 2;
            int row2Count = modeCount - row1Count;
            float segW1   = (modeW - gap * (float)(row1Count - 1)) / (float)row1Count;
            float segW2   = (modeW - gap * (float)(row2Count - 1)) / (float)row2Count;

            for (int m = 0; m < row1Count; m++) {
                if (m > 0) ImGui::SameLine(0.0f, gap);
                bool active = (*modeVal == m);

                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(accentV.x * 0.35f, accentV.y * 0.35f, accentV.z * 0.55f, 1.0f)
                           : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
                ImGui::PushStyleColor(ImGuiCol_Text, active ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

                std::string btnId = std::string(modeLabels[m]) + "##mode" + std::to_string(m);
                if (ImGui::Button(btnId.c_str(), ImVec2(segW1, 22.0f))) {
                    *modeVal = m;
                    changed  = true;
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, curContentY + 4.0f + 25.0f));
            for (int m = row1Count; m < modeCount; m++) {
                if (m > row1Count) ImGui::SameLine(0.0f, gap);
                bool active = (*modeVal == m);

                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(accentV.x * 0.35f, accentV.y * 0.35f, accentV.z * 0.55f, 1.0f)
                           : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
                ImGui::PushStyleColor(ImGuiCol_Text, active ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

                std::string btnId = std::string(modeLabels[m]) + "##mode" + std::to_string(m);
                if (ImGui::Button(btnId.c_str(), ImVec2(segW2, 22.0f))) {
                    *modeVal = m;
                    changed  = true;
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }
        } else {
            const float segW = (modeW - gap * (float)(modeCount - 1)) / (float)modeCount;
            for (int m = 0; m < modeCount; m++) {
                if (m > 0) ImGui::SameLine(0.0f, gap);
                bool active = (*modeVal == m);

                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(accentV.x * 0.35f, accentV.y * 0.35f, accentV.z * 0.55f, 1.0f)
                           : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
                ImGui::PushStyleColor(ImGuiCol_Text, active ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

                std::string btnId = std::string(modeLabels[m]) + "##mode" + std::to_string(m);
                if (ImGui::Button(btnId.c_str(), ImVec2(segW, 24.0f))) {
                    *modeVal = m;
                    changed  = true;
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }
        }
    }

    ImGui::PopID();
    return changed;
}

} // namespace

void ShadersPanel::RenderContent() {
    auto& settingsMgr = Settings::SettingsManager::Get();
    auto& p           = settingsMgr.GetSettings().projection;
    auto& core        = Core::PresentationCore::Get();
    bool  changed     = false;

    const float availW = ImGui::GetContentRegionAvail().x;

    // ── 1. Cabecera con Título, Ayuda y Zoom ──────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.94f, 0.98f, 1.0f));
    ImGui::SetWindowFontScale(1.05f);
    ImGui::TextUnformatted("Post-Procesado y Shaders de Video");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    Wiki::InfoButton(Wiki::Topic::ShadersRender);

    // Zoom a la derecha (compacto y responsivo)
    const float zoomW = std::clamp(availW * 0.20f, 75.0f, 110.0f);
    if (availW > 380.0f) {
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - zoomW));
        UI::LPZoomSlider("##shaderzoom", &m_ThumbZoom, 0.7f, 1.4f, zoomW);
    }

    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    // ── 2. Barra de Búsqueda y Botón Restablecer (Fila 1) ──────────────────────
    const float resetBtnW = 120.0f;
    const float searchW   = std::max(120.0f, availW - resetBtnW - 14.0f);

    ImGui::SetNextItemWidth(std::min(searchW, 320.0f));
    ImGui::InputTextWithHint("##shaderSearch", "🔍 Buscar efecto...", m_SearchFilter, sizeof(m_SearchFilter));
    if (m_SearchFilter[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::Button("✕##clearSearch", ImVec2(24.0f, 0.0f))) {
            m_SearchFilter[0] = '\0';
        }
    }

    // Botón Restablecer Todo — fijado a la derecha de la fila de búsqueda para que NUNCA se corte
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - resetBtnW));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 35, 45, 180));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(140, 45, 60, 230));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(245, 140, 150, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
    if (ImGui::Button("↺ Restablecer", ImVec2(resetBtnW, 0.0f))) {
        p.crtEnabled = false; p.grainEnabled = false; p.fxaaEnabled = false;
        p.saturationEnabled = false; p.vignetteEnabled = false; p.blurEnabled = false;
        p.sharpenEnabled = false; p.bloomEnabled = false; p.chromaticAberrationEnabled = false;
        p.vhsEnabled = false; p.cineEnabled = false; p.contrastEnabled = false;
        p.luminosityEnabled = false; p.taaEnabled = false; p.glitchEnabled = false;
        p.colorGradingEnabled = false; p.pixelateEnabled = false; p.radialBlurEnabled = false;
        p.wavesEnabled = false; p.mirrorEnabled = false; p.thermalEnabled = false;
        p.halftoneEnabled = false;
        p.volumetricFogEnabled = false; p.volumetricCloudsEnabled = false; p.zonedDistortionEnabled = false;
        core.SetCRTEnabled(false); core.SetGrainEnabled(false); core.SetFXAAEnabled(false);
        core.SetSaturationEnabled(false); core.SetVignetteEnabled(false); core.SetBlurEnabled(false);
        core.SetSharpenEnabled(false); core.SetBloomEnabled(false); core.SetChromaticAberrationEnabled(false);
        core.SetVHSEnabled(false); core.SetCineEnabled(false); core.SetContrastEnabled(false);
        core.SetLuminosityEnabled(false); core.SetTAAEnabled(false); core.SetGlitchEnabled(false);
        core.SetColorGradingEnabled(false); core.SetPixelateEnabled(false); core.SetRadialBlurEnabled(false);
        core.SetWavesEnabled(false); core.SetMirrorEnabled(false); core.SetThermalEnabled(false);
        core.SetHalftoneEnabled(false);
        core.SetVolumetricFogEnabled(false); core.SetVolumetricCloudsEnabled(false); core.SetZonedDistortionEnabled(false);
        changed = true;
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // ── 3. Presets Rápidos con Auto-Wrap Responsivo (Fila 2) ───────────────────
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.9f), "Presets:");

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));

    auto RenderPresetBtn = [&](const char* label, auto onClickFn) {
        float btnW = ImGui::CalcTextSize(label).x + 16.0f;
        if (ImGui::GetContentRegionAvail().x >= btnW + 4.0f) {
            ImGui::SameLine(0.0f, 4.0f);
        }
        if (ImGui::Button(label)) {
            onClickFn();
            changed = true;
        }
    };

    RenderPresetBtn("🎬 Cine Épico", [&]() {
        p.cineEnabled = true; p.cineIntensity = 0.65f; p.cineTint = 0; core.SetCineEnabled(true); core.SetCineIntensity(0.65f);
        p.vignetteEnabled = true; p.vignetteIntensity = 0.40f; core.SetVignetteEnabled(true); core.SetVignetteIntensity(0.40f);
        p.bloomEnabled = true; p.bloomIntensity = 0.25f; core.SetBloomEnabled(true); core.SetBloomIntensity(0.25f);
        p.chromaticAberrationEnabled = true; p.chromaticAberrationIntensity = 0.20f; core.SetChromaticAberrationEnabled(true);
    });

    RenderPresetBtn("📺 Retro VHS", [&]() {
        p.vhsEnabled = true; p.vhsIntensity = 0.60f; core.SetVHSEnabled(true); core.SetVHSIntensity(0.60f);
        p.crtEnabled = true; p.crtScanlineIntensity = 0.45f; core.SetCRTEnabled(true); core.SetCRTScanlineIntensity(0.45f);
        p.grainEnabled = true; p.grainIntensity = 0.20f; core.SetGrainEnabled(true); core.SetGrainIntensity(0.20f);
        p.glitchEnabled = true; p.glitchIntensity = 0.25f; p.glitchMode = 2; core.SetGlitchEnabled(true); core.SetGlitchMode(2);
    });

    RenderPresetBtn("⚡ Cyberpunk", [&]() {
        p.colorGradingEnabled = true; p.colorGradingPreset = 2; p.colorGradingIntensity = 0.85f;
        core.SetColorGradingEnabled(true); core.SetColorGradingPreset(2); core.SetColorGradingIntensity(0.85f);
        p.glitchEnabled = true; p.glitchIntensity = 0.35f; p.glitchMode = 1;
        core.SetGlitchEnabled(true); core.SetGlitchIntensity(0.35f); core.SetGlitchMode(1);
        p.bloomEnabled = true; p.bloomIntensity = 0.45f; core.SetBloomEnabled(true); core.SetBloomIntensity(0.45f);
    });

    RenderPresetBtn("🌫 Humo & Niebla", [&]() {
        p.volumetricFogEnabled = true; p.volumetricFogDensity = 0.55f; p.volumetricFogColorMode = 0;
        core.SetVolumetricFogEnabled(true); core.SetVolumetricFogDensity(0.55f); core.SetVolumetricFogColorMode(0);
        p.vignetteEnabled = true; p.vignetteIntensity = 0.35f; core.SetVignetteEnabled(true);
    });

    RenderPresetBtn("⛅ Nubes & Rayos", [&]() {
        p.volumetricCloudsEnabled = true; p.volumetricCloudsCoverage = 0.60f; p.volumetricCloudsSunIntensity = 0.75f;
        core.SetVolumetricCloudsEnabled(true); core.SetVolumetricCloudsCoverage(0.60f); core.SetVolumetricCloudsSunIntensity(0.75f);
    });

    RenderPresetBtn("🔥 Calor en Suelo", [&]() {
        p.zonedDistortionEnabled = true; p.zonedDistortionZone = 0; p.zonedDistortionIntensity = 0.55f;
        core.SetZonedDistortionEnabled(true); core.SetZonedDistortionZone(0); core.SetZonedDistortionIntensity(0.55f);
    });

    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── 4. Pestañas de Categoría con Auto-Wrap (Fila 3) ────────────────────────
    const char* kCategories[] = {
        "Todos los Efectos",
        "✨ Calidad & Color",
        "🎬 Cine & Estilo",
        "📺 Retro & Distorsión",
        "🌊 Óptico & Creativo",
        "🌫 Volumétricos & Zonas"
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 4.0f));
    for (int catIdx = 0; catIdx < 6; ++catIdx) {
        float btnW = ImGui::CalcTextSize(kCategories[catIdx]).x + 20.0f;
        if (catIdx > 0) {
            if (ImGui::GetContentRegionAvail().x >= btnW + 4.0f) {
                ImGui::SameLine(0.0f, 4.0f);
            }
        }
        bool isSel = (m_SelectedCategory == catIdx);

        if (isSel) {
            ImGui::PushStyleColor(ImGuiCol_Button, DS::AccentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::AccentColorHov);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        }

        if (ImGui::Button(kCategories[catIdx])) {
            m_SelectedCategory = catIdx;
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // ── 5. Definición de Efectos y Filtro ─────────────────────────────────────
    struct Effect {
        const char* id;
        IconFn      icon;
        ImU32       accent;
        const char* title;
        const char* desc;
        int         category; // 1=Calidad, 2=Cine, 3=Retro, 4=Optico
        bool*       enabled;
        const char* sliderLabel;
        float*      sliderVal;
        float       sliderMin, sliderMax;
        std::function<void(bool)>  onToggle;
        std::function<void(float)> onSlide;

        int*        modeVal    = nullptr;
        const char* const* modeLabels = nullptr;
        int         modeCount  = 0;
        std::function<void(int)> onModeChange;

        const char* secSliderLabel = nullptr;
        float*      secSliderVal   = nullptr;
        float       secMin = 0.0f, secMax = 1.0f;
        std::function<void(float)> onSecSlide;
    };

    const bool hasNvidiaGpu = Core::SystemStats::Get().IsNvidiaGpu();

    static const char* kCineTintLabels[] = { "Rojo", "Verde", "Azul" };
    static const char* kGlitchModes[]    = { "Sutil", "Cyberpunk", "Cinta" };
    static const char* kGradingPresets[] = { "Cálido", "Teal&Or", "Neón", "Sepia", "Noir", "Matrix", "Pastel" };
    static const char* kPixelDepths[]    = { "Color Real", "16-Bit", "8-Bit Retro" };
    static const char* kMirrorModes[]    = { "Horizontal", "Vertical", "Caleido 4x", "Radial 8x" };
    static const char* kThermalModes[]   = { "Térmico", "Visión Nocturna", "Solarizado" };
    static const char* kHalftoneModes[]  = { "Pop-Art", "Monocromo", "Periódico" };
    static const char* kVolumetricFogColorModes[] = { "Humo Gris", "Místico Cian", "Fuego Cálido", "Cyber Neón" };
    static const char* kZonedDistortionZones[]    = { "Suelo / Calor", "Cielo / Atmósfera", "Centro Focal", "Lateral Izq", "Lateral Der" };

    std::vector<Effect> allEffects;

    // ── CATEGORÍA 1: Calidad & Color ──────────────────────────────────────────
    allEffects.push_back({
        "fsr", IconUpscale, IM_COL32(90, 170, 245, 255), "FSR 1.0",
        "Reescala y afila video de baja resolución mediante algoritmo AMD FSR de ultra calidad.",
        1, &p.fsrEnabled, "Nitidez", &p.fsrSharpness, 0.0f, 2.0f,
        [&](bool v){ core.SetFSREnabled(v); }, [&](float v){ core.SetFSRSharpness(v); }
    });
    if (hasNvidiaGpu) {
        allEffects.push_back({
            "nis", IconUpscale, IM_COL32(118, 185, 0, 255), "NIS (NVIDIA)",
            "Escalador NVIDIA Image Scaling de alta precisión exclusivo para GPUs GeForce.",
            1, &p.nisEnabled, "Nitidez", &p.nisSharpness, 0.0f, 1.0f,
            [&](bool v){ core.SetNISEnabled(v); }, [&](float v){ core.SetNISSharpness(v); }
        });
    }
    allEffects.push_back({
        "sharpen", IconSharpen, IM_COL32(235, 160, 90, 255), "Sharpen",
        "Realza micro-bordes y detalles de textura en texturas borrosas o videos.",
        1, &p.sharpenEnabled, "Intensidad", &p.sharpenIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetSharpenEnabled(v); }, [&](float v){ core.SetSharpenIntensity(v); }
    });
    allEffects.push_back({
        "fxaa", IconFXAA, IM_COL32(120, 190, 230, 255), "FXAA (Anti-aliasing)",
        "Suaviza bordes dentados y dientes de sierra en composiciones complejas.",
        1, &p.fxaaEnabled, nullptr, nullptr, 0.0f, 0.0f,
        [&](bool v){ core.SetFXAAEnabled(v); }, nullptr
    });
    allEffects.push_back({
        "taa", IconTAA, IM_COL32(120, 200, 235, 255), "TAA Temporal",
        "Mezcla temporal entre fotogramas consecutivos para suavizado cinemático.",
        1, &p.taaEnabled, "Intensidad", &p.taaIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetTAAEnabled(v); }, [&](float v){ core.SetTAAIntensity(v); }
    });
    allEffects.push_back({
        "saturation", IconSaturation, IM_COL32(235, 110, 165, 255), "Saturación",
        "Intensifica o desatura la viveza de los colores hasta blanco y negro.",
        1, &p.saturationEnabled, "Cantidad", &p.saturationAmount, 0.0f, 2.0f,
        [&](bool v){ core.SetSaturationEnabled(v); }, [&](float v){ core.SetSaturationAmount(v); }
    });
    allEffects.push_back({
        "contrast", IconContrast, IM_COL32(150, 150, 160, 255), "Contraste",
        "Mayor rango y separación dinámica entre sombras profundas y luces.",
        1, &p.contrastEnabled, "Cantidad", &p.contrastAmount, 0.0f, 2.0f,
        [&](bool v){ core.SetContrastEnabled(v); }, [&](float v){ core.SetContrastAmount(v); }
    });
    allEffects.push_back({
        "luminosity", IconLuminosity, IM_COL32(255, 230, 140, 255), "Luminosidad",
        "Control maestro de brillo general de la señal de proyección.",
        1, &p.luminosityEnabled, "Cantidad", &p.luminosityAmount, 0.0f, 2.0f,
        [&](bool v){ core.SetLuminosityEnabled(v); }, [&](float v){ core.SetLuminosityAmount(v); }
    });
    allEffects.push_back({
        "fillblur", IconFill, IM_COL32(90, 210, 190, 255), "Rellenado de Barras",
        "Rellena barras negras con el mismo fondo estirado y desenfocado con iluminación ambiental.",
        1, &p.fillBlurEnabled, "Brillo", &p.fillBlurBrightness, 0.0f, 1.0f,
        [&](bool v){ core.SetFillBlurEnabled(v); }, [&](float v){ core.SetFillBlurBrightness(v); }
    });

    // ── CATEGORÍA 2: Cine & Estilo ────────────────────────────────────────────
    allEffects.push_back({
        "colorgrading", IconColorGrading, IM_COL32(245, 130, 80, 255), "Color Grading LUTs",
        "Gradación de color cinematográfica profesional: Cálido, Teal&Orange, Neón, Sepia, Noir, Matrix y Pastel.",
        2, &p.colorGradingEnabled, "Intensidad", &p.colorGradingIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetColorGradingEnabled(v); }, [&](float v){ core.SetColorGradingIntensity(v); },
        &p.colorGradingPreset, kGradingPresets, 7,
        [&](int m){ core.SetColorGradingPreset(m); }
    });
    allEffects.push_back({
        "cine", IconCine, IM_COL32(235, 200, 90, 255), "Cine Filmico",
        "Curva de respuesta fílmica con tinte cromático direccional.",
        2, &p.cineEnabled, "Intensidad", &p.cineIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetCineEnabled(v); }, [&](float v){ core.SetCineIntensity(v); },
        &p.cineTint, kCineTintLabels, 3,
        [&](int m){ core.SetCineTint(m); }
    });
    allEffects.push_back({
        "bloom", IconBloom, IM_COL32(255, 220, 120, 255), "Bloom Resplandor",
        "Derramamiento de luz etérea y resplandor desde las zonas más iluminadas.",
        2, &p.bloomEnabled, "Intensidad", &p.bloomIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetBloomEnabled(v); }, [&](float v){ core.SetBloomIntensity(v); }
    });
    allEffects.push_back({
        "vignette", IconVignette, IM_COL32(180, 140, 235, 255), "Viñetado",
        "Oscurece suavemente los bordes para concentrar la atención en el centro de la pantalla.",
        2, &p.vignetteEnabled, "Intensidad", &p.vignetteIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetVignetteEnabled(v); }, [&](float v){ core.SetVignetteIntensity(v); }
    });
    allEffects.push_back({
        "chromaticaberration", IconChromaticAberration, IM_COL32(235, 100, 200, 255), "Aberración Cromática",
        "Desfase prismático RGB en los extremos de la lente para estilo óptico de cine.",
        2, &p.chromaticAberrationEnabled, "Intensidad", &p.chromaticAberrationIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetChromaticAberrationEnabled(v); }, [&](float v){ core.SetChromaticAberrationIntensity(v); }
    });
    allEffects.push_back({
        "radialblur", IconRadialBlur, IM_COL32(245, 90, 120, 255), "Radial Zoom Blur",
        "Desenfoque radial cinematográfico de alta energía con dirección concéntrica.",
        2, &p.radialBlurEnabled, "Intensidad", &p.radialBlurIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetRadialBlurEnabled(v); }, [&](float v){ core.SetRadialBlurIntensity(v); }
    });

    // ── CATEGORÍA 3: Retro & Distorsión ───────────────────────────────────────
    allEffects.push_back({
        "glitch", IconGlitch, IM_COL32(245, 80, 150, 255), "Glitch Digital",
        "Desplazamiento horizontal por bloques, salto de sincronía y split RGB.",
        3, &p.glitchEnabled, "Intensidad", &p.glitchIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetGlitchEnabled(v); }, [&](float v){ core.SetGlitchIntensity(v); },
        &p.glitchMode, kGlitchModes, 3,
        [&](int m){ core.SetGlitchMode(m); },
        "Velocidad", &p.glitchSpeed, 0.1f, 3.0f,
        [&](float v){ core.SetGlitchSpeed(v); }
    });
    allEffects.push_back({
        "vhs", IconVHS, IM_COL32(200, 90, 220, 255), "VHS Cinta Retro",
        "Emulación analógica de cinta de video: sangrado de color, tracking y bamboleo.",
        3, &p.vhsEnabled, "Intensidad", &p.vhsIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetVHSEnabled(v); }, [&](float v){ core.SetVHSIntensity(v); }
    });
    allEffects.push_back({
        "crt", IconCRT, IM_COL32(120, 220, 150, 255), "Modo CRT TV",
        "Líneas de escaneo (scanlines) y curvatura de tubo retro estilo televisión.",
        3, &p.crtEnabled, "Scanlines", &p.crtScanlineIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetCRTEnabled(v); }, [&](float v){ core.SetCRTScanlineIntensity(v); }
    });
    allEffects.push_back({
        "pixelate", IconPixelate, IM_COL32(90, 220, 245, 255), "Pixel Art Retro",
        "Rasterización retro con tamaño de píxel ajustable y cuantización de color 8/16-bit.",
        3, &p.pixelateEnabled, "Tamaño de Píxel", &p.pixelateSize, 2.0f, 48.0f,
        [&](bool v){ core.SetPixelateEnabled(v); }, [&](float v){ core.SetPixelateSize(v); },
        &p.pixelateColorDepth, kPixelDepths, 3,
        [&](int m){ core.SetPixelateColorDepth(m); }
    });
    allEffects.push_back({
        "grain", IconGrain, IM_COL32(230, 190, 90, 255), "Grano de Película",
        "Ruido dinámico fino animado tipo celuloide fotográfico.",
        3, &p.grainEnabled, "Intensidad", &p.grainIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetGrainEnabled(v); }, [&](float v){ core.SetGrainIntensity(v); }
    });
    allEffects.push_back({
        "halftone", IconHalftone, IM_COL32(245, 170, 70, 255), "Semitono Pop-Art",
        "Trama de puntos serigráficos estilo cómic clásico y prensa retro.",
        3, &p.halftoneEnabled, "Escala de Punto", &p.halftoneDotScale, 4.0f, 30.0f,
        [&](bool v){ core.SetHalftoneEnabled(v); }, [&](float v){ core.SetHalftoneDotScale(v); },
        &p.halftoneMode, kHalftoneModes, 3,
        [&](int m){ core.SetHalftoneMode(m); }
    });

    // ── CATEGORÍA 4: Óptico & Creativo ────────────────────────────────────────
    allEffects.push_back({
        "waves", IconWaves, IM_COL32(70, 210, 245, 255), "Ondas Acuáticas",
        "Distorsión sinusoidal armónica fluida tipo superficie de agua u ondas de calor.",
        4, &p.wavesEnabled, "Intensidad", &p.wavesIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetWavesEnabled(v); }, [&](float v){ core.SetWavesIntensity(v); },
        nullptr, nullptr, 0, nullptr,
        "Frecuencia", &p.wavesFrequency, 2.0f, 20.0f,
        [&](float v){ core.SetWavesFrequency(v); }
    });
    allEffects.push_back({
        "mirror", IconMirror, IM_COL32(160, 120, 245, 255), "Espejo & Caleidoscopio",
        "Simetría visual múltiple: Horizontal, Vertical, Cuadrante 4x o Caleidoscopio 8x.",
        4, &p.mirrorEnabled, nullptr, nullptr, 0.0f, 0.0f,
        [&](bool v){ core.SetMirrorEnabled(v); }, nullptr,
        &p.mirrorMode, kMirrorModes, 4,
        [&](int m){ core.SetMirrorMode(m); }
    });
    allEffects.push_back({
        "thermal", IconThermal, IM_COL32(245, 80, 80, 255), "Térmico & Visión Nocturna",
        "Mapeo de calor infrarrojo, visión nocturna verde fósforo o inversión solarizada.",
        4, &p.thermalEnabled, "Intensidad", &p.thermalIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetThermalEnabled(v); }, [&](float v){ core.SetThermalIntensity(v); },
        &p.thermalMode, kThermalModes, 3,
        [&](int m){ core.SetThermalMode(m); }
    });
    allEffects.push_back({
        "blur", IconBlur, IM_COL32(150, 170, 235, 255), "Blur Gaussiano",
        "Desenfoque suave de toda la imagen para fondos de ambiente.",
        4, &p.blurEnabled, "Intensidad", &p.blurIntensity, 0.0f, 1.0f,
        [&](bool v){ core.SetBlurEnabled(v); }, [&](float v){ core.SetBlurIntensity(v); }
    });

    // ── CATEGORÍA 5: Volumétricos & Zonas ────────────────────────────────────
    allEffects.push_back({
        "volumetricfog", IconVolumetricFog, IM_COL32(170, 210, 245, 255), "Humo & Niebla Volumétrica",
        "Generación procedural 3D de volutas de humo, niebla densa y calima con dinámica de fluidos por GPU.",
        5, &p.volumetricFogEnabled, "Densidad", &p.volumetricFogDensity, 0.05f, 1.0f,
        [&](bool v){ core.SetVolumetricFogEnabled(v); }, [&](float v){ core.SetVolumetricFogDensity(v); },
        &p.volumetricFogColorMode, kVolumetricFogColorModes, 4,
        [&](int m){ core.SetVolumetricFogColorMode(m); },
        "Velocidad Convección", &p.volumetricFogSpeed, 0.1f, 3.0f,
        [&](float v){ core.SetVolumetricFogSpeed(v); }
    });
    allEffects.push_back({
        "volumetricclouds", IconVolumetricClouds, IM_COL32(245, 210, 120, 255), "Nubes & Rayos Volumétricos",
        "Manto de nubes volumétricas raymarched en movimiento con iluminación solar y resplandor de bordes (Silver Lining).",
        5, &p.volumetricCloudsEnabled, "Cobertura Nubosa", &p.volumetricCloudsCoverage, 0.1f, 1.0f,
        [&](bool v){ core.SetVolumetricCloudsEnabled(v); }, [&](float v){ core.SetVolumetricCloudsCoverage(v); },
        nullptr, nullptr, 0, nullptr,
        "Resplandor Solar", &p.volumetricCloudsSunIntensity, 0.0f, 1.5f,
        [&](float v){ core.SetVolumetricCloudsSunIntensity(v); }
    });
    allEffects.push_back({
        "zoneddistortion", IconZonedDistortion, IM_COL32(245, 120, 70, 255), "Distorsión / Calor por Zonas",
        "Ondas de calor, turbulencia o refracción concentradas en una zona específica (Suelo, Cielo, Centro, Laterales).",
        5, &p.zonedDistortionEnabled, "Intensidad", &p.zonedDistortionIntensity, 0.05f, 1.0f,
        [&](bool v){ core.SetZonedDistortionEnabled(v); }, [&](float v){ core.SetZonedDistortionIntensity(v); },
        &p.zonedDistortionZone, kZonedDistortionZones, 5,
        [&](int m){ core.SetZonedDistortionZone(m); },
        "Difuminado Borde", &p.zonedDistortionFeather, 0.05f, 0.6f,
        [&](float v){ core.SetZonedDistortionFeather(v); }
    });

    // ── 6. Filtrado por Categoría y Búsqueda ──────────────────────────────────
    std::string searchLower = m_SearchFilter;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    std::vector<Effect> filteredEffects;
    int activeCount = 0;

    for (const auto& e : allEffects) {
        if (*e.enabled) activeCount++;

        // Filtro por categoría
        if (m_SelectedCategory > 0 && e.category != m_SelectedCategory) continue;

        // Filtro por búsqueda
        if (!searchLower.empty()) {
            std::string tLower = e.title;
            std::string dLower = e.desc;
            std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
            std::transform(dLower.begin(), dLower.end(), dLower.begin(), ::tolower);
            if (tLower.find(searchLower) == std::string::npos && dLower.find(searchLower) == std::string::npos)
                continue;
        }

        filteredEffects.push_back(e);
    }

    // Badge contador de efectos activos
    if (activeCount > 0) {
        ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.55f, 1.0f), "● %d efecto%s activo%s en la salida en vivo",
            activeCount, activeCount > 1 ? "s" : "", activeCount > 1 ? "s" : "");
    } else {
        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 0.8f), "○ Ningún efecto activo (imagen pura)");
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // ── 7. Grilla de Tarjetas Responsiva ──────────────────────────────────────
    const float gap       = 12.0f;
    const float baseCardW = 230.0f * m_ThumbZoom;
    const int   cols      = std::max(1, (int)((availW + gap) / (baseCardW + gap)));
    const float cardW     = (availW - gap * (float)(cols - 1)) / (float)cols;

    const int   count    = (int)filteredEffects.size();
    const float originX  = ImGui::GetCursorScreenPos().x;
    float       cursorY  = ImGui::GetCursorScreenPos().y;

    for (int i = 0; i < count; i += cols) {
        float rowH = 0.0f;
        for (int c = 0; c < cols && i + c < count; c++) {
            Effect& e = filteredEffects[i + c];
            rowH = std::max(rowH, ComputeCardHeight(e.sliderLabel != nullptr, e.secSliderLabel != nullptr, *e.enabled, e.modeVal != nullptr, e.modeCount));
        }

        for (int c = 0; c < cols && i + c < count; c++) {
            Effect& e = filteredEffects[i + c];
            ImVec2  origin(originX + (float)c * (cardW + gap), cursorY);
            if (ShaderCard(origin, e.id, e.icon, e.accent, e.title, e.desc,
                           e.enabled, e.sliderLabel, e.sliderVal, e.sliderMin, e.sliderMax, cardW,
                           e.modeVal, e.modeLabels, e.modeCount,
                           e.secSliderLabel, e.secSliderVal, e.secMin, e.secMax)) {
                e.onToggle(*e.enabled);
                if (e.onSlide && e.sliderVal) e.onSlide(*e.sliderVal);
                if (e.onSecSlide && e.secSliderVal) e.onSecSlide(*e.secSliderVal);
                if (e.onModeChange && e.modeVal) e.onModeChange(*e.modeVal);
                changed = true;
            }
        }

        cursorY += rowH + gap;
    }

    if (filteredEffects.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);
        ImGui::SetCursorPosX(availW * 0.5f - 120.0f);
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.7f), "No se encontraron efectos con ese filtro.");
    }

    ImGui::SetCursorScreenPos(ImVec2(originX, cursorY));
    ImGui::Dummy(ImVec2(availW, 10.0f));

    if (changed) settingsMgr.Save();
}

} // namespace ProyecThor::UI

