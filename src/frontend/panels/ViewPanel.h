#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <unordered_map>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;
class TeamChatPanel;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr) : m_UIManager(uiManager) {}
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

    void SetTeamChatPanelRef(TeamChatPanel* ref) { m_TeamChatPanelRef = ref; }

private:
    UIManager*      m_UIManager        = nullptr;
    TeamChatPanel*  m_TeamChatPanelRef = nullptr;

    void RenderContent(float panelW, float panelH);

    void RenderQuickActionsClear(float railW);
    void RenderQuickActionsConfig(float stripH);

    void RenderCompactWide(ImVec2 avail, bool showQuickActions);

    enum class InlineTool { None, Overlays, Chat, Pads, Clock };
    InlineTool m_ActiveTool = InlineTool::None;

    static constexpr float kLiveTransportMinH = 120.0f;

    void RenderInlineTool(float w, float h);
    void RenderOverlaysContent();
    void RenderChatContent();
    void RenderPadsContent();
    void RenderClockContent();
    std::unordered_map<std::string, ImTextureID> m_OverlayThumbCache;

    void RenderNetworkBar(
        const ImVec2&                   p0,
        const ImVec2&                   p1,
        const Core::PresentationState&  state,
        Core::PresentationCore&         core);

    void RenderLiveTransport(float w, float h);

    enum class PreviewSource { Publico, Stage, Transmision, Lan };
    PreviewSource m_PreviewSource = PreviewSource::Publico;

    AudioMeters m_AudioMeters;
    bool        m_LivePlaying = false;
    bool        m_LiveMuted   = false;
    float       m_LiveVolume  = 0.8f;
};

}
