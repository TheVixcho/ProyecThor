#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "BibleTypes.h"

namespace ProyecThor::UI {

struct QuickNavResolution {
    bool hasBook            = false;
    int  bookCandidateCount = 0;
    int  bookIdx            = -1;

    bool hasChapter         = false;
    int  chapterNumber      = -1;
    int  chapterIdx         = -1;
    int  chapterVerseCount  = 0;

    bool hasVerse           = false;
    int  verseNumber        = -1;
    int  verseIdx           = -1;
};

enum class QuickNavStep {
    Book,
    Chapter,
    Verse
};

class BibleQuickNav {
public:
    void Open();
    void Close();
    bool IsOpen() const { return m_Open; }
ImVec2 m_CardMin = {};
ImVec2 m_CardMax = {};
int    m_OpenedFrame = -1;

    bool Update(const BibleData& bible);

    void Render(const BibleData& bible);

    const QuickNavResolution& GetResolution() const { return m_Resolution; }

private:
    void RefreshBookCandidates(const BibleData& bible);
    void ConfirmBookStep(const BibleData& bible);
    void ConfirmChapterStep(const BibleData& bible);
    bool ConfirmVerseStep(const BibleData& bible);
    void GoBackStep(const BibleData& bible);

    bool         m_Open = false;
    QuickNavStep m_Step = QuickNavStep::Book;
double m_OpenSince    = -1.0;
    double m_ClosingUntil = -1.0;
    std::string  m_BookBuffer;
    std::string  m_ChapterBuffer;
    std::string  m_VerseBuffer;

    std::vector<int> m_BookCandidates;

    std::string  m_StatusMessage;

    QuickNavResolution m_Resolution;
};

}
