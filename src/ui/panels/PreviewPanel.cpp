#include "PreviewPanel.h"
#include "../../PresentationCore.h"
#include "../../VLCBasePlayer.h"
#include <imgui.h>

namespace ProyecThor::UI {

    PreviewPanel::PreviewPanel() {
        m_PreviewPlayer = std::make_unique<Core::VLCBasePlayer>(1280, 720);
    }

    PreviewPanel::~PreviewPanel() {
        if (m_PreviewPlayer) m_PreviewPlayer->Stop();
    }

    std::string PreviewPanel::GetName() const {
        return "PreviewPanel";
    }

    void PreviewPanel::Render() {
        if (m_PreviewPlayer)
            m_PreviewPlayer->UpdateTexture();

        auto& core = Core::PresentationCore::Get();
        core.Update(); 
        
        // Leemos la selección sin consumirla
        auto selection = core.PeekSelection();

        ImGui::Begin("Preview");

        if (selection.type == Core::ItemType::Bible) {
            // La Biblia va directo a su propia vista
            m_BibleView.Render();
        }
        else if (selection.type == Core::ItemType::Song) {
            // Las Canciones van directo a su propia vista
            m_SongView.Render();
        }
        else if (selection.type == Core::ItemType::Image) {
            // Las imágenes NO necesitan la pantalla negra de video (MonitorView)
            // Solo necesitan sus botones de control
            m_MediaView.Render(m_PreviewPlayer.get());
        }
        else if (selection.type == Core::ItemType::Video) {
            // Los videos necesitan AMBOS: La pantalla visual y los controles
            m_MonitorView.Render(m_PreviewPlayer.get());
            ImGui::Separator();
            ImGui::Spacing();
            m_MediaView.Render(m_PreviewPlayer.get());
        }
        else {
            ImVec2 windowSize = ImGui::GetWindowSize();
            const char* msg   = "Seleccione un archivo multimedia, cancion o pasaje";
            ImVec2 textSize   = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2(
                (windowSize.x - textSize.x) * 0.5f,
                (windowSize.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", msg);
        }

        ImGui::End();
    }

} // namespace ProyecThor::UI