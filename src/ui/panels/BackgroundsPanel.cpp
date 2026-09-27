#include "BackgroundsPanel.h"
#include "layers/LayersBgTab.h"

namespace ProyecThor::UI {

BackgroundsPanel::BackgroundsPanel()
    : m_BgTab(std::make_unique<LayersBgTab>())
{}

BackgroundsPanel::~BackgroundsPanel() = default;

void BackgroundsPanel::RenderContent() {
    m_BgTab->Render();
}

} // namespace ProyecThor::UI