#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"

namespace ProyecThor::UI {

namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

// Columna central: botones TRANSMITIR y LOOP.
void MonitorView::RenderCenterColumn(float w, float h, Core::VLCBasePlayer* previewPlayer)
{
    float btnW   = std::max(w - 4.0f, 16.0f);
    float hPad   = (w - btnW) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.0f, 0.0f, 0.0f, 0.0f });
    ImGui::BeginChild("##center_col", { w, h }, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float baseMainH   = 52.0f;
    const float baseLoopH   = 32.0f;
    const float baseSpacing = 6.0f;
    const float baseTotalH  = baseMainH + baseSpacing + baseLoopH;
    const float scale       = std::clamp(h / baseTotalH, 0.55f, 1.0f);

    const float mainH   = baseMainH * scale;
    const float loopH   = baseLoopH * scale;
    const float spacing = baseSpacing * scale;
    const float totalH  = mainH + spacing + loopH;
    float       startY  = std::max(0.0f, (h - totalH) * 0.5f);

    ImGui::SetCursorPosY(startY);

    ImGui::SetCursorPosX(hPad);
    ImGui::PushID("btn_transmit");
    if (DrawIconButton("arrow_forward", mainH * 0.56f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, { btnW, mainH }))
    {
        auto sel = Core::PresentationCore::Get().PeekSelection();
        if (!sel.title.empty())
        {
            std::string finalPath = sel.title;
            bool isVideo = (sel.type == Core::ItemType::Video);
            if (finalPath.rfind("http", 0) != 0 && !std::filesystem::path(finalPath).is_absolute()) {
                if (isVideo) finalPath = VideosPath() + finalPath;
            }

            std::string norm = finalPath;
            std::replace(norm.begin(), norm.end(), '\\', '/');
            bool isBg = (norm.find("/backgrounds/") != std::string::npos ||
                         norm.find("assets/backgrounds") != std::string::npos ||
                         sel.type != Core::ItemType::Video);
            bool allowAudio = isVideo && !isBg;

            Core::PresentationCore::Get().SetLiveMute(m_LiveMuted);
            Core::PresentationCore::Get().SetLiveVolume(
                m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));

            Core::PresentationCore::Get().SetBackgroundMedia(finalPath, isVideo, allowAudio);
            Core::PresentationCore::Get().SetProjecting(true);
            m_LivePlaying = true;

            Core::VLCBasePlayer* newBg = Core::PresentationCore::Get().GetBackgroundPlayer();
            if (previewPlayer && previewPlayer != newBg) {
                previewPlayer->SetPause(true);
                m_PreviewPlaying = false;
            }
            if (newBg)
                newBg->SetPause(false);
        }
    }
    ImGui::PopID();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);

    bool loopEnabled = Core::PresentationCore::Get().GetLiveLoop();
    ImGui::SetCursorPosX(hPad);
    ImVec4 loopBase = loopEnabled ? MT::k_AmberBtn    : MT::k_NeutBtn;
    ImVec4 loopHov  = loopEnabled ? MT::k_AmberBtnHov : MT::k_NeutBtnHov;
    ImVec4 loopAct  = loopEnabled ? MT::k_AmberBtnAct : MT::k_NeutBtnAct;

    ImGui::PushID("btn_loop");
    if (DrawIconButton("repeat", loopH * 0.56f, loopBase, loopHov, loopAct, { btnW, loopH }, loopEnabled))
    {
        Core::PresentationCore::Get().SetLiveLoop(!loopEnabled);
    }
    ImGui::PopID();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI