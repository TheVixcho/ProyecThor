#include "BibleView.h"
#include "../../../PresentationCore.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    static std::string ToLowerUTF8(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            out += static_cast<char>(std::tolower(c));
        }
        return out;
    }

    static std::string StripAccents(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x80) {
                out += static_cast<char>(c);
                i++;
            } else if (c == 0xC3 && i + 1 < s.size()) {
                unsigned char n = static_cast<unsigned char>(s[i + 1]);
                char rep = '?';
                if (n >= 0xA0 && n <= 0xA5) rep = 'a';
                else if (n >= 0xA8 && n <= 0xAB) rep = 'e';
                else if (n >= 0xAC && n <= 0xAF) rep = 'i';
                else if (n >= 0xB2 && n <= 0xB6) rep = 'o';
                else if (n >= 0xB9 && n <= 0xBC) rep = 'u';
                else if (n == 0xB1) rep = 'n';
                else if (n >= 0x80 && n <= 0x85) rep = 'a';
                else if (n >= 0x88 && n <= 0x8B) rep = 'e';
                else if (n >= 0x8C && n <= 0x8F) rep = 'i';
                else if (n >= 0x92 && n <= 0x96) rep = 'o';
                else if (n >= 0x99 && n <= 0x9C) rep = 'u';
                else if (n == 0x91) rep = 'n';
                else rep = static_cast<char>(n);
                out += rep;
                i += 2;
            } else {
                i++;
            }
        }
        return out;
    }

    BibleSection BibleView::GetBookSection(int n) const {
        if (n >= 1 && n <= 5)   return BibleSection::Pentateuch;
        if (n >= 6 && n <= 17)  return BibleSection::HistoricalOT;
        if (n >= 18 && n <= 22) return BibleSection::Wisdom;
        if (n >= 23 && n <= 27) return BibleSection::MajorProphets;
        if (n >= 28 && n <= 39) return BibleSection::MinorProphets;
        if (n >= 40 && n <= 43) return BibleSection::Gospels;
        if (n == 44)            return BibleSection::Acts;
        if (n >= 45 && n <= 57) return BibleSection::PaulineEpistles;
        if (n >= 58 && n <= 65) return BibleSection::GeneralEpistles;
        return BibleSection::Apocalypse;
    }

    void BibleView::GetSectionColor(BibleSection section, float& r, float& g, float& b) const {
        switch (section) {
            case BibleSection::Pentateuch:       r=0.95f; g=0.75f; b=0.35f; break;
            case BibleSection::HistoricalOT:     r=0.70f; g=0.85f; b=0.55f; break;
            case BibleSection::Wisdom:           r=0.95f; g=0.85f; b=0.40f; break;
            case BibleSection::MajorProphets:    r=0.75f; g=0.55f; b=0.95f; break;
            case BibleSection::MinorProphets:    r=0.60f; g=0.70f; b=0.95f; break;
            case BibleSection::Gospels:          r=0.40f; g=0.85f; b=0.85f; break;
            case BibleSection::Acts:             r=0.55f; g=0.90f; b=0.65f; break;
            case BibleSection::PaulineEpistles:  r=0.95f; g=0.60f; b=0.40f; break;
            case BibleSection::GeneralEpistles:  r=0.90f; g=0.55f; b=0.70f; break;
            case BibleSection::Apocalypse:       r=0.95f; g=0.40f; b=0.40f; break;
            default:                             r=0.80f; g=0.80f; b=0.80f; break;
        }
    }

    void BibleView::LoadXMLBible(const std::string& path) {
        m_CurrentBible = BibleData();
        m_BibleLoaded = false;
        m_LoadedBiblePath = path;

        const char* bookNames[] = {
            "Génesis","Éxodo","Levítico","Números","Deuteronomio",
            "Josué","Jueces","Rut","1 Samuel","2 Samuel","1 Reyes","2 Reyes",
            "1 Crónicas","2 Crónicas","Esdras","Nehemías","Ester","Job","Salmos",
            "Proverbios","Eclesiastés","Cantares","Isaías","Jeremías","Lamentaciones",
            "Ezequiel","Daniel","Oseas","Joel","Amós","Abdías","Jonás","Miqueas",
            "Nahúm","Habacuc","Sofonías","Hageo","Zacarías","Malaquías",
            "Mateo","Marcos","Lucas","Juan","Hechos","Romanos","1 Corintios",
            "2 Corintios","Gálatas","Efesios","Filipenses","Colosenses","1 Tesalonicenses",
            "2 Tesalonicenses","1 Timoteo","2 Timoteo","Tito","Filemón","Hebreos",
            "Santiago","1 Pedro","2 Pedro","1 Juan","2 Juan","3 Juan","Judas","Apocalipsis"
        };

        std::ifstream file(path);
        if (!file.is_open()) return;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string xml = buffer.str();

        size_t bookPos = 0;
        while ((bookPos = xml.find("<book", bookPos)) != std::string::npos) {
            BookData book;
            int bookNum = 0;

            size_t numStart = xml.find("number=\"", bookPos);
            if (numStart != std::string::npos) {
                numStart += 8;
                size_t numEnd = xml.find("\"", numStart);
                try { bookNum = std::stoi(xml.substr(numStart, numEnd - numStart)); } catch(...) {}
            }

            book.canonicalNumber = bookNum;
            if (bookNum >= 1 && bookNum <= 66)
                book.name = bookNames[bookNum - 1];
            else
                book.name = "Libro " + std::to_string(bookNum);

            size_t nextBookPos = xml.find("<book", bookPos + 5);
            if (nextBookPos == std::string::npos) nextBookPos = xml.length();

            size_t chapPos = bookPos;
            while ((chapPos = xml.find("<chapter", chapPos)) != std::string::npos && chapPos < nextBookPos) {
                ChapterData chapter;
                size_t cnumStart = xml.find("number=\"", chapPos);
                if (cnumStart != std::string::npos && cnumStart < nextBookPos) {
                    cnumStart += 8;
                    size_t cnumEnd = xml.find("\"", cnumStart);
                    try { chapter.number = std::stoi(xml.substr(cnumStart, cnumEnd - cnumStart)); } catch(...) {}
                }

                size_t nextChapPos = xml.find("<chapter", chapPos + 8);
                if (nextChapPos == std::string::npos) nextChapPos = nextBookPos;

                size_t versPos = chapPos;
                while ((versPos = xml.find("<verse", versPos)) != std::string::npos && versPos < nextChapPos) {
                    VerseData verse;
                    size_t vnumStart = xml.find("number=\"", versPos);
                    if (vnumStart != std::string::npos && vnumStart < nextChapPos) {
                        vnumStart += 8;
                        size_t vnumEnd = xml.find("\"", vnumStart);
                        try { verse.number = std::stoi(xml.substr(vnumStart, vnumEnd - vnumStart)); } catch(...) {}
                    }

                    size_t textStart = xml.find(">", versPos) + 1;
                    size_t textEnd = xml.find("</verse>", textStart);
                    if (textStart != std::string::npos && textEnd != std::string::npos && textStart < textEnd)
                        verse.text = xml.substr(textStart, textEnd - textStart);

                    chapter.verses.push_back(verse);
                    versPos = textEnd;
                }

                if (!chapter.verses.empty())
                    book.chapters.push_back(chapter);

                chapPos = nextChapPos;
            }

            if (!book.chapters.empty())
                m_CurrentBible.books.push_back(book);

            bookPos = nextBookPos;
        }

        m_CurrentBible.name = fs::path(path).stem().string();
        m_BibleLoaded = !m_CurrentBible.books.empty();
    }

    // Saves the edited verse text back into the XML file on disk.
    // Finds the exact <verse number="N">...</verse> block and replaces its content.
    void BibleView::SaveVerseToXML(int bookIndex, int chapterIndex, int verseNumber, const std::string& newText) {
        if (m_LoadedBiblePath.empty()) return;

        std::ifstream fileIn(m_LoadedBiblePath);
        if (!fileIn.is_open()) return;

        std::stringstream buffer;
        buffer << fileIn.rdbuf();
        fileIn.close();
        std::string xml = buffer.str();

        // We need to locate the correct book → chapter → verse in the raw XML.
        // Strategy: iterate <book> blocks by index, then <chapter> blocks, then find <verse number="N">
        // This mirrors the parsing logic so we hit the same node.

        auto findNthTag = [&](const std::string& tag, size_t startPos, int targetIndex) -> size_t {
            size_t pos = startPos;
            int count = 0;
            while ((pos = xml.find("<" + tag, pos)) != std::string::npos) {
                if (count == targetIndex) return pos;
                count++;
                pos++;
            }
            return std::string::npos;
        };

        size_t bookStart = findNthTag("book", 0, bookIndex);
        if (bookStart == std::string::npos) return;

        size_t nextBookStart = xml.find("<book", bookStart + 5);
        if (nextBookStart == std::string::npos) nextBookStart = xml.length();

        // Find chapter by index within this book's range
        size_t chapSearch = bookStart;
        int chapCount = 0;
        size_t chapStart = std::string::npos;
        while ((chapSearch = xml.find("<chapter", chapSearch)) != std::string::npos && chapSearch < nextBookStart) {
            if (chapCount == chapterIndex) { chapStart = chapSearch; break; }
            chapCount++;
            chapSearch++;
        }
        if (chapStart == std::string::npos) return;

        size_t nextChapStart = xml.find("<chapter", chapStart + 8);
        if (nextChapStart == std::string::npos) nextChapStart = nextBookStart;

        // Find <verse number="verseNumber"> within this chapter's range
        size_t versSearch = chapStart;
        size_t verseTagStart = std::string::npos;
        while ((versSearch = xml.find("<verse", versSearch)) != std::string::npos && versSearch < nextChapStart) {
            // Check the number attribute
            size_t numAttr = xml.find("number=\"", versSearch);
            if (numAttr != std::string::npos && numAttr < versSearch + 50) {
                numAttr += 8;
                size_t numEnd = xml.find("\"", numAttr);
                int vNum = -1;
                try { vNum = std::stoi(xml.substr(numAttr, numEnd - numAttr)); } catch(...) {}
                if (vNum == verseNumber) { verseTagStart = versSearch; break; }
            }
            versSearch++;
        }
        if (verseTagStart == std::string::npos) return;

        size_t textStart = xml.find(">", verseTagStart);
        if (textStart == std::string::npos) return;
        textStart++; // skip '>'

        size_t textEnd = xml.find("</verse>", textStart);
        if (textEnd == std::string::npos) return;

        // Replace old text with new text
        xml.replace(textStart, textEnd - textStart, newText);

        std::ofstream fileOut(m_LoadedBiblePath);
        if (!fileOut.is_open()) return;
        fileOut << xml;
        fileOut.close();

        // Patch in-memory data too so it's immediately reflected
        m_CurrentBible.books[bookIndex].chapters[chapterIndex].verses[verseNumber - 1].text = newText;
        // Note: verse index != verse number in general; find by number
        for (auto& v : m_CurrentBible.books[bookIndex].chapters[chapterIndex].verses) {
            if (v.number == verseNumber) { v.text = newText; break; }
        }
    }

    void BibleView::ParseSearchQuery(const std::string& rawQuery) {
        std::string q = StripAccents(ToLowerUTF8(rawQuery));

        m_FilteredBook = -1;
        m_FilteredChapter = -1;

        if (q.empty()) return;

        std::string bookPart;
        int chapNum = -1;

        auto parseChapterVerse = [&](const std::string& rest) {
            std::string numStr;
            for (char c : rest) {
                if (std::isdigit(static_cast<unsigned char>(c))) numStr += c;
                else if (!numStr.empty()) break;
            }
            if (!numStr.empty()) {
                try { chapNum = std::stoi(numStr); } catch(...) {}
            }
        };

        size_t colonPos = q.find(':');
        size_t spacePos = std::string::npos;

        if (colonPos != std::string::npos) {
            size_t lastSpaceBefore = q.rfind(' ', colonPos);
            if (lastSpaceBefore != std::string::npos) {
                bookPart = q.substr(0, lastSpaceBefore);
                parseChapterVerse(q.substr(lastSpaceBefore + 1));
            } else {
                bookPart = q;
            }
        } else {
            spacePos = q.rfind(' ');
            if (spacePos != std::string::npos) {
                std::string afterSpace = q.substr(spacePos + 1);
                bool allDigits = !afterSpace.empty();
                for (char c : afterSpace)
                    if (!std::isdigit(static_cast<unsigned char>(c))) { allDigits = false; break; }

                if (allDigits) {
                    bookPart = q.substr(0, spacePos);
                    try { chapNum = std::stoi(afterSpace); } catch(...) {}
                } else {
                    bookPart = q;
                }
            } else {
                bookPart = q;
            }
        }

        for (size_t i = 0; i < m_CurrentBible.books.size(); i++) {
            std::string bName = StripAccents(ToLowerUTF8(m_CurrentBible.books[i].name));
            if (bName.find(bookPart) != std::string::npos) {
                m_FilteredBook = static_cast<int>(i);
                if (chapNum > 0 && chapNum <= static_cast<int>(m_CurrentBible.books[i].chapters.size()))
                    m_FilteredChapter = chapNum - 1;
                else
                    m_FilteredChapter = 0;
                break;
            }
        }
    }

    void BibleView::Render() {
        auto selection = Core::PresentationCore::Get().GetSelection();

        if (selection.title != m_LastSelectedFile) {
            m_LastSelectedFile = selection.title;
            LoadXMLBible("assets/bibles/" + selection.title);
            m_SelectedBook = 0;
            m_SelectedChapter = 0;
            m_FilteredBook = -1;
            m_FilteredChapter = -1;
            m_LiveSearch[0] = '\0';
            m_LastLiveSearch = "";
            m_NeedsFocusSearch = true;
        }

        ImGuiIO& io = ImGui::GetIO();

        bool isTypingChar = io.InputQueueCharacters.Size > 0;
        bool isBackspace   = ImGui::IsKeyPressed(ImGuiKey_Backspace, true);
        bool isEscape      = ImGui::IsKeyPressed(ImGuiKey_Escape);

        if (!m_SearchFocused && (isTypingChar || isBackspace)) {
            m_NeedsFocusSearch = true;
        }

        // ─────────────────────────────────────────────────────────────────────
        // TOP BAR: Bible name + search + horizontal history
        // ─────────────────────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 1.0f));
        ImGui::BeginChild("TopBar", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Bible label
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.80f, 1.0f, 1.0f));
        ImGui::SetCursorPosY((40.0f - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::Text("Biblia: %s", m_CurrentBible.name.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 20);

        // Search box
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        ImGui::SetNextItemWidth(300);
        ImGui::SetCursorPosY((40.0f - ImGui::GetFrameHeight()) * 0.5f);

        if (m_NeedsFocusSearch) {
            ImGui::SetKeyboardFocusHere();
            m_NeedsFocusSearch = false;
        }

        ImGui::InputTextWithHint(
            "##liveSearch",
            "Buscar (Libro Abreviado + 1:1)",
            m_LiveSearch,
            sizeof(m_LiveSearch)
        );

        m_SearchFocused = ImGui::IsItemActive();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        if (isEscape && m_SearchFocused) {
            m_LiveSearch[0] = '\0';
        }

        std::string currentSearch(m_LiveSearch);
        if (currentSearch != m_LastLiveSearch) {
            m_LastLiveSearch = currentSearch;
            ParseSearchQuery(currentSearch);
            if (m_FilteredBook >= 0) {
                m_SelectedBook = m_FilteredBook;
                m_SelectedChapter = (m_FilteredChapter >= 0) ? m_FilteredChapter : 0;
            }
        }

        ImGui::SameLine(0, 16);

        // Separator line
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.32f, 0.38f, 1.0f));
        ImGui::SetCursorPosY((40.0f - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::Text("|");
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 12);

        // Clear history button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::SetCursorPosY((40.0f - ImGui::GetFrameHeight()) * 0.5f);
        if (ImGui::Button("✕##clearHistory", ImVec2(26, 26))) m_VerseHistory.clear();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Limpiar historial");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 8);

        // Horizontal history pills — scroll horizontally inside remaining space
        float histAreaX = ImGui::GetCursorPosX();
        float histAreaW = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("HistoryBar", ImVec2(histAreaW, 40), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::SetCursorPosY((40.0f - ImGui::GetFrameHeight()) * 0.5f);
ImGui::Dummy(ImVec2(0.0f, 40.0f)); 
    ImGui::SameLine(); // Volvemos a la misma línea para empezar a dibujar las pills
    // -------------------------
        // Render history items right-to-left (most recent first) horizontally
        float cursorY = (40.0f - ImGui::GetFrameHeight()) * 0.5f;
        for (int i = (int)m_VerseHistory.size() - 1; i >= 0; i--) {
            ImGui::PushID(i);
            std::string ref = m_VerseHistory[i].substr(0, m_VerseHistory[i].find('\n'));

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.18f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.30f, 0.48f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.44f, 0.68f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.75f, 0.88f, 1.0f, 1.0f));

            ImGui::SetCursorPosY(cursorY);
            if (ImGui::Button(ref.c_str())) {
                Core::PresentationCore::Get().SetLayer2_Text(m_VerseHistory[i]);
                Core::PresentationCore::Get().SetProjecting(true);
            }
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, 6);
            ImGui::PopID();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndChild();
        ImGui::PopStyleColor(); // ChildBg transparent

        ImGui::EndChild();      // TopBar
        ImGui::PopStyleColor(); // TopBar ChildBg

        ImGui::Spacing();

        if (!m_BibleLoaded) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("Error: No se pudo cargar el archivo XML de la Biblia.");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Asegurate de que el archivo XML este correctamente estructurado.");
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // MAIN TABLE: Nav | Verses  (history column removed)
        // ─────────────────────────────────────────────────────────────────────
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));
        if (!ImGui::BeginTable("BibleTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::PopStyleVar();
            return;
        }

        ImGui::TableSetupColumn("Navegacion",  ImGuiTableColumnFlags_WidthFixed,   210.0f);
        ImGui::TableSetupColumn("Versiculos",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();

        // COLUMNA 1 — LIBROS Y CAPÍTULOS
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
        ImGui::BeginChild("NavChild", ImVec2(0, 0), false);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.65f, 0.75f, 1.0f));
        ImGui::Text("Libros");
        ImGui::PopStyleColor();

        float booksHeight = ImGui::GetContentRegionAvail().y * 0.62f;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::BeginListBox("##Books", ImVec2(-1, booksHeight))) {
            for (size_t i = 0; i < m_CurrentBible.books.size(); i++) {
                const bool is_selected = (m_SelectedBook == (int)i);
                auto& b = m_CurrentBible.books[i];

                bool isFiltered = (m_FilteredBook < 0) || ((int)i == m_FilteredBook);

                if (!isFiltered) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.32f, 0.35f, 1.0f));
                } else {
                    BibleSection sec = GetBookSection(b.canonicalNumber);
                    float r, g, bv;
                    GetSectionColor(sec, r, g, bv);
                    if (is_selected)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    else
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, bv, 1.0f));
                }

                if (is_selected) {
                    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.18f, 0.28f, 0.45f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.34f, 0.55f, 1.0f));
                }

                if (ImGui::Selectable(b.name.c_str(), is_selected, 0, ImVec2(0, 0))) {
                    m_SelectedBook = (int)i;
                    m_SelectedChapter = 0;
                    m_FilteredBook = (int)i;
                    m_FilteredChapter = 0;
                    m_LiveSearch[0] = '\0';
                    m_LastLiveSearch = "";
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndListBox();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.65f, 0.75f, 1.0f));
        ImGui::Text("Capitulos");
        ImGui::PopStyleColor();

        if (m_SelectedBook >= 0 && m_SelectedBook < (int)m_CurrentBible.books.size()) {
            auto& selBook = m_CurrentBible.books[m_SelectedBook];
            BibleSection sec = GetBookSection(selBook.canonicalNumber);
            float secR, secG, secB;
            GetSectionColor(sec, secR, secG, secB);

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            if (ImGui::BeginListBox("##Chapters", ImVec2(-1, -1))) {
                for (size_t i = 0; i < selBook.chapters.size(); i++) {
                    const bool is_selected = (m_SelectedChapter == (int)i);
                    std::string capLabel = "Cap. " + std::to_string(selBook.chapters[i].number);

                    if (is_selected) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(secR * 0.35f, secG * 0.35f, secB * 0.35f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(secR * 0.45f, secG * 0.45f, secB * 0.45f, 1.0f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(secR * 0.85f, secG * 0.85f, secB * 0.85f, 1.0f));
                    }

                    if (ImGui::Selectable(capLabel.c_str(), is_selected)) {
                        m_SelectedChapter = (int)i;
                        m_FilteredChapter = (int)i;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                        ImGui::PopStyleColor(3);
                    } else {
                        ImGui::PopStyleColor(1);
                    }
                }
                ImGui::EndListBox();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ─────────────────────────────────────────────────────────────────────
        // COLUMNA 2 — VERSÍCULOS
        // ─────────────────────────────────────────────────────────────────────
        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
        ImGui::BeginChild("VersesChild", ImVec2(0, 0), false);

        if (m_SelectedBook >= 0 && m_SelectedBook < (int)m_CurrentBible.books.size() &&
            m_SelectedChapter >= 0 && m_SelectedChapter < (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) {

            auto& book = m_CurrentBible.books[m_SelectedBook];
            auto& chap = book.chapters[m_SelectedChapter];

            BibleSection sec = GetBookSection(book.canonicalNumber);
            float secR, secG, secB;
            GetSectionColor(sec, secR, secG, secB);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(secR, secG, secB, 1.0f));
            ImGui::Text("%s  —  Capitulo %d", book.name.c_str(), chap.number);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(secR * 0.4f, secG * 0.4f, secB * 0.4f, 1.0f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            std::string liveQ = StripAccents(ToLowerUTF8(currentSearch));

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));

            for (auto& verse : chap.verses) {
                ImGui::PushID(verse.number);

                bool verseMatch = true;
                if (!liveQ.empty() && m_FilteredBook >= 0 && m_FilteredChapter >= 0) {
                    size_t colonPos2 = liveQ.find(':');
                    if (colonPos2 != std::string::npos) {
                        std::string afterColon = liveQ.substr(colonPos2 + 1);
                        std::string verseNumStr;
                        for (char c : afterColon)
                            if (std::isdigit(static_cast<unsigned char>(c))) verseNumStr += c;
                            else break;
                        if (!verseNumStr.empty()) {
                            try {
                                int targetVerse = std::stoi(verseNumStr);
                                verseMatch = (verse.number == targetVerse);
                            } catch(...) {}
                        }
                    }
                }

                std::string numStr = std::to_string(verse.number);
                std::string projText = book.name + " " + std::to_string(chap.number) + ":" + numStr + "\n" + verse.text;

                float alpha = verseMatch ? 1.0f : 0.30f;

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.11f, 0.13f, 0.17f, alpha));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(secR * 0.20f, secG * 0.20f, secB * 0.20f, alpha));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(secR * 0.35f, secG * 0.35f, secB * 0.35f, 1.0f));

                float availW = ImGui::GetContentRegionAvail().x;
                float numW = 38.0f;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(secR, secG, secB, alpha));
                ImGui::Text("%s", numStr.c_str());
                ImGui::PopStyleColor();

                ImGui::SameLine(numW);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.88f, 0.90f, alpha));
                ImGui::PushTextWrapPos(availW);

                // LEFT CLICK → project
                if (ImGui::Button(verse.text.c_str(), ImVec2(availW - numW, 0))) {
                    if (m_VerseHistory.empty() || m_VerseHistory.back() != projText)
                        m_VerseHistory.push_back(projText);
                    Core::PresentationCore::Get().SetLayer2_Text(projText);
                    Core::PresentationCore::Get().SetProjecting(true);
                }

                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor(3);

                // ── RIGHT CLICK → context menu to edit verse in XML ──────────
                std::string popupId = "##verseCtx" + std::to_string(verse.number);
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup(popupId.c_str());
                    // Pre-fill edit buffer with current verse text
                    strncpy(m_EditVerseBuffer, verse.text.c_str(), sizeof(m_EditVerseBuffer) - 1);
                    m_EditVerseBuffer[sizeof(m_EditVerseBuffer) - 1] = '\0';
                    m_EditingVerseNumber = verse.number;
                    m_EditingBookIndex   = m_SelectedBook;
                    m_EditingChapIndex   = m_SelectedChapter;
                    m_EditSaveStatus     = "";
                }

                if (ImGui::BeginPopup(popupId.c_str())) {
                    // Header
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(secR, secG, secB, 1.0f));
                    ImGui::Text("Editar  %s %d:%d", book.name.c_str(), chap.number, verse.number);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Multiline text editor
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    ImGui::InputTextMultiline(
                        "##editVerse",
                        m_EditVerseBuffer,
                        sizeof(m_EditVerseBuffer),
                        ImVec2(460, 100),
                        ImGuiInputTextFlags_AllowTabInput
                    );
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    // Save button
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.35f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.50f, 0.28f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    if (ImGui::Button("Guardar en XML", ImVec2(160, 28))) {
                        std::string newText(m_EditVerseBuffer);
                        SaveVerseToXML(m_EditingBookIndex, m_EditingChapIndex, m_EditingVerseNumber, newText);
                        m_EditSaveStatus = "✓ Guardado";
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(2);

                    ImGui::SameLine();

                    // Cancel button
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.20f, 0.20f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    if (ImGui::Button("Cancelar", ImVec2(100, 28)))
                        ImGui::CloseCurrentPopup();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(2);

                    if (!m_EditSaveStatus.empty()) {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.90f, 0.50f, 1.0f));
                        ImGui::Text("%s", m_EditSaveStatus.c_str());
                        ImGui::PopStyleColor();
                    }

                    ImGui::EndPopup();
                }
                // ── end right-click block ─────────────────────────────────────

                ImGui::Spacing();
                ImGui::PopID();
            }

            ImGui::PopStyleVar(2);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

} // namespace ProyecThor::UI
