#pragma once
#include "IPanel.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

    class LayersPanel : public IPanel {
    public:
        LayersPanel();
        ~LayersPanel() override = default;

        void Render() override;
        std::string GetName() const override { return "Inspector de Capas y Fondos"; }

    private:
        // --- ESTADO ACTUAL DEL TEXTO ---
        int m_VAlignment = 1; 
        float m_TextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float m_TextSize = 80.0f;
        int m_TextAlignment = 1; 
        float m_Margins[4] = { 100.0f, 50.0f, 100.0f, 50.0f }; 
        bool m_AutoScale = true;

        // --- SISTEMAS DE ARCHIVOS ---
        std::vector<std::string> m_AvailableThemes;
        std::vector<std::string> m_AvailableBackgrounds; // NUEVO: Lista de videos
        std::string m_SelectedTheme = "";
        
        // --- EDITOR VISUAL DE TEMAS (SOLO TEXTO) ---
        bool m_ShowThemeEditor = false;
        char m_EditThemeName[64] = "";
        bool m_IsEditingExisting = false; 
        
        // Variables temporales para el Editor
        int m_TempVAlignment = 1;
        float m_TempTextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float m_TempTextSize = 80.0f;
        int m_TempTextAlignment = 1;
        float m_TempMargins[4] = { 100.0f, 50.0f, 100.0f, 50.0f };
        bool m_TempAutoScale = true;

        // Funciones internas
        void LoadThemeList();
        void LoadBackgroundsList(); // NUEVO: Carga los videos
        
        void SaveTheme(const std::string& name, bool isFromEditor = false);
        void ApplyTheme(const std::string& name);
        void DeleteTheme(const std::string& name);
        void LoadThemeToEditor(const std::string& name);
        
        // Renderizadores de UI
        void RenderBackgroundsGrid(); // NUEVO: Dibuja los videos
        void RenderThemeGrid();
        void RenderThemeEditorModal();
    };

} // namespace ProyecThor::UI