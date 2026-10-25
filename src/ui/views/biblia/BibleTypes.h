#pragma once

#include <string>
#include <vector>

namespace ProyecThor::UI {

struct VerseData {
    int         number = 0;
    std::string text;
    bool        edited = false;
};

struct ChapterData {
    int                    number = 0;
    std::vector<VerseData> verses;
};

struct BookData {
    std::string              name;
    std::vector<ChapterData> chapters;
    int                      canonicalNumber = 0;
};

struct BibleData {
    std::string           name;
    std::vector<BookData> books;

    std::string translation;
    std::string info;
    std::string link;
};

enum class BibleSection {
    Pentateuch, HistoricalOT, Wisdom, MajorProphets, MinorProphets,
    Gospels, Acts, PaulineEpistles, GeneralEpistles, Apocalypse
};

struct HistoryEntry {
    std::string ref;
    std::string fullText;
    std::string body;
    int         bookIdx  = -1;
    int         chapIdx  = -1;
    int         verseIdx = -1;
};

}
