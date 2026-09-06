#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

class TransitionPanel;

// LayersTransitionsTab — pestaña "Transiciones" del hub Diseño, junto a
// "Estilos": catálogo de transiciones GUARDADAS con nombre (tipo + duración
// + a qué afecta: Fondos/Letras/ambos), mismo espíritu que LayersStyleTab
// pero en lista (sin thumbnail visual relevante para una transición). El
// editor reusa TransitionPanel::RenderContent() embebido en un popup,
// editando el motor EN VIVO -- mismo criterio que ya usaba el popup
// "Avanzado" existente (ver StylesHubPanel::RenderTransitionQuickBar).
class LayersTransitionsTab {
public:
    LayersTransitionsTab() = default;

    // Mismo TransitionPanel que ya vive en UIManager -- se edita/aplica en
    // vivo, nunca se crea una copia aparte.
    void SetTransitionPanel(TransitionPanel* tp) { m_TransitionsRef = tp; }

    void RenderContent();

private:
    void RenderRow(int idx, float rowW);
    void RenderEditorPopup();

    void LoadPresetIntoLive(int idx);
    void SaveLiveAsPreset(int idx); // idx == -1 => nuevo preset
    void DeletePreset(int idx);

    TransitionPanel* m_TransitionsRef = nullptr;

    char m_EditNameBuf[64] = {};
    int  m_EditingIndex    = -1; // -1 mientras se crea uno nuevo
};

} // namespace ProyecThor::UI
