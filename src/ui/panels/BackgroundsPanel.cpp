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

void BackgroundsPanel::PreloadThumbnails(
    const std::function<void(int done, int total, const std::string& name)>& onProgress)
{
    if (m_BgTab)
        m_BgTab->PreloadThumbnails(onProgress);
}

} // namespace ProyecThor::UI