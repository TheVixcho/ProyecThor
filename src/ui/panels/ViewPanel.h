#include "core/settings/SettingsManager.h"
#pragma once
#include "../IPanel.h"
#include "core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <unordered_map>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;
class TeamChatPanel;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr);
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

    void SetTeamChatPanelRef(TeamChatPanel* ref) { m_TeamChatPanelRef = ref; }

    bool IsDetached() const { return m_IsDetached; }
    void SetDetached(bool detached);

    void RenderOverlaysContent();
    void RenderChatContent();
    void RenderPadsContent(float w = 0.0f, float h = 0.0f);
    void RenderPadEditorModal();
    void SetFolderPlaylist(const std::vector<std::string>& vids) {
        m_FolderPlaylistVideos = vids;
        m_FolderPlaylistIndex = 0;
    }
    void ClearFolderPlaylist() {
        m_FolderPlaylistVideos.clear();
        m_FolderPlaylistIndex = 0;
    }
    void RenderClockContent();
    void RenderTilesContent();

private:
    UIManager*      m_UIManager        = nullptr;
    TeamChatPanel*  m_TeamChatPanelRef = nullptr;

    void RenderContent(float panelW, float panelH);

    void RenderQuickActionsClear(float railW);
    void RenderQuickActionsConfig(float stripH);

    void RenderCompactWide(ImVec2 avail, bool showQuickActions);

    enum class InlineTool { None, Overlays, Chat, Pads, Clock, Tiles };
    InlineTool m_ActiveTool = InlineTool::None;

    static constexpr float kLiveTransportMinH = 160.0f;

    void RenderInlineTool(float w, float h);

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

    // Edición en vivo de letras en ViewPanel
    bool        m_IsEditingLiveLyrics = false;
    std::string m_LiveLyricsEditBuf;
    std::string m_LastLiveLyricsEditBuf;
    std::string m_LiveLyricsOriginalText;
    std::string m_EditingSongFilename;
    int         m_EditingStanzaIndex = -1;
    bool        m_FocusLiveEditInput = false;
    double      m_LiveLyricsSavedFeedbackTime = 0.0;
    bool        m_IsDetached = false;

    // ── Estado de Pads / Escenas Rápidas ──
    int         m_EditingPadIndex = -1;
    bool        m_ShowPadEditModal = false;
    ProyecThor::Settings::PadSettings m_TempPadEdit;
    int         m_LastActivatedPadIndex = -1;
    double      m_LastPadActivatedFeedbackTime = 0.0;
    std::string m_ActiveFeedbackName;
    std::vector<std::string> m_FolderPlaylistVideos;
    size_t      m_FolderPlaylistIndex = 0;
};

}
