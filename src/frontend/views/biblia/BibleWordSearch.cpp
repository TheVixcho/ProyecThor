#include "BibleWordSearch.h"
#include "frontend/ui/DesignSystem.h"
#include "ControlWidgets.h"
#include <imgui.h>
#include <cstring>
#include <cctype>

namespace ProyecThor::UI {

namespace {

std::string FoldSpanish(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); )
    {
        unsigned char c = (unsigned char)s[i];
        if (c == 0xC3 && i + 1 < s.size())
        {
            unsigned char c2 = (unsigned char)s[i + 1];
            char folded = 0;
            switch (c2)
            {
                case 0xA1: case 0x81: folded = 'a'; break;
                case 0xA9: case 0x89: folded = 'e'; break;
                case 0xAD: case 0x8D: folded = 'i'; break;
                case 0xB3: case 0x93: folded = 'o'; break;
                case 0xBA: case 0x9A: folded = 'u'; break;
                case 0xBC: case 0x9C: folded = 'u'; break;
                case 0xB1: case 0x91: folded = 'n'; break;
                default: break;
            }
            if (folded) { out += folded; i += 2; continue; }
        }
        out += (char)std::tolower(c);
        i += 1;
    }
    return out;
}

std::vector<std::string> SplitWords(const std::string& folded)
{
    std::vector<std::string> words;
    std::string cur;
    for (char c : folded)
    {
        if (std::isspace((unsigned char)c)) {
            if (!cur.empty()) { words.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(cur);
    return words;
}

}

void BibleWordSearch::Open()
{
    m_Open = true;
    m_NeedsFocus = true;
    m_Buffer[0] = '\0';
    m_LastQuery.clear();
    m_Hits.clear();
    m_Resolution = WordSearchHit{};
}

void BibleWordSearch::Close()
{
    m_Open = false;
}

void BibleWordSearch::RunSearch(const BibleData& bible)
{
    m_Hits.clear();

    std::vector<std::string> words = SplitWords(FoldSpanish(m_Buffer));
    if (words.empty()) return;

    for (int bi = 0; bi < (int)bible.books.size(); bi++)
    {
        const auto& book = bible.books[bi];
        for (int ci = 0; ci < (int)book.chapters.size(); ci++)
        {
            const auto& chap = book.chapters[ci];
            for (int vi = 0; vi < (int)chap.verses.size(); vi++)
            {
                const auto& verse = chap.verses[vi];
                std::string folded = FoldSpanish(verse.text);

                bool matchesAll = true;
                for (const auto& w : words)
                {
                    if (folded.find(w) == std::string::npos) { matchesAll = false; break; }
                }

                if (matchesAll)
                {
                    m_Hits.push_back({ bi, ci, vi });
                    if ((int)m_Hits.size() >= kMaxHits) return;
                }
            }
        }
    }
}

void BibleWordSearch::Update(const BibleData& bible)
{
    if (!m_Open) return;

    std::string query = m_Buffer;
    if (query != m_LastQuery)
    {
        m_LastQuery = query;
        RunSearch(bible);
    }
}

bool BibleWordSearch::Render(const BibleData& bible, ImVec2 anchorPos, ImVec2 anchorSize)
{
    if (!m_Open) return false;

    ImVec2 winPos = ImVec2(anchorPos.x, anchorPos.y + anchorSize.y + 4.0f);

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 90.0f), ImVec2(460.0f, 480.0f));
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(6.0f, 3.0f));

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    bool windowOpen = true;
    bool picked     = false;

    if (ImGui::Begin("##BibleWordSearchWin", &windowOpen, kFlags))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
        ImGui::TextUnformatted("  Buscar por palabras");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);
        if (m_NeedsFocus) { ImGui::SetKeyboardFocusHere(); m_NeedsFocus = false; }
        ImGui::InputTextWithHint("##wordSearchInput", "ej: fe esperanza amor", m_Buffer, sizeof(m_Buffer));

        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
        if (m_Buffer[0] == '\0')
            ImGui::TextWrapped("Escribi una o mas palabras del versiculo. Se muestran los que contienen TODAS.");
        else if (m_Hits.empty())
            ImGui::TextUnformatted("Sin coincidencias.");
        else
            ImGui::Text("%d coincidencia%s%s", (int)m_Hits.size(), m_Hits.size() == 1 ? "" : "s",
                        (int)m_Hits.size() >= kMaxHits ? " (mostrando las primeras)" : "");
        ImGui::PopStyleColor();

        ImGui::Separator();

        ImGui::BeginChild("##wordSearchResults", ImVec2(0.0f, 280.0f), false);
        for (int i = 0; i < (int)m_Hits.size(); i++)
        {
            const auto& hit  = m_Hits[i];
            const auto& book = bible.books[hit.bookIdx];
            const auto& chap = book.chapters[hit.chapIdx];
            const auto& verse = chap.verses[hit.verseIdx];

            std::string ref = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
            std::string snippet = verse.text;
            if (snippet.size() > 90) snippet = snippet.substr(0, 90) + "...";

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Header,        ToVec4(ColA(DS::AccentColor, 0)));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::AccentColor));

            bool selected = ImGui::Selectable(ref.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 0.0f));

            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
            ImGui::TextWrapped("%s", snippet.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PopID();

            if (selected)
            {
                m_Resolution = hit;
                picked = true;
                m_Open = false;
            }
        }
        ImGui::EndChild();

        if (!picked
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_Open = false;
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!windowOpen) m_Open = false;

    return picked;
}

}

