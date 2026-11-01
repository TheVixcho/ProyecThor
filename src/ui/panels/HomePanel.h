#pragma once
#include <memory>
#include <string>
#include "IPanel.h"
#include "monitor/MonitorView.h"
#include "ui/views/MediaView.h"
#include "ui/views/BibleView.h"
#include "ui/views/SongView.h"
#include "ui/views/DocumentView.h"
#include "ui/views/MediaGridView.h"
#include "media/player/VLCBasePlayer.h"
#include "ui/views/Audio.h"

namespace ProyecThor::Core { class VLCBasePlayer; }

namespace ProyecThor::UI {

class UIManager;

class HomePanel : public IPanel {
public:
    HomePanel();
    virtual ~HomePanel();

    void        Render()  override;
    std::string GetName() const override { return "Home"; }
    void SetAudioPanel(AudioPanel* ap) { m_AudioPanelRef = ap; }
    bool IsBibleSearchActive() const { return m_BibleView.IsSearchOrOverlayActive(); }

    void RenderFloatingPreview(Core::VLCBasePlayer* player, float w, float h);
    MonitorView* GetMonitorView() { return &m_MonitorView; }

    bool IsFondosCollapsed() const { return m_FondosCollapsed; }
    void SetFondosCollapsed(bool collapsed) { m_FondosCollapsed = collapsed; }

    UIManager*  m_UIManagerRef  = nullptr;

private:
    void RenderHomeContent(); // contenido de la sección "Home" (preview en vivo)

    AudioPanel*   m_AudioPanelRef = nullptr;
    MonitorView   m_MonitorView;
    MediaView     m_MediaView;
    BibleView     m_BibleView;
    SongView      m_SongView;
    DocumentView  m_DocumentView;
    MediaGridView m_MediaGridView;
    std::string   m_LastSelectionTitle;

    bool          m_FondosCollapsed = false;
    float         m_FondosHeight    = 240.0f;
};

} // namespace ProyecThor::UI
