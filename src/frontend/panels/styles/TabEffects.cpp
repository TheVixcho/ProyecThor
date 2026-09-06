#include "TabEffects.h"
#include "DesignSystem.h"
#include <imgui.h>

namespace ProyecThor::UI {

void TabEffects::RenderEffectCard(const char* title, const ImVec4& accent, float colWidth,
                                   bool& enabled, float* color4,
                                   float* intensity, const char* intensityLabel,
                                   float* color4B, float* angleDegrees, const char* angleLabel)
{
    ImGui::PushID(title);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float  cardH = 40.0f + (enabled ? (
        (color4 ? 34.0f : 0.0f) + (color4B ? 34.0f : 0.0f) +
        (intensity ? 34.0f : 0.0f) + (angleDegrees ? 34.0f : 0.0f)
    ) : 0.0f);
    ImVec2 p1 = { p0.x + colWidth, p0.y + cardH };

    ImU32 bg = enabled
        ? CanvaPalette::ToU32(ImVec4(accent.x, accent.y, accent.z, 0.12f))
        : CanvaPalette::ToU32(CanvaPalette::Surface1);
    ImU32 border = enabled ? CanvaPalette::ToU32(accent) : CanvaPalette::ToU32(CanvaPalette::Border);

    dl->AddRectFilled(p0, p1, bg, 5.0f);
    dl->AddRect(p0, p1, border, 5.0f, 0, enabled ? 1.4f : 1.0f);

    ImGui::SetCursorScreenPos({ p0.x + 12.0f, p0.y + 10.0f });
    ImGui::PushStyleColor(ImGuiCol_Text, enabled ? accent : CanvaPalette::TextMuted);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos({ p1.x - 34.0f, p0.y + 8.0f });
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::Checkbox("##enabled", &enabled);
    ImGui::PopStyleColor(3);

    float y = p0.y + 36.0f;

    if (enabled && color4) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::SetNextItemWidth(colWidth - 24.0f);
        ImGui::ColorEdit4("##color", color4,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        y += 34.0f;
    }

    if (enabled && color4B) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::SetNextItemWidth(colWidth - 24.0f);
        ImGui::ColorEdit4("##colorB", color4B,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        y += 34.0f;
    }

    if (enabled && intensity) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextUnformatted(intensityLabel);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y + 16.0f });
        DS::ModernSlider("##intensity", intensity, 0.0f, 1.0f, colWidth - 24.0f, CanvaPalette::ToU32(accent));
        y += 34.0f;
    }

    if (enabled && angleDegrees) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextUnformatted(angleLabel);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y + 16.0f });
        ImGui::SetNextItemWidth(colWidth - 24.0f);
        ImGui::DragFloat("##angle", angleDegrees, 1.0f, -180.0f, 180.0f, "%.0f°");
    }

    // Resetear cursor a p0 antes del Dummy final: los widgets de arriba se
    // posicionaron con SetCursorScreenPos, asi que el auto-layout quedo desalineado.
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy({ colWidth, cardH + 8.0f });
    ImGui::PopID();
}

void TabEffects::Render(Core::TextEffectsData& fx, float colWidth) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("EFECTOS DE TEXTO", CanvaPalette::Accent);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    CanvaStyleEditor::SectionLabel("Capas dibujadas sobre las letras, de atras hacia adelante.");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    RenderEffectCard("Fondo", CanvaPalette::Accent, colWidth,
                      fx.bgEnabled, fx.bgColor, nullptr, nullptr);

    RenderEffectCard("Borde", CanvaPalette::Accent, colWidth,
                      fx.borderEnabled, fx.borderColor, &fx.borderWidth, "Grosor");

    RenderEffectCard("Sombra", CanvaPalette::Accent, colWidth,
                      fx.shadowEnabled, fx.shadowColor, &fx.shadowIntensity, "Distancia");

    RenderEffectCard("Aberración cromática", CanvaPalette::Accent, colWidth,
                      fx.chromaticAberrationEnabled, nullptr,
                      &fx.chromaticAberrationIntensity, "Intensidad");

    RenderEffectCard("Glow (bloom)", CanvaPalette::Accent, colWidth,
                      fx.glowEnabled, fx.glowColor, &fx.glowIntensity, "Intensidad");

    RenderEffectCard("Neon", CanvaPalette::Accent, colWidth,
                      fx.neonEnabled, fx.neonColor, &fx.neonIntensity, "Intensidad");

    RenderEffectCard("Subrayado", CanvaPalette::Accent, colWidth,
                      fx.underlineEnabled, fx.underlineColor, &fx.underlineThickness, "Grosor");

    RenderEffectCard("Texto 3D", CanvaPalette::Accent, colWidth,
                      fx.text3dEnabled, fx.text3dColor, &fx.text3dDepth, "Profundidad");

    RenderEffectCard("Degradado de color", CanvaPalette::Pink, colWidth,
                      fx.gradientEnabled, fx.gradientColorA, nullptr, nullptr,
                      fx.gradientColorB, &fx.gradientAngle, "Angulo");

    RenderEffectCard("Transparencia con angulo", CanvaPalette::Accent, colWidth,
                      fx.opacityGradientEnabled, nullptr,
                      &fx.opacityGradientStrength, "Fuerza",
                      nullptr, &fx.opacityGradientAngle, "Angulo");
}

} // namespace ProyecThor::UI
