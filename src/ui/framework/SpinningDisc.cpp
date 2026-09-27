#include "SpinningDisc.h"
#include <cmath>
#include <algorithm>

namespace ProyecThor::UI {

void UpdateSpinningDisc(SpinningDiscState& state, float targetSpeed, float dt) {
    state.currentSpeed += (targetSpeed - state.currentSpeed) * std::min(1.0f, dt * 4.0f);
    state.rotationAngle += state.currentSpeed * dt;
    constexpr float kTwoPi = 6.28318530718f;
    if (state.rotationAngle > kTwoPi) state.rotationAngle -= kTwoPi;
}

void DrawSpinningDisc(ImDrawList* dl, ImVec2 center, float radius,
                     const SpinningDiscState& state, ImTextureID albumArt)
{
    // Base del vinilo
    dl->AddCircleFilled(center, radius, IM_COL32(18, 18, 22, 255), 72);
    dl->AddCircle(center, radius, IM_COL32(60, 62, 72, 255), 72, 1.5f);

    // Surcos concentricos
    for (int i = 1; i <= 4; i++) {
        float r = radius * (0.55f + i * 0.09f);
        if (r >= radius) break;
        dl->AddCircle(center, r, IM_COL32(40, 41, 48, 200), 64, 1.0f);
    }

    // Brillo rotando (vende el giro sin necesitar textura animada)
    {
        float a0 = state.rotationAngle;
        float a1 = a0 + 0.35f;
        ImU32 shineCol = IM_COL32(255, 255, 255, 18);
        dl->PathArcTo(center, radius * 0.98f, a0, a1, 24);
        dl->PathStroke(shineCol, ImDrawFlags_None, radius * 0.5f);
    }

    // Centro: portada o glifo generico
    float labelR = radius * 0.34f;
    if (albumArt) {
        dl->AddCircleFilled(center, labelR + 2.0f, IM_COL32(12, 12, 14, 255), 48);
        ImVec2 pMin(center.x - labelR, center.y - labelR);
        ImVec2 pMax(center.x + labelR, center.y + labelR);
        dl->AddImageRounded(albumArt, pMin, pMax, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, labelR);
    } else {
        dl->AddCircleFilled(center, labelR, IM_COL32(45, 60, 90, 255), 48);
        // Glifo de nota simple, centrado
        float sz = labelR * 1.1f;
        ImVec2 o(center.x - sz * 0.5f, center.y - sz * 0.5f);
        ImU32  noteCol = IM_COL32(220, 226, 240, 230);
        dl->AddEllipseFilled({o.x + sz*0.28f, o.y + sz*0.72f}, ImVec2(sz*0.16f, sz*0.11f), noteCol, 0.f, 12);
        dl->AddLine({o.x + sz*0.43f, o.y + sz*0.72f}, {o.x + sz*0.43f, o.y + sz*0.20f}, noteCol, sz*0.07f);
        dl->AddBezierCubic(
            {o.x + sz*0.43f, o.y + sz*0.20f}, {o.x + sz*0.80f, o.y + sz*0.20f},
            {o.x + sz*0.80f, o.y + sz*0.44f}, {o.x + sz*0.43f, o.y + sz*0.48f},
            noteCol, sz*0.065f, 8);
    }

    // Agujero central
    dl->AddCircleFilled(center, radius * 0.045f, IM_COL32(15, 15, 18, 255), 20);
}

} // namespace ProyecThor::UI
