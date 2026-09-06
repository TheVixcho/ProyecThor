#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "biblia/BibleTypes.h"
#include "biblia/BibleQuickNav.h"
#include "biblia/BibleWordSearch.h"

namespace ProyecThor::UI {

class BibleView {
public:
    BibleView()  = default;
    ~BibleView() = default;

    void Render();

private:
    void LoadXMLBible(const std::string& path);
    void SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx);
    void ProjectVerse(int bookIdx, int chapIdx, int verseIdx);
    void ReprojectInCurrentBible();
    void NavigateVerse(int delta);
    std::string PeekNextVerseText(int bookIdx, int chapIdx, int verseIdx) const;

    void RenderTopBar();
    void RenderHistoryPopup();
    void RenderFavoritesPopup();
    void RenderBookGrid();
    void RenderChapterGrid();
    void RenderVerseList();
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
};

}
