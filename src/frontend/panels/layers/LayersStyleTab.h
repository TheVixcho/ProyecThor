#pragma once
#include "../styles/CanvaStyleEditor.h"
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;

// ─────────────────────────────────────────────────────────────────────────────
//  LayersStyleTab — toda la lógica del tab "Estilos de Letra"
// ─────────────────────────────────────────────────────────────────────────────
class LayersStyleTab {
public:
    LayersStyleTab();
    ~LayersStyleTab() = default;

    // El editor de estilos se abre a pantalla completa (ver
    // UIManager::EnterFullscreenEditor) -- sin esto, el botón "Nuevo estilo"/
    // "Editar" simplemente no hace nada.
    void SetUIManager(UIManager* uiManager) { m_UIManager = uiManager; }

    void Render();

    // Acceso externo para sincronizar lista de fuentes tras importar
    void ReloadFonts();

    // Permite al panel principal obtener el estado de estilo activo
    const StyleData& GetCurrentStyle() const { return m_CurrentStyle; }

private:
    // ── Datos ─────────────────────────────────────────────────────────────────
    StyleData                m_CurrentStyle;
    std::string              m_SelectedTheme;
    std::vector<std::string> m_AvailableThemes;
    std::vector<std::string> m_AvailableFonts;

    bool  m_GridMode  = true;
    float m_ThumbZoom = 1.0f; // 0.65 .. 1.8 — tamano de las tarjetas de tema

    std::unique_ptr<CanvaStyleEditor> m_StyleEditor;
    UIManager* m_UIManager = nullptr;

    // Abre m_StyleEditor (nuevo o existente) a pantalla completa -- ver
    // UIManager::EnterFullscreenEditor.
    void OpenStyleEditorFullscreen(bool isNew, const std::string& name, const StyleData& data);

    // ── Render helpers ────────────────────────────────────────────────────────
    // Riel angosto a la izquierda (icon-only, apilado vertical): grid/lista,
    // zoom +/-, recargar fuentes, nuevo estilo, ajustes rapidos -- antes era
    // una barra horizontal arriba de la galeria, le robaba alto util a las
    // tarjetas de tema. Ver Render().
    void RenderLeftRail();
    void RenderThemeGrid();
    void RenderQuickAdjust();
    // Ajustes rapidos ya no vive fijo debajo de la galeria: ahora es un
    // icono en la toolbar que abre esto como popup flotante (ver RenderTopBar).
    void RenderQuickAdjustPopup();

    void RenderThemeCard(const std::string& name, float cardW, float cardH,
                         int idx, int col, int cols);
    void RenderThemeRow(const std::string& name, float panelW, float rowH, int idx);

    // ── Temas ─────────────────────────────────────────────────────────────────
    bool SaveTheme(const std::string& name, const StyleData& data);
    bool LoadThemeData(const std::string& name, StyleData& outData);
    void ApplyTheme(const std::string& name);
    void DeleteTheme(const std::string& name);
    void ApplyCurrentStyleToCore();

    void LoadThemeList();
    void LoadFontsList();
};

} // namespace ProyecThor::UI
