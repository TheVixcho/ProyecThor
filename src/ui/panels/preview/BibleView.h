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
    };

    struct BibleData {
        std::string name;
        std::vector<BookData> books;
    };

    class BibleView {
    public:
        BibleView() = default;
        ~BibleView() = default;

        void Render();

    private:
        void LoadXMLBible(const std::string& path);

        BibleData m_CurrentBible;
        bool m_BibleLoaded = false;
        std::string m_LoadedBiblePath;
        std::string m_LastSelectedFile;

        int m_SelectedBook = 0;
        int m_SelectedChapter = 0;
        std::vector<std::string> m_VerseHistory;
        char m_QuickSearch[64] = "";
    };

} // namespace ProyecThor::UI