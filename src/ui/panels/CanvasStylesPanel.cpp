#include "CanvasStylesPanel.h"
#include "layers/LayersStyleTab.h"

namespace ProyecThor::UI {

CanvasStylesPanel::CanvasStylesPanel()
    : m_StyleTab(std::make_unique<LayersStyleTab>())
{}

CanvasStylesPanel::~CanvasStylesPanel() = default;

void CanvasStylesPanel::RenderContent() {
    m_StyleTab->Render();
}

void CanvasStylesPanel::SetUIManager(UIManager* uiManager) {
    m_StyleTab->SetUIManager(uiManager);
}

} // namespace ProyecThor::UI