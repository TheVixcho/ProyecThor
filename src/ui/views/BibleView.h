#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <imgui.h>
#include "biblia/BibleTypes.h"
#include "biblia/BibleQuickNav.h"
#include "biblia/BibleWordSearch.h"

namespace ProyecThor::UI {

class BibleView {
public:
    BibleView();
    ~BibleView() = default;

    void Render();
    bool IsSearchOrOverlayActive() const {
        return m_SearchFocused || m_QuickNav.IsOpen() || m_WordSearch.IsOpen() ||
               m_JumpMode != JumpKind::None || m_ShowEditModal ||
               m_ShowVersionPicker || m_ShowHistory || m_ShowFavorites;
    }

private:
    void LoadXMLBible(const std::string& path);
    void SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx);
    void ProjectVerse(int bookIdx, int chapIdx, int verseIdx);
    void ReprojectInCurrentBible();
    void NavigateVerse(int delta);
    std::string PeekNextVerseText(int bookIdx, int chapIdx, int verseIdx) const;
    bool        PeekNextVerse(int bookIdx, int chapIdx, int verseIdx, std::string& outBody, std::string& outRef) const;
    bool        PeekPrevVerse(int bookIdx, int chapIdx, int verseIdx, std::string& outBody, std::string& outRef) const;

    void RenderTopBar();
    void RenderVersionPopup();
    void RefreshAvailableBibles();
    void ImportBibleFiles();
    bool DeleteBibleFile(const std::string& filename);
    void RenderHistoryPopup();
    void RenderFavoritesPopup();
    void RenderBookGrid();
    void RenderChapterGrid();
    void RenderVerseList();
    void RenderClassicVerseList(const BookData& book, const ChapterData& chap, float r, float g, float bv, float marginH);
    void RenderPreviewVerseGrid(const BookData& book, const ChapterData& chap, float r, float g, float bv, float marginH);
    void RenderEditModal();

    void JumpToFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum);
enum class JumpKind { None, Chapter, Verse };

void UpdateModifierTaps();
void OpenJump(JumpKind kind);
void CloseJump();
void UpdateJumpOverlay();
void RenderJumpOverlay();
void ConfirmJump();

JumpKind    m_JumpMode = JumpKind::None;
std::string m_JumpBuffer;
std::string m_JumpStatus;
ImVec2      m_JumpCardMin = {};
ImVec2      m_JumpCardMax = {};

double m_CtrlDownSince  = -1.0;
bool   m_CtrlComboFired = false;
double m_AltDownSince   = -1.0;
bool   m_AltComboFired  = false;
    void HandleQuickNavConfirm();
    void HandleWordSearchConfirm();

    BibleData   m_CurrentBible;
    bool        m_BibleLoaded     = false;
    std::string m_LoadedBiblePath;
    std::string m_LastSelectedFile;

    int m_SelectedBook    = 0;
    int m_SelectedChapter = 0;
    int m_SelectedVerse   = 0;

    int m_ProjectedBookNum  = -1;
    int m_ProjectedChapNum  = -1;
    int m_ProjectedVerseNum = -1;
    int m_ProjectedBookIdx  = -1;
    int m_ProjectedChapIdx  = -1;
    int m_ProjectedVerseIdx = -1;

    char m_LiveSearch[128]  = "";
    int  m_FilteredBook     = -1;
    int  m_FilteredChapter  = -1;
    int  m_TestamentFilter  = 0;
    bool m_SearchFocused    = false;
    bool m_NeedsFocusSearch = false;

    std::vector<HistoryEntry> m_History;
    bool   m_ShowHistory    = false;
    ImVec2 m_HistoryBtnPos  = {};
    ImVec2 m_HistoryBtnSize = {};

    bool   m_ShowFavorites    = false;
    ImVec2 m_FavoritesBtnPos  = {};
    ImVec2 m_FavoritesBtnSize = {};

    BibleQuickNav m_QuickNav;
    ImVec2        m_QuickNavBtnPos  = {};
    ImVec2        m_QuickNavBtnSize = {};

    BibleWordSearch m_WordSearch;
    ImVec2          m_WordSearchBtnPos  = {};
    ImVec2          m_WordSearchBtnSize = {};

    bool        m_ShowEditModal    = false;
    int         m_EditBookIdx      = -1;
    int         m_EditChapIdx      = -1;
    int         m_EditVerseIdx     = -1;
    char        m_EditBuffer[4096] = {};
    std::string m_EditStatus;

    int m_ScrollToVerse = -1;

    bool  m_ClassicListView = false;
    float m_VerseCardZoom   = 1.0f;

    bool                     m_ShowVersionPicker    = false;
    bool                     m_DeleteBibleMode      = false;
    bool                     m_TriedAutoSelectBible = false;
    std::string              m_ConfirmDeleteBible;
    std::atomic<bool>        m_BibleImportRunning{false};
    std::atomic<bool>        m_NeedsRefreshBibles{false};
    std::mutex               m_ImportedBibleMutex;
    std::string              m_LastImportedBibleFile;
    ImVec2                   m_VersionBtnPos     = {};
    ImVec2                   m_VersionBtnSize    = {};
    std::vector<std::string> m_AvailableBibleFiles;
};

}
