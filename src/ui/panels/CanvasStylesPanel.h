#pragma once
#include "styles/CanvaStyleEditor.h"
#include <string>
#include <memory>

namespace ProyecThor::UI {

class LayersStyleTab; // Tu clase original que renderiza los estilos
class UIManager;

// Ya no es un IPanel independiente: vive como seccion del sidebar del hub de
// Diseño (ver StylesHubPanel.h/.cpp).
class CanvasStylesPanel {
public:
    CanvasStylesPanel();
    ~CanvasStylesPanel();

    void RenderContent();
    void SetUIManager(UIManager* uiManager);
    std::string GetName() const { return "Estilos"; }

private:
    std::unique_ptr<LayersStyleTab> m_StyleTab;
};

} // namespace ProyecThor::UI