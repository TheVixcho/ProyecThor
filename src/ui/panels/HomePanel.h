#pragma once
#include <memory>
#include <string>
#include "IPanel.h"
#include "monitor/MonitorView.h"
#include "ui/views/MediaView.h"
#include "ui/views/BibleView.h"
#include "ui/views/SongView.h"
#include "ui/views/DocumentView.h"
#include "media/player/VLCBasePlayer.h"
#include "ui/views/Audio.h"

namespace ProyecThor::Core { class VLCBasePlayer; }

namespace ProyecThor::UI {

class UIManager;

// Home ya no es un hub multi-seccion: Reloj/Anuncios/Captura/Transmision se
// movieron a StylesHubPanel (Diseño) o a LibraryPanel — sin rail de iconos ni
// pestañas propias. Notas vive en un popup propio desde la toolbar superior
// (ver UIManager::RenderNotesPopup), no aca.
class HomePanel : public IPanel {
public:
    HomePanel();
    virtual ~HomePanel();

    void        Render()  override;
    std::string GetName() const override { return "Home"; }
    void SetAudioPanel(AudioPanel* ap) { m_AudioPanelRef = ap; }

    UIManager*  m_UIManagerRef  = nullptr;

private:
    void RenderHomeContent(); // contenido de la sección "Home" (preview en vivo)

    AudioPanel* m_AudioPanelRef = nullptr;
    MonitorView  m_MonitorView;
    MediaView    m_MediaView;
    BibleView    m_BibleView;
    SongView     m_SongView;
    DocumentView m_DocumentView;
};

} // namespace ProyecThor::UI
