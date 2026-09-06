#pragma once
#include "IPanel.h"
#include "AudioDawPanel.h"
#include <memory>
#include <string>

namespace ProyecThor::UI {

class UIManager;
class LibraryPanel;
class OverlayLibraryTab;

// ── VideoEditorPanel ─────────────────────────────────────────────────────────
// Ventana dockeable "Producción" (ver Settings::WorkspaceLayoutPreset::Video
// / UIManager::BuildWorkspaceLayoutVideo). Rail vertical a la izquierda
// (mismo lenguaje visual que LibrarySidebar::RenderSidebarButton -- pedido
// explicito de estilo) con 3 secciones, absorbiendo lo que antes eran
// presets de espacio de trabajo separados ("Render"/"Audio"/"Imagen"):
//   - Render: conversor de formato real (LibraryPanel::RenderConverterSection,
//     misma instancia de LibraryPanel de siempre).
//   - Audio: DAW real (grabar/cortar/mover/exportar, ver AudioDawPanel).
//   - Overlays: galeria + editor de Overlays (ver OverlayLibraryTab), misma
//     instancia que antes vivia en el ex-preset "Imagen".
// Colorimetria/Canales de trabajo (placeholders) se sacaron -- pedido
// explicito de dejar solo lo que tiene contenido real.
class VideoEditorPanel : public IPanel {
public:
    ~VideoEditorPanel();

    void Render() override;
    std::string GetName() const override { return "VideoEditor"; }

    void SetUIManager(UIManager* mgr);

private:
    enum class Tab { Render, Audio, Overlays };

    void RenderRail();
    void RenderRenderTab();
    void RenderAudioTab();
    void RenderOverlaysTab();

    LibraryPanel* ResolveLibraryPanel();

    UIManager*    m_UIManagerRef = nullptr;
    Tab           m_Tab          = Tab::Render;
    AudioDawPanel m_Daw;
    std::unique_ptr<OverlayLibraryTab> m_OverlayTab;
};

} // namespace ProyecThor::UI
