#pragma once
#include <memory>
#include <string>
#include "IPanel.h"
#include "preview/MonitorView.h"
#include "preview/MediaView.h"
#include "preview/BibleView.h"
#include "preview/SongView.h"
#include "../../VLCBasePlayer.h"

namespace ProyecThor::Core {
    class VLCBasePlayer;
}

namespace ProyecThor::UI {

class PreviewPanel : public IPanel {
public:
    PreviewPanel();
    virtual ~PreviewPanel();

    void Render() override;
    std::string GetName() const override;

private:
    std::unique_ptr<Core::VLCBasePlayer> m_PreviewPlayer;

    MonitorView m_MonitorView;
    MediaView   m_MediaView;
    BibleView   m_BibleView;
    SongView    m_SongView;
};

} // namespace ProyecThor::UI