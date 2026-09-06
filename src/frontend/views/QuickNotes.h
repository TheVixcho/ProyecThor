#pragma once
#include <string>
#include <vector>
#include <array>

namespace ProyecThor::UI {

enum class QuickNoteTransmitMode {
    Off,
    MainOnly,
    LANOnly,
    Both
};

struct QuickNoteItem {
    std::string id;
    std::string title;
    std::string content;
    std::string category;
    std::string styleName;
    std::string updatedAt;
    bool        isFavorite = false;
};

enum class QuickNotesTab {
    LiveEditor = 0,
    Library = 1
};

class QuickNotes {
public:
    QuickNotes();
    ~QuickNotes();

    std::string GetName() const;
    void Render();
    void PersistNow();

    void SaveCurrentToLibrary(const std::string& title, const std::string& category = "General");
    void LoadFromLibrary(const QuickNoteItem& item, bool transmitImmediately = false);
    void DeleteFromLibrary(const std::string& id);
    void ToggleFavorite(const std::string& id);

private:
    void SyncTransmission();
    void ClearFromCore();
    void RenderHeaderBar();
    void RenderLiveEditorTab();
    void RenderLibraryTab();
    void RenderTransmitCards();
    void RenderStyleSelector();
    void RenderSaveModal();
    void LoadPersisted();
    void LoadLibraryFromDisk();
    void SaveLibraryToDisk();

    std::array<char, 4096> m_TextBuffer{};
    bool m_IsLive = false;
    double m_LastPersistTime = 0.0;

    QuickNoteTransmitMode m_TransmitMode     = QuickNoteTransmitMode::Off;
    QuickNoteTransmitMode m_PrevTransmitMode = QuickNoteTransmitMode::Off;
    std::string m_StyleName;

    std::vector<QuickNoteItem> m_SavedNotes;
    QuickNotesTab              m_CurrentTab = QuickNotesTab::LiveEditor;
    char                       m_SearchFilter[128]{};
    std::string                m_SelectedCategoryFilter = "Todos";

    bool                       m_ShowSaveModal = false;
    char                       m_SaveTitleBuf[128]{};
    int                        m_SaveCategoryIdx = 0;
    char                       m_SaveCustomCategoryBuf[64]{};

    std::string                m_FeedbackMessage;
    double                     m_FeedbackTime = 0.0;
};

}

