#pragma once
#include <string>
#include <vector>
#include <imgui.h>

namespace ProyecThor::UI {

class SongEditView {
public:
    SongEditView() = default;

    void Open(const std::string& filename);

    bool Render();

    void FlushIfDirty();

    const std::string& GetFilename() const { return m_Filename; }

private:
    struct EditSnapshot {
        std::string title, author, note, copyright, extra;
        std::string lyrics;

        std::string baseLyrics;

        int linesPerSlide = 0;
    };

    void PushUndoSnapshot();
    void Undo();
    void Redo();
    void MarkDirty();

    void RenderTopBar(bool& outWantsBack);
    void RenderLeftPane(float width);
    void RenderRightPane(float width);

    std::vector<std::string> ComputePreviewSlides() const;

    std::string m_Filename;
    std::string m_FilePath;

    EditSnapshot m_Current;
    EditSnapshot m_Undo;
    EditSnapshot m_Redo;
    bool m_HasUndo = false;
    bool m_HasRedo = false;

    bool   m_Dirty        = false;
    double m_LastEditTime  = 0.0;
    bool   m_JustSaved     = false;
    double m_JustSavedAt   = 0.0;

    float m_PreviewZoom = 1.0f;

    int              m_TempoBpm = 0;
    std::vector<int> m_VerseDurationOverrideMs;

    int    m_DurationPopupForSlide  = -1;
    bool   m_OpenDurationPopupRequest = false;
    int    m_DurationPopupValueMs     = 0;

    void RenderVerseDurationPopup(const std::vector<std::string>& slides);
};

}

