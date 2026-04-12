#pragma once
#include <imgui.h>
#include <string>
#include <vector>

namespace ProyecThor::UI::Settings {

    // ─────────────────────────────────────────────────────────────────────────
    // SettingsPanel
    //
    // Ventana de Preferencias con panel lateral de categorías.
    // Cada categoría tiene su propio .cpp:
    //   CategoryTheme.cpp        — Apariencia
    //   CategoryGeneral.cpp      — General
    //   CategoryProjection.cpp   — Proyección
    //   CategoryAudio.cpp        — Audio
    //   CategoryLanguage.cpp     — Idioma
    //   CategoryUpdates.cpp      — Actualizaciones
    // ─────────────────────────────────────────────────────────────────────────
    class SettingsPanel {
    public:
        SettingsPanel();
        void Render(bool* isOpen);
        void InitializeTheme();
    private:
        int m_SelectedCategory = 0;

        // Banderas para feedback de guardado
        bool        m_SavePending   = false;
        float       m_SaveTimer     = 0.0f;
        std::string m_SaveStatusMsg = "";

        void RenderSidebar();
        void RenderContent();
        void RenderSaveBar();    // barra inferior con botón Guardar

        // Una función por categoría — cada una vive en su propio .cpp
        void RenderCategoryTheme();
        void RenderCategoryGeneral();
        void RenderCategoryProjection();
        void RenderCategoryAudio();
        void RenderCategoryLanguage();
        void RenderCategoryUpdates();

        // Helpers compartidos
        void SectionTitle(const char* label);
        void HelpTooltip(const char* desc);
    };

} // namespace ProyecThor::UI::Settings
