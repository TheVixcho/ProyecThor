#pragma once
#include <memory>
#include <string>
#include "preview/SongView.h"
#include "IPanel.h" 

#include "preview/BibleView.h"
#include "preview/MonitorView.h"
#include "preview/MediaView.h"

namespace ProyecThor::Core {
    class VLCVideoLayer;
}

namespace ProyecThor::UI {

    class PreviewPanel : public IPanel { 
    public:
        PreviewPanel();
        ~PreviewPanel() override;

        void Render() override; 
        
        // AÑADIDO: Implementación de la función virtual pura de IPanel
        std::string GetName() const override { return "PreviewPanel"; }

    private:
        std::unique_ptr<ProyecThor::Core::VLCVideoLayer> m_PreviewPlayer;

        BibleView m_BibleView;
        MonitorView m_MonitorView;
        MediaView m_MediaView;
        SongView m_SongView;
    };

} // namespace ProyecThor::UI