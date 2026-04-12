#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

    void SettingsPanel::RenderCategoryTheme() {
        auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
        bool changed = false;

        static ImGuiColorEditFlags flags =
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf |
            ImGuiColorEditFlags_NoInputs;

        ImGui::TextDisabled("Los cambios se aplican al guardar.");
        ImGui::Spacing();

        SectionTitle("Fondos y Superficies");
        changed |= ImGui::ColorEdit4("Fondo Principal##base",  theme.base,    flags); ImGui::SameLine(0,8); HelpTooltip("Color de ventanas principales.");
        changed |= ImGui::ColorEdit4("Superficie 0##s0",       theme.surface0,flags);
        changed |= ImGui::ColorEdit4("Superficie 1##s1",       theme.surface1,flags);
        changed |= ImGui::ColorEdit4("Superficie 2##s2",       theme.surface2,flags);
        changed |= ImGui::ColorEdit4("Superficie 3##s3",       theme.surface3,flags);

        SectionTitle("Color de Acento");
        changed |= ImGui::ColorEdit4("Acento Principal##ac",   theme.accent,      flags);
        changed |= ImGui::ColorEdit4("Acento Claro##acl",      theme.accentLight, flags);
        changed |= ImGui::ColorEdit4("Acento Oscuro##acd",     theme.accentDim,   flags);
        changed |= ImGui::ColorEdit4("Acento Tenue##acf",      theme.accentFaint, flags);

        SectionTitle("Bordes");
        changed |= ImGui::ColorEdit4("Borde##bd",              theme.border,      flags);
        changed |= ImGui::ColorEdit4("Borde Tenue##bdf",       theme.borderFaint, flags);

        SectionTitle("Texto");
        changed |= ImGui::ColorEdit4("Texto Principal##tp",    theme.textPrimary, flags);
        changed |= ImGui::ColorEdit4("Texto Secundario##td",   theme.textDim,     flags);
        changed |= ImGui::ColorEdit4("Texto Inactivo##tf",     theme.textFaint,   flags);

        ImGui::Spacing();
        // Preview inmediato opcional
        if (changed) {
            // Aplicar en tiempo real sin guardar a disco (solo para preview)
            ProyecThor::Settings::SettingsManager::Get().ApplyTheme();
        }
    }

} // namespace ProyecThor::UI::Settings
