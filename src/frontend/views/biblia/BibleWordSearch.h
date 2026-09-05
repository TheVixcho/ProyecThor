#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "BibleTypes.h"

namespace ProyecThor::UI {

struct WordSearchHit {
    int bookIdx  = -1;
    int chapIdx  = -1;
    int verseIdx = -1;
};

class BibleWordSearch {
public:
    void Open();
    void Close();
    bool IsOpen() const { return m_Open; }

    void Update(const BibleData& bible);

    bool Render(const BibleData& bible, ImVec2 anchorPos, ImVec2 anchorSize);

    const WordSearchHit& GetResolution() const { return m_Resolution; }

private:
    void RunSearch(const BibleData& bible);

    bool        m_Open = false;
    bool        m_NeedsFocus = false;
    char        m_Buffer[128] = "";
    std::string m_LastQuery;

    std::vector<WordSearchHit> m_Hits;
    static constexpr int kMaxHits = 200;

    WordSearchHit m_Resolution;
};

}

