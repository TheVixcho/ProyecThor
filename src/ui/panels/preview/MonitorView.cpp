#include "MonitorView.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <iomanip>
#include <sstream>

namespace ProyecThor::UI {

std::string MonitorView::FormatTime(int64_t ms) {
    if (ms < 0) ms = 0;
    int total   = static_cast<int>(ms / 1000);
    int minutes = total / 60;
    int seconds = total % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

void MonitorView::Render(Core::VLCBasePlayer* player) {
    if (!player) {
        ImGui::TextDisabled("Sin reproductor disponible.");
        return;
    }

    // --- Miniatura de video ---
    void* texID = player->GetTextureID();
    float previewW = ImGui::GetContentRegionAvail().x;
    float previewH = previewW / (16.0f / 9.0f);

    if (texID) {
        ImGui::Image(reinterpret_cast<ImTextureID>(texID),
                     ImVec2(previewW, previewH));
    } else {
        ImGui::Dummy(ImVec2(previewW, previewH));
        ImGui::TextDisabled("Sin señal de video.");
    }

    ImGui::Spacing();

    // --- Timeline ---
    float pos = player->GetPosition();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##timeline", &pos, 0.0f, 1.0f, ""))
        player->SetPosition(pos);

    // --- Tiempo ---
    int64_t curMs = player->GetTime();
    int64_t lenMs = player->GetLength();
    ImGui::TextDisabled("%s / %s",
        FormatTime(curMs).c_str(),
        FormatTime(lenMs).c_str());

    ImGui::Spacing();

    // --- Controles de transporte ---
    if (m_IsPlaying) {
        if (ImGui::Button("Pausa", ImVec2(80, 28))) {
            player->SetPause(true);
            m_IsPlaying = false;
        }
    } else {
        if (ImGui::Button("Play", ImVec2(80, 28))) {
            player->SetPause(false);
            m_IsPlaying = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Detener", ImVec2(80, 28))) {
        player->SetPosition(0.0f);
        player->SetPause(true);
        m_IsPlaying = false;
    }

    ImGui::Spacing();

    // --- Volumen ---
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::SliderFloat("Volumen##vol", &m_Volume, 0.0f, 1.0f, "%.0f%%",
                           ImGuiSliderFlags_None)) {
        player->SetVolume(static_cast<int>(m_Volume * 100.0f));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Proyectar ---
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("PROYECTAR VIDEO (LIVE)", ImVec2(-1.0f, 40.0f))) {
        auto sel = Core::PresentationCore::Get().PeekSelection();
        if (!sel.title.empty()) {
            Core::PresentationCore::Get().SetBackgroundMedia(
                "assets/videos/" + sel.title, true);
            Core::PresentationCore::Get().SetProjecting(true);
        }
    }
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI