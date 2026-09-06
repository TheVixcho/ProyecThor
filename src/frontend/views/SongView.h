#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include "SongEditView.h"

namespace ProyecThor::UI {

    class SongView {
    public:
        SongView();
        ~SongView() = default;

        void Render();

    private:
        std::string m_CurrentSongTitle;
        int         m_ActiveStanzaIndex;
        bool        m_HasRecordedCurrentSongProjection;
        float       m_StanzaCardZoom = 1.0f;

        bool         m_ShowEditor = false;
        SongEditView m_EditView;

        int         m_ColorPickerForStanza    = -1;
        bool        m_OpenColorPickerRequest  = false;
        bool        m_OpenSongSettingsRequest = false;

        int               m_TempoBpm              = 0;
        std::vector<int>  m_VerseDurationOverrideMs;
        bool              m_AutoAdvancePlaying    = false;
        double            m_AutoAdvanceDeadline   = 0.0;

        void ReloadTempoMeta(const std::string& songFilename);
        float ComputeVerseDurationSeconds(const std::string& stanzaText, int stanzaIndex) const;

        void RenderBrowseGrid();
        void RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered);
        void RenderSongSettingsPopup(const std::string& songFilename);
        void RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH, bool matchesSearch = false);
    };

}

