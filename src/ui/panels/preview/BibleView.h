#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

    struct VerseData {
        int number;
        std::string text;
    };

    struct ChapterData {
        int number;
        std::vector<VerseData> verses;
    };

    struct BookData {
        std::string name;
        std::vector<ChapterData> chapters;
        int canonicalNumber = 0;
    };

    struct BibleData {
        std::string name;
        std::vector<BookData> books;
    };

    enum class BibleSection {
        Pentateuch,
        HistoricalOT,
        Wisdom,
        MajorProphets,
        MinorProphets,
        Gospels,
        Acts,
        PaulineEpistles,
        GeneralEpistles,
        Apocalypse
    };

    class BibleView {
    public:
        BibleView() = default;
        ~BibleView() = default;

        void Render();

    private:
        void LoadXMLBible(const std::string& path);
        void ParseSearchQuery(const std::string& query);
        void ApplyLiveFilter();
        BibleSection GetBookSection(int canonicalNumber) const;
        void GetSectionColor(BibleSection section, float& r, float& g, float& b) const;
// Edit popup state
char        m_EditVerseBuffer[4096] = {};
int         m_EditingVerseNumber    = -1;
int         m_EditingBookIndex      = -1;
int         m_EditingChapIndex      = -1;
std::string m_EditSaveStatus;

void SaveVerseToXML(int bookIndex, int chapterIndex, int verseNumber, const std::string& newText);

        BibleData m_CurrentBible;
        bool m_BibleLoaded = false;
        std::string m_LoadedBiblePath;
        std::string m_LastSelectedFile;

        int m_SelectedBook = 0;
        int m_SelectedChapter = 0;

        char m_LiveSearch[128] = "";
        std::string m_LastLiveSearch;

        int m_FilteredBook = -1;
        int m_FilteredChapter = -1;

        std::vector<std::string> m_VerseHistory;
        bool m_SearchFocused = false;
        bool m_NeedsFocusSearch = false;
    };

} // namespace ProyecThor::UI
