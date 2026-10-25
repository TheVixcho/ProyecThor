#pragma once
#include "IPanel.h"
#include "BackgroundsPanel.h"
#include "CanvasStylesPanel.h"
#include "ShadersPanel.h"
#include "layers/LayersTransitionsTab.h"
#include "ui/views/Announcements.h"
#include "capture/CapturePanel.h"
#include "overlay/OverlayLibraryTab.h"
#include <string>
#include <memory>
#include <functional>

namespace ProyecThor::UI {

class UIManager;
class TransitionPanel;

// Hub de Diseño / Barra de herramientas inferior de Main:
// Reúne Fondos + Multimedia + Overlays + Estilos + Shaders + Transiciones + Anuncios + Captura
// y módulos adicionales activados (Render, Web, 3D, Lab, Plugins).
enum class StylesSection {
    Backgrounds   = 0,
    Multimedia    = 1,
    Overlays      = 2,
    Styles        = 3,
    Shaders       = 4,
    Transitions   = 5,
    Announcements = 6,
    Capture       = 7,
    Render        = 8,
    Web           = 9,
    Model3D       = 10,
    Lab           = 11,
    Plugins       = 12,
};

class StylesHubPanel : public IPanel {
public:
    explicit StylesHubPanel(UIManager* uiManager);
    ~StylesHubPanel() override = default;

    void        Render()  override;
    std::string GetName() const override { return "Diseño"; }

    void SelectSection(StylesSection section);
    StylesSection GetCurrentSection() const { return m_CurrentSection; }

    // TransitionPanel sigue siendo dueño de UIManager (su Update/Trigger ya
    // corre incondicionalmente cada frame, independiente de este hub) — acá
    // solo se recibe un puntero para dibujar su UI de configuracion.
    void SetTransitionPanel(TransitionPanel* tp) {
        m_TransitionsRef = tp;
        m_TransitionsTab.SetTransitionPanel(tp);
    }

    // Usado por UIManager (bloque ProjectorLive) para llegar a Anuncios/
    // Captura en vivo sin que sean IPanel propios (mismo patron que HomePanel
    // usaba antes de que se mudaran aca).
    Announcements& GetAnnouncements() { return m_Announcements; }
    CapturePanel&  GetCapturePanel()  { return m_Capture; }

    void PreloadThumbnails(const std::function<void(int done, int total, const std::string& name)>& onProgress = nullptr) {
        m_Backgrounds.PreloadThumbnails(onProgress);
    }

private:
    // Barra compacta de transicion (duracion + Sin transicion/Disolver/
    // Avanzado). Ya no se dibuja fija arriba de Estilos (le robaba espacio
    // vertical a algo que solo importa para canciones/multi-slide) -- ahora
    // vive detras de un icono chico en el rail de arriba (ver
    // RenderTransitionRailButton), como un popup flotante. "Avanzado" abre
    // el panel completo (m_TransitionsRef->RenderContent()) como OTRO popup
    // anidado, para los tipos complejos (Zoom, Slide, Cover, Uncover).
    void RenderTransitionQuickBar();
    void RenderTransitionRailButton();
    void RenderMultimedia();

    UIManager*    m_UIManager = nullptr;
    StylesSection m_CurrentSection = StylesSection::Backgrounds;

    BackgroundsPanel                   m_Backgrounds;
    CanvasStylesPanel                  m_Styles;
    ShadersPanel                       m_Shaders;
    TransitionPanel*                   m_TransitionsRef = nullptr;
    LayersTransitionsTab               m_TransitionsTab; // pestaña "Transiciones" (catalogo con nombre)
    Announcements                      m_Announcements;
    CapturePanel                       m_Capture;
    std::unique_ptr<OverlayLibraryTab> m_OverlayTab;

    char m_MultimediaSearch[128] = {0};
};

} // namespace ProyecThor::UI
