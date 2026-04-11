#include "PreviewPanel.h"
#include "../../PresentationCore.h"
#include <imgui.h>

namespace ProyecThor::UI {

    PreviewPanel::PreviewPanel() {
        m_PreviewPlayer = std::make_unique<ProyecThor::Core::VLCVideoLayer>(1280, 720);
    }

    PreviewPanel::~PreviewPanel() {
        if (m_PreviewPlayer) m_PreviewPlayer->Stop();
    }

    void PreviewPanel::Render() {
        if (m_PreviewPlayer)
            m_PreviewPlayer->UpdateTexture();

        auto& core = ProyecThor::Core::PresentationCore::Get();
        core.Update(); 

        ImGui::Begin("Preview");

        auto selection = core.GetSelection();

        if (selection.type == ProyecThor::Core::ItemType::Bible) {
            m_BibleView.Render();
        }
        else if (selection.type == ProyecThor::Core::ItemType::Video ||
                 selection.type == ProyecThor::Core::ItemType::Image) {
            m_MonitorView.Render(m_PreviewPlayer.get());
            ImGui::Separator();
            ImGui::Spacing();
            m_MediaView.Render(m_PreviewPlayer.get());
        }
        else if (selection.type == ProyecThor::Core::ItemType::Song) {
            m_SongView.Render();
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
