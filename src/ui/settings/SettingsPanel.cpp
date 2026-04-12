#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

    struct Category {
        const char* icon;
        const char* label;
    };

    static const Category k_Categories[] = {
        { "🎨", "Apariencia"      },
        { "⚙️", "General"         },
        { "📽️", "Proyección"      },
        { "🔊", "Audio"           },
        { "🌐", "Idioma"          },
        { "🔄", "Actualizaciones" },
    };
    static constexpr int k_CategoryCount = 6;

    // ─────────────────────────────────────────────────────────────────────────
    SettingsPanel::SettingsPanel() {}

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::Render(bool* isOpen) {
        if (!*isOpen) return;

        ImGui::SetNextWindowSize(ImVec2(780, 560), ImGuiCond_FirstUseEver);
        // En lugar de SetNextWindowMinSize, usamos restricciones:
ImGui::SetNextWindowSizeConstraints(ImVec2(600, 420), ImVec2(FLT_MAX, FLT_MAX));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.060f, 0.065f, 0.088f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        bool open = ImGui::Begin("Preferencias — ProyecThor", isOpen,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        ImGui::PopStyleVar();

        if (open) {
            // Layout: sidebar (180px) | content (resto) — todo dentro del área sin padding
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float sideW  = 180.0f;
            float contentW = avail.x - sideW;
            float contentH = avail.y - 44.0f; // dejar espacio para la barra inferior

            // ── Sidebar ──
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.048f, 0.052f, 0.072f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
            ImGui::BeginChild("##sidebar", ImVec2(sideW, contentH), false);
            ImGui::PopStyleVar();
            RenderSidebar();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 0);

            // Thin divider
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + contentH),
                IM_COL32(50, 54, 72, 255), 1.0f);

            // ── Content ──
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.065f, 0.070f, 0.094f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 16));
            ImGui::BeginChild("##content", ImVec2(contentW, contentH), false);
            ImGui::PopStyleVar();
            RenderContent();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // ── Save bar ──
            RenderSaveBar();
        }

        ImGui::End();
        ImGui::PopStyleColor(); // WindowBg

        // Auto-save timer feedback
        if (m_SaveTimer > 0.0f) {
            m_SaveTimer -= ImGui::GetIO().DeltaTime;
            if (m_SaveTimer <= 0.0f) m_SaveStatusMsg = "";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::RenderSidebar() {
        auto& mgr = ProyecThor::Settings::SettingsManager::Get();

        // App version badge at top
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.38f, 0.16f, 1.0f));
        ImGui::SetCursorPosX(12);
        ImGui::Text("v%s", mgr.GetSettings().updates.currentVersion.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

        for (int i = 0; i < k_CategoryCount; i++) {
            bool selected = (m_SelectedCategory == i);

            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.24f, 0.20f, 0.085f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.25f, 0.10f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.12f, 0.13f, 0.175f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.62f, 0.62f, 0.60f, 1.0f));
            }

            // Gold left bar for selected item
            if (selected) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cp = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + 3, cp.y + 32),
                    IM_COL32(200, 168, 76, 255));
            }

            ImGui::SetCursorPosX(selected ? 8.0f : 6.0f);
            char label[64];
            snprintf(label, sizeof(label), "  %s  %s##cat%d",
                k_Categories[i].icon, k_Categories[i].label, i);

            if (ImGui::Selectable(label, selected, 0, ImVec2(0, 32)))
                m_SelectedCategory = i;

            ImGui::PopStyleColor(3);
        }

        ImGui::PopStyleVar(2);
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::RenderContent() {
        // Category title
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
        ImGui::SetWindowFontScale(1.15f);
        ImGui::Text("%s  %s", k_Categories[m_SelectedCategory].icon,
                               k_Categories[m_SelectedCategory].label);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.21f, 0.09f, 0.70f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        switch (m_SelectedCategory) {
            case 0: RenderCategoryTheme();      break;
            case 1: RenderCategoryGeneral();    break;
            case 2: RenderCategoryProjection(); break;
            case 3: RenderCategoryAudio();      break;
            case 4: RenderCategoryLanguage();   break;
            case 5: RenderCategoryUpdates();    break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::RenderSaveBar() {
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // Background bar
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float barY = winPos.y + winSize.y - 44.0f;
        dl->AddRectFilled(
            ImVec2(winPos.x, barY),
            ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            IM_COL32(30, 32, 44, 255)
        );
        dl->AddLine(ImVec2(winPos.x, barY), ImVec2(winPos.x + winSize.x, barY),
            IM_COL32(50, 54, 72, 255), 1.0f);

        ImGui::SetCursorPos(ImVec2(winSize.x - 240.0f, winSize.y - 36.0f));

        // Reset button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("Restablecer", ImVec2(105, 28))) {
            ProyecThor::Settings::SettingsManager::Get().ResetToDefaults();
            ProyecThor::Settings::SettingsManager::Get().ApplyTheme();
            ProyecThor::Settings::SettingsManager::Get().ApplyProjection();
            m_SaveStatusMsg = "Valores restablecidos";
            m_SaveTimer     = 3.0f;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 8);

        // Save button — gold
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.240f, 0.200f, 0.085f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.415f, 0.180f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.650f, 0.530f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.970f, 0.870f, 0.600f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("  Guardar  ", ImVec2(105, 28))) {
            auto& mgr = ProyecThor::Settings::SettingsManager::Get();
            mgr.ApplyTheme();
            mgr.ApplyProjection();
            mgr.Save();
            m_SaveStatusMsg = "✓  Guardado";
            m_SaveTimer     = 3.0f;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        // Status message
        if (!m_SaveStatusMsg.empty()) {
            ImGui::SameLine(0, 12);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.88f, 0.55f, m_SaveTimer));
            ImGui::Text("%s", m_SaveStatusMsg.c_str());
            ImGui::PopStyleColor();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers compartidos
    // ─────────────────────────────────────────────────────────────────────────
    void SettingsPanel::SectionTitle(const char* label) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.65f, 0.35f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.19f, 0.08f, 0.60f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    void SettingsPanel::HelpTooltip(const char* desc) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.43f, 1.0f));
        ImGui::TextDisabled("(?)");
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(300.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
// ─────────────────────────────────────────────────────────────────────────
void SettingsPanel::InitializeTheme() {
    // Aquí puedes llamar a la lógica inicial para aplicar el tema guardado
    // Por ejemplo, cargar el estilo visual de ImGui la primera vez:
    ProyecThor::Settings::SettingsManager::Get().ApplyTheme();
}
} // namespace ProyecThor::UI::Settings
