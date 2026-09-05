#include "BibleView.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "biblia/BibleTextUtils.h"
#include "biblia/BibleBookData.h"
#include "biblia/BibleXmlIO.h"
#include "biblia/BibleSearch.h"
#include "biblia/BibleFavorites.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/ui/DesignSystem.h"
#include "ControlWidgets.h"

namespace ProyecThor::UI {

using TextUtils::Col;

static void DrawPlusBadge(ImDrawList* dl, ImVec2 corner) {
    dl->AddCircleFilled(corner, 6.0f, ImGui::ColorConvertFloat4ToU32(ToVec4(DS::AccentColor)));
    dl->AddLine({ corner.x - 3.0f, corner.y }, { corner.x + 3.0f, corner.y }, IM_COL32(10, 10, 12, 255), 1.4f);
    dl->AddLine({ corner.x, corner.y - 3.0f }, { corner.x, corner.y + 3.0f }, IM_COL32(10, 10, 12, 255), 1.4f);
}

static std::string BuildVerseRef(const BookData& book,
                                  const ChapterData& chap,
                                  const VerseData& verse) {
    return book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
}

static std::string BuildProjectedText(const BookData& book,
                                       const ChapterData& chap,
                                       const VerseData& verse,
                                       const std::string& bibleName) {
    std::string ref = BuildVerseRef(book, chap, verse) + " (" + bibleName + ")";
    return ref + "\n" + verse.text;
}

void BibleView::LoadXMLBible(const std::string& path) {
    m_CurrentBible    = BibleData();
    m_BibleLoaded     = false;
    m_LoadedBiblePath = path;
    m_BibleLoaded     = XmlIO::LoadBible(path, m_CurrentBible);
}

void BibleView::SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx) {
    if (m_LoadedBiblePath.empty()) { m_EditStatus = "Error: ruta desconocida"; return; }
    if (bookIdx  < 0 || bookIdx  >= (int)m_CurrentBible.books.size())
        { m_EditStatus = "Error: libro invalido"; return; }
    auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx  < 0 || chapIdx  >= (int)book.chapters.size())
        { m_EditStatus = "Error: capítulo invalido"; return; }
    auto& chap = book.chapters[chapIdx];
    if (verseIdx < 0 || verseIdx >= (int)chap.verses.size())
        { m_EditStatus = "Error: versiculo invalido"; return; }

    chap.verses[verseIdx].text   = std::string(m_EditBuffer);
    chap.verses[verseIdx].edited = true;

    if (XmlIO::SaveBible(m_LoadedBiblePath, m_CurrentBible))
        m_EditStatus = "Guardado correctamente";
    else
        m_EditStatus = "Error: no se pudo abrir el archivo";
}

void BibleView::ProjectVerse(int bookIdx, int chapIdx, int verseIdx) {
    if (bookIdx < 0 || bookIdx >= (int)m_CurrentBible.books.size()) return;
    auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx < 0 || chapIdx >= (int)book.chapters.size()) return;
    auto& chap = book.chapters[chapIdx];
    if (verseIdx < 0 || verseIdx >= (int)chap.verses.size()) return;
    auto& verse = chap.verses[verseIdx];

    std::string ref      = BuildVerseRef(book, chap, verse);
    std::string body     = verse.text;
    std::string fullText = BuildProjectedText(book, chap, verse, m_CurrentBible.name);

    m_ProjectedBookNum  = book.canonicalNumber;
    m_ProjectedChapNum  = chap.number;
    m_ProjectedVerseNum = verse.number;
    m_ProjectedBookIdx  = bookIdx;
    m_ProjectedChapIdx  = chapIdx;
    m_ProjectedVerseIdx = verseIdx;
    m_ScrollToVerse     = verseIdx;

    bool isDuplicate = !m_History.empty()
        && m_History.back().bookIdx  == bookIdx
        && m_History.back().chapIdx  == chapIdx
        && m_History.back().verseIdx == verseIdx;

    if (!isDuplicate) {
        HistoryEntry entry;
        entry.ref      = ref;
        entry.fullText = fullText;
        entry.body     = body;
        entry.bookIdx  = bookIdx;
        entry.chapIdx  = chapIdx;
        entry.verseIdx = verseIdx;
        m_History.push_back(entry);
        if (m_History.size() > 50)
            m_History.erase(m_History.begin());
    } else {
        m_History.back().fullText = fullText;
        m_History.back().body     = body;
    }

    auto& core = Core::PresentationCore::Get();
    core.SetLayer2_Text(body);
    core.SetCurrentRef(ref);
    core.SetNextText(PeekNextVerseText(bookIdx, chapIdx, verseIdx));
    core.SetProjecting(true);
}

std::string BibleView::PeekNextVerseText(int bookIdx, int chapIdx, int verseIdx) const {
    if (bookIdx < 0 || bookIdx >= (int)m_CurrentBible.books.size()) return "";
    const auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx < 0 || chapIdx >= (int)book.chapters.size()) return "";
    if (verseIdx < 0 || verseIdx >= (int)book.chapters[chapIdx].verses.size()) return "";

    int nextBookIdx  = bookIdx;
    int nextChapIdx  = chapIdx;
    int nextVerseIdx = verseIdx + 1;

    if (nextVerseIdx >= (int)book.chapters[chapIdx].verses.size()) {
        if (chapIdx + 1 < (int)book.chapters.size()) {
            nextChapIdx  = chapIdx + 1;
            nextVerseIdx = 0;
        } else if (bookIdx + 1 < (int)m_CurrentBible.books.size()) {
            nextBookIdx  = bookIdx + 1;
            nextChapIdx  = 0;
            nextVerseIdx = 0;
        } else {
            return "";
        }
    }

    const auto& nextBook = m_CurrentBible.books[nextBookIdx];
    if (nextChapIdx < 0 || nextChapIdx >= (int)nextBook.chapters.size()) return "";
    const auto& nextChap = nextBook.chapters[nextChapIdx];
    if (nextVerseIdx < 0 || nextVerseIdx >= (int)nextChap.verses.size()) return "";

    return BuildProjectedText(nextBook, nextChap, nextChap.verses[nextVerseIdx], m_CurrentBible.name);
}

void BibleView::ReprojectInCurrentBible() {
    if (!m_BibleLoaded || m_ProjectedBookNum < 0) return;
    for (int bi = 0; bi < (int)m_CurrentBible.books.size(); bi++) {
        if (m_CurrentBible.books[bi].canonicalNumber != m_ProjectedBookNum) continue;
        auto& book = m_CurrentBible.books[bi];
        for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
            if (book.chapters[ci].number != m_ProjectedChapNum) continue;
            auto& chap = book.chapters[ci];
            for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
                if (chap.verses[vi].number == m_ProjectedVerseNum)
                    { ProjectVerse(bi, ci, vi); return; }
            }
            if (!chap.verses.empty()) ProjectVerse(bi, ci, (int)chap.verses.size() - 1);
            return;
        }
        break;
    }
}

void BibleView::NavigateVerse(int delta) {
    if (!m_BibleLoaded) return;
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    auto& book     = m_CurrentBible.books[m_SelectedBook];
    int verseCount = (int)book.chapters[m_SelectedChapter].verses.size();
    int newVerse   = m_SelectedVerse + delta;
    if (newVerse >= verseCount) {
        if (m_SelectedChapter + 1 < (int)book.chapters.size()) {
            m_SelectedChapter++;
            m_SelectedVerse = 0;
        }
    } else if (newVerse < 0) {
        if (m_SelectedChapter - 1 >= 0) {
            m_SelectedChapter--;
            m_SelectedVerse = (int)book.chapters[m_SelectedChapter].verses.size() - 1;
        }
    } else {
        m_SelectedVerse = newVerse;
    }
    ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
}

void BibleView::HandleQuickNavConfirm() {
    const QuickNavResolution& res = m_QuickNav.GetResolution();
    if (!res.hasBook) return;

    m_SelectedBook    = res.bookIdx;
    m_SelectedChapter = res.hasChapter ? res.chapterIdx : 0;
    m_SelectedVerse   = res.hasVerse   ? res.verseIdx   : 0;
    m_LiveSearch[0]   = '\0';
    m_FilteredBook    = -1;
    m_FilteredChapter = -1;

    ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
}

void BibleView::HandleWordSearchConfirm() {
    const WordSearchHit& hit = m_WordSearch.GetResolution();
    if (hit.bookIdx < 0) return;

    m_SelectedBook    = hit.bookIdx;
    m_SelectedChapter = hit.chapIdx;
    m_SelectedVerse   = hit.verseIdx;
    m_ScrollToVerse   = hit.verseIdx;
    m_LiveSearch[0]   = '\0';
    m_FilteredBook    = -1;
    m_FilteredChapter = -1;

    ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
}

void BibleView::RenderTopBar() {
    const float barH    = 52.0f;
    const float centerY = (barH - 32.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillTop));
    ImGui::BeginChild("##BibleTopBar", ImVec2(0.0f, barH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::SetCursorPos(ImVec2(12.0f, centerY));
    {
        std::string bName = m_CurrentBible.name.empty() ? "Sin Biblia" : m_CurrentBible.name;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleColor(ImGuiCol_Button, ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
        ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        std::string btnText = "📖 " + bName;
        ImGui::Button(btnText.c_str(), ImVec2(0.0f, 32.0f));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Versión activa: %s\n(Cambia de versión en la Biblioteca)", bName.c_str());

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    ImGui::SameLine(0.0f, 12.0f);

    float availW = ImGui::GetContentRegionAvail().x;
    float rightToolsW = 270.0f;
    if (m_ProjectedBookIdx >= 0) rightToolsW += 160.0f;
    float searchW = std::clamp(availW - rightToolsW, 200.0f, 460.0f);

    ImGui::SetCursorPosY(centerY);
    ImGui::PushItemWidth(searchW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
    ImGui::PushStyleColor(ImGuiCol_Border, m_SearchFocused ? ToVec4(DS::AccentColor) : ToVec4(DS::GlassBorder));

    if (m_NeedsFocusSearch) {
        ImGui::SetKeyboardFocusHere();
        m_NeedsFocusSearch = false;
    }

    bool enterPressed = ImGui::InputTextWithHint("##bibleLiveSearch",
        "🔍 Buscar cita (ej: Jn 3:16, Sal 23, Gén 1)...",
        m_LiveSearch, sizeof(m_LiveSearch),
        ImGuiInputTextFlags_EnterReturnsTrue);

    m_SearchFocused = ImGui::IsItemActive();

    if (m_LiveSearch[0] != '\0') {
        int bIdx = -1, cIdx = -1, vIdx = -1;
        if (Search::ParseSmartQuery(m_CurrentBible, m_LiveSearch, bIdx, cIdx, vIdx)) {
            m_FilteredBook = bIdx;
            m_FilteredChapter = cIdx;
            if (enterPressed && bIdx >= 0) {
                m_SelectedBook = bIdx;
                m_SelectedChapter = (cIdx >= 0) ? cIdx : 0;
                m_SelectedVerse = (vIdx >= 0) ? vIdx : 0;
                m_ScrollToVerse = m_SelectedVerse;
                if (vIdx >= 0) {
                    ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
                }
            }
        } else {
            std::string normQuery = TextUtils::Normalize(m_LiveSearch);
            std::vector<int> candidates = BibleBooks::FindBookCandidates(normQuery);
            if (!candidates.empty()) {
                for (int bi = 0; bi < (int)m_CurrentBible.books.size(); ++bi) {
                    if (m_CurrentBible.books[bi].canonicalNumber == candidates.front()) {
                        m_FilteredBook = bi;
                        break;
                    }
                }
                if (enterPressed && m_FilteredBook >= 0) {
                    m_SelectedBook = m_FilteredBook;
                    m_SelectedChapter = 0;
                    m_SelectedVerse = 0;
                    m_ScrollToVerse = 0;
                }
            } else {
                m_FilteredBook = -1;
            }
            m_FilteredChapter = -1;
        }
    } else {
        m_FilteredBook = -1;
        m_FilteredChapter = -1;
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopItemWidth();

    if (m_LiveSearch[0] != '\0') {
        ImGui::SameLine(0.0f, -26.0f);
        ImGui::SetCursorPosY(centerY + 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
        if (ImGui::Button("✕##clearSearch", ImVec2(20.0f, 24.0f))) {
            m_LiveSearch[0] = '\0';
            m_FilteredBook = -1;
            m_FilteredChapter = -1;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0.0f, 14.0f);
    ImGui::SetCursorPosY(centerY);

    const float iconBtnSize = 32.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button,
        m_WordSearch.IsOpen() ? ToVec4(ColA(DS::AccentColor, 90)) : ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
    ImGui::PushStyleColor(ImGuiCol_Text, m_WordSearch.IsOpen() ? ToVec4(DS::AccentLight) : ToVec4(DS::TextSecondary));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    bool wordSearchClicked = ImGui::Button("🔍 Aa##wordsearch", ImVec2(iconBtnSize + 22.0f, iconBtnSize));
    if (wordSearchClicked && m_BibleLoaded)
        m_WordSearch.Open();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Buscar por palabras en el texto de los versículos (Aa)");

    m_WordSearchBtnPos  = ImGui::GetItemRectMin();
    m_WordSearchBtnSize = ImGui::GetItemRectSize();
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.0f, 6.0f);
    bool hasHistory = !m_History.empty();
    ImGui::PushStyleColor(ImGuiCol_Button,
        m_ShowHistory ? ToVec4(ColA(DS::AccentColor, 90)) : ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_Text, m_ShowHistory ? ToVec4(DS::AccentLight) : (hasHistory ? ToVec4(DS::TextSecondary) : ToVec4(DS::TextHint)));
    bool histClicked = ImGui::Button("🕒##history", ImVec2(iconBtnSize, iconBtnSize));
    if (histClicked && hasHistory)
        m_ShowHistory = !m_ShowHistory;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(hasHistory ? "Historial de proyectados (%d)" : "Historial vacío", (int)m_History.size());

    m_HistoryBtnPos  = ImGui::GetItemRectMin();
    m_HistoryBtnSize = ImGui::GetItemRectSize();
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        m_ShowFavorites ? ToVec4(ColA(DS::AccentColor, 90)) : ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_Text, m_ShowFavorites ? ToVec4(DS::AccentLight) : ToVec4(DS::TextSecondary));
    bool favClicked = ImGui::Button("⭐##favorites", ImVec2(iconBtnSize, iconBtnSize));
    if (favClicked)
        m_ShowFavorites = !m_ShowFavorites;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Versículos favoritos");

    m_FavoritesBtnPos  = ImGui::GetItemRectMin();
    m_FavoritesBtnSize = ImGui::GetItemRectSize();
    ImGui::PopStyleColor(2);

    if (m_ProjectedBookIdx >= 0 && m_ProjectedBookIdx < (int)m_CurrentBible.books.size()) {
        auto& pb = m_CurrentBible.books[m_ProjectedBookIdx];
        if (m_ProjectedChapIdx < (int)pb.chapters.size()) {
            auto& pc = pb.chapters[m_ProjectedChapIdx];
            if (m_ProjectedVerseIdx < (int)pc.verses.size()) {
                ImGui::SameLine(0.0f, 10.0f);
                float r, g, b;
                BibleBooks::GetSectionColor(BibleBooks::GetBookSection(pb.canonicalNumber), r, g, b);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(r*0.20f, g*0.20f, b*0.20f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(r*0.35f, g*0.35f, b*0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(r, g, b, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

                std::string projLabel = "📺 " + pb.name + " " + std::to_string(pc.number) + ":" + std::to_string(pc.verses[m_ProjectedVerseIdx].number);
                if (ImGui::Button(projLabel.c_str(), ImVec2(0.0f, iconBtnSize))) {
                    m_SelectedBook    = m_ProjectedBookIdx;
                    m_SelectedChapter = m_ProjectedChapIdx;
                    m_SelectedVerse   = m_ProjectedVerseIdx;
                    m_ScrollToVerse   = m_ProjectedVerseIdx;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Versículo proyectado en vivo (clic para enfocar)");

                ImGui::PopStyleColor(4);
            }
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (m_ShowHistory)
        RenderHistoryPopup();
    if (m_ShowFavorites)
        RenderFavoritesPopup();

    if (m_WordSearch.IsOpen()) {
        m_WordSearch.Update(m_CurrentBible);
        if (m_WordSearch.Render(m_CurrentBible, m_WordSearchBtnPos, m_WordSearchBtnSize))
            HandleWordSearchConfirm();
    }
}

void BibleView::RenderHistoryPopup() {
    if (!m_ShowHistory || m_History.empty()) return;

    ImVec2 winPos = ImVec2(m_HistoryBtnPos.x,
                           m_HistoryBtnPos.y + m_HistoryBtnSize.y + 4.0f);

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 60.0f), ImVec2(400.0f, 420.0f));
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(6.0f, 3.0f));

    constexpr ImGuiWindowFlags kHistFlags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav;

    bool windowOpen = true;
    bool earlyExit  = false;

    if (ImGui::Begin("##BibleHistoryWin", &windowOpen, kHistFlags)) {

        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
        ImGui::TextUnformatted("  Historial de versiculos");
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::DangerColor));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::SmallButton("Limpiar")) {
            m_History.clear();
            m_ShowHistory = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::Separator();

        for (int i = (int)m_History.size() - 1; i >= 0; i--) {
            auto& entry = m_History[i];
            ImGui::PushID(i);

            bool isActive = (entry.bookIdx  == m_ProjectedBookIdx
                          && entry.chapIdx  == m_ProjectedChapIdx
                          && entry.verseIdx == m_ProjectedVerseIdx);

            ImGui::PushStyleColor(ImGuiCol_Header,
                ToVec4(ColA(DS::AccentColor, isActive ? 90 : 0)));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text,
                isActive ? ToVec4(DS::AccentColor) : ToVec4(DS::TextPrimary));

            bool selected = ImGui::Selectable(entry.ref.c_str(), isActive,
                                              ImGuiSelectableFlags_None, ImVec2(0.0f, 0.0f));
            ImGui::PopStyleColor(3);
            ImGui::PopID();

            if (selected) {
                m_SelectedBook      = entry.bookIdx;
                m_SelectedChapter   = entry.chapIdx;
                m_SelectedVerse     = entry.verseIdx;
                m_ScrollToVerse     = entry.verseIdx;
                m_ProjectedBookIdx  = entry.bookIdx;
                m_ProjectedChapIdx  = entry.chapIdx;
                m_ProjectedVerseIdx = entry.verseIdx;
                Core::PresentationCore::Get().SetLayer2_Text(entry.body);
                Core::PresentationCore::Get().SetCurrentRef(entry.ref);
                Core::PresentationCore::Get().SetProjecting(true);
                m_ShowHistory = false;
                earlyExit = true;
                break;
            }
        }

        if (!earlyExit
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_ShowHistory = false;
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!windowOpen) m_ShowHistory = false;
}

void BibleView::RenderFavoritesPopup() {
    if (!m_ShowFavorites) return;

    ImVec2 winPos = ImVec2(m_FavoritesBtnPos.x,
                           m_FavoritesBtnPos.y + m_FavoritesBtnSize.y + 4.0f);

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 60.0f), ImVec2(420.0f, 460.0f));
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(6.0f, 4.0f));

    constexpr ImGuiWindowFlags kFavFlags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav;

    bool windowOpen = true;
    bool earlyExit  = false;

    if (ImGui::Begin("##BibleFavoritesWin", &windowOpen, kFavFlags)) {

        auto favorites = Favorites::GetAll();

        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
        ImGui::Text("  Versiculos favoritos (%d)", (int)favorites.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (favorites.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
            ImGui::TextWrapped("Todavia no marcaste ningun versiculo. Usa el corazon junto a un versiculo, o click derecho sobre el, para agregarlo aca.");
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < (int)favorites.size(); i++) {
            auto& fv = favorites[i];
            ImGui::PushID(i);

            const char* bookName = BibleBooks::GetCanonicalBookName(fv.bookNum);
            std::string ref = (bookName ? bookName : ("Libro " + std::to_string(fv.bookNum)))
                             + " " + std::to_string(fv.chapterNum) + ":" + std::to_string(fv.verseNum)
                             + " (" + fv.bible + ")";

            float removeW = 22.0f;
            float rowW    = ImGui::GetContentRegionAvail().x - removeW - 4.0f;

            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::TextPrimary));

            bool selected = ImGui::Selectable(("##fav" + std::to_string(i)).c_str(), false,
                                              ImGuiSelectableFlags_None, ImVec2(rowW, 0.0f));
            ImGui::PopStyleColor(3);

            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            ImDrawList* fdl = ImGui::GetWindowDrawList();
            fdl->AddText({ rMin.x + 4.0f, rMin.y + 2.0f }, DS::TextPrimary, ref.c_str());
            std::string snippet = fv.text.size() > 60 ? fv.text.substr(0, 57) + "..." : fv.text;
            fdl->AddText({ rMin.x + 4.0f, rMin.y + 2.0f + ImGui::GetTextLineHeight() },
                         DS::TextSecondary, snippet.c_str());
            (void)rMax;

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.10f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::DangerColor));
            if (ImGui::SmallButton("x")) {
                Favorites::ToggleFavorite(fv.bible, fv.bookNum, fv.chapterNum, fv.verseNum, fv.text);
            }
            ImGui::PopStyleColor(3);

            ImGui::PopID();

            if (selected) {
                JumpToFavorite(fv.bible, fv.bookNum, fv.chapterNum, fv.verseNum);
                m_ShowFavorites = false;
                earlyExit = true;
                break;
            }
        }

        if (!earlyExit
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_ShowFavorites = false;
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!windowOpen) m_ShowFavorites = false;
}

void BibleView::JumpToFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum) {
    if (bible != m_CurrentBible.name)
        LoadXMLBible(BiblesPath() + bible + ".xml");

    for (int bi = 0; bi < (int)m_CurrentBible.books.size(); bi++) {
        if (m_CurrentBible.books[bi].canonicalNumber != bookNum) continue;
        auto& book = m_CurrentBible.books[bi];
        for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
            if (book.chapters[ci].number != chapterNum) continue;
            auto& chap = book.chapters[ci];
            for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
                if (chap.verses[vi].number != verseNum) continue;
                m_SelectedBook    = bi;
                m_SelectedChapter = ci;
                m_SelectedVerse   = vi;
                m_ScrollToVerse   = vi;
                return;
            }
        }
        break;
    }
}

void BibleView::RenderBookGrid() {
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::TextUnformatted("LIBROS");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);

    auto RenderFilterPill = [this](const char* label, int filterIdx) {
        bool sel = (m_TestamentFilter == filterIdx);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ToVec4(ColA(DS::AccentColor, 180)) : ImVec4(1,1,1,0.06f));
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ToVec4(DS::TextSecondary));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
        if (ImGui::Button(label)) {
            m_TestamentFilter = filterIdx;
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    };

    RenderFilterPill("Todos", 0);
    ImGui::SameLine(0.0f, 4.0f);
    RenderFilterPill("AT", 1);
    ImGui::SameLine(0.0f, 4.0f);
    RenderFilterPill("NT", 2);

    ImGui::Spacing();

    ImGui::BeginChild("##BookGrid",
        ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.58f), false);

    const float cellW = 58.0f, cellH = 32.0f, spacing = 6.0f;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cellW + spacing)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(spacing, spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

    const QuickNavResolution& quickRes = m_QuickNav.GetResolution();
    int drawnCol = 0;

    for (int i = 0; i < (int)m_CurrentBible.books.size(); i++) {
        auto& b = m_CurrentBible.books[i];

        if (m_TestamentFilter == 1 && b.canonicalNumber > 39) continue;
        if (m_TestamentFilter == 2 && b.canonicalNumber <= 39) continue;

        float r, g, bv;
        BibleBooks::GetSectionColor(BibleBooks::GetBookSection(b.canonicalNumber), r, g, bv);

        bool selected = (m_SelectedBook == i);
        bool filtered = (m_FilteredBook < 0) || (i == m_FilteredBook);
        bool quickHit = m_QuickNav.IsOpen() && quickRes.hasBook && quickRes.bookIdx == i;
        float alpha   = filtered ? 1.0f : 0.18f;

        ImGui::PushStyleColor(ImGuiCol_Button,
            selected ? ImVec4(r*0.38f, g*0.38f, bv*0.38f, 1.0f)
                     : ImVec4(r*0.10f, g*0.10f, bv*0.10f, alpha));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(r*0.25f, g*0.25f, bv*0.25f, alpha));
        ImGui::PushStyleColor(ImGuiCol_Text,
            selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                     : ImVec4(r, g, bv, alpha));
        ImGui::PushStyleColor(ImGuiCol_Border,
            selected ? ImVec4(r, g, bv, 0.90f) : ImVec4(r, g, bv, 0.35f * alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 1.5f : 1.0f);

        const char* abbrev = BibleBooks::GetBookShortAbbrev(b.canonicalNumber);
        std::string btnLabel = abbrev ? abbrev : TextUtils::Utf8SafeSubstr(b.name, 4);

        ImGui::PushID(i);
        if (ImGui::Button(btnLabel.c_str(), ImVec2(cellW, cellH))) {
            m_SelectedBook    = i;
            m_SelectedChapter = 0;
            m_SelectedVerse   = 0;
            m_LiveSearch[0]   = '\0';
            m_FilteredBook    = -1;
            m_FilteredChapter = -1;
        }
        if (quickHit) {
            ImVec2 bMin = ImGui::GetItemRectMin();
            ImVec2 bMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(bMin, bMax,
                Col(0.55f, 0.95f, 0.65f, 0.90f), 6.0f, 0, 2.0f);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s (%s)", b.name.c_str(), b.canonicalNumber <= 39 ? "Antiguo Testamento" : "Nuevo Testamento");
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        drawnCol++;
        if (drawnCol % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

void BibleView::RenderChapterGrid() {
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    auto& book = m_CurrentBible.books[m_SelectedBook];

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::Text("CAPÍTULOS - %s", book.name.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::BeginChild("##ChapterGrid", ImVec2(0.0f, 0.0f), false);

    const float cellW = 34.0f, cellH = 28.0f, spacing = 6.0f;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cellW + spacing)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(spacing, spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    float r, g, bv;
    BibleBooks::GetSectionColor(BibleBooks::GetBookSection(book.canonicalNumber), r, g, bv);

    for (int i = 0; i < (int)book.chapters.size(); i++) {
        bool selected = (m_SelectedChapter == i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            selected ? ImVec4(r*0.35f, g*0.35f, bv*0.35f, 1.0f)
                     : ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(r*0.20f, g*0.20f, bv*0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ToVec4(DS::TextSecondary));
        ImGui::PushStyleColor(ImGuiCol_Border,
            selected ? ImVec4(r, g, bv, 0.90f) : ToVec4(DS::GlassBorder));

        ImGui::PushID(1000 + i);
        if (ImGui::Button(std::to_string(book.chapters[i].number).c_str(), ImVec2(cellW, cellH))) {
            m_SelectedChapter = i;
            m_SelectedVerse   = 0;
        }
        ImGui::PopID();
        ImGui::PopStyleColor(4);

        if ((i + 1) % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar(3);
    ImGui::EndChild();
}

void BibleView::RenderVerseList() {
    if (m_SelectedBook    < 0 || m_SelectedBook    >= (int)m_CurrentBible.books.size()) return;
    if (m_SelectedChapter < 0 || m_SelectedChapter >= (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) return;

    auto& book = m_CurrentBible.books[m_SelectedBook];
    auto& chap = book.chapters[m_SelectedChapter];

    float r, g, bv;
    BibleBooks::GetSectionColor(BibleBooks::GetBookSection(book.canonicalNumber), r, g, bv);

    const float marginH     = 24.0f;
    const float numColW     = 36.0f;
    const float gap         = 10.0f;
    const float rowPadV     = 8.0f;
    const float availW      = ImGui::GetContentRegionAvail().x;
    const float verseTextW  = availW - marginH * 2.0f - numColW - gap - 28.0f;
    const float textAbsX    = ImGui::GetCursorScreenPos().x + marginH + numColW + gap;

    ImGui::SetCursorPosX(marginH);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, bv, 1.0f));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("📖 %s  Capítulo %d", book.name.c_str(), chap.number);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button, ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (m_SelectedChapter > 0) {
        if (ImGui::Button("◀ Anterior", ImVec2(80.0f, 22.0f))) {
            m_SelectedChapter--;
            m_SelectedVerse = 0;
            m_ScrollToVerse = 0;
        }
        ImGui::SameLine(0.0f, 6.0f);
    }

    if (m_SelectedChapter + 1 < (int)book.chapters.size()) {
        if (ImGui::Button("Siguiente ▶", ImVec2(80.0f, 22.0f))) {
            m_SelectedChapter++;
            m_SelectedVerse = 0;
            m_ScrollToVerse = 0;
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x + marginH, p.y), ImVec2(p.x + ImGui::GetContentRegionAvail().x - marginH, p.y),
            DS::GlassBorder);
    }
    ImGui::Spacing();
    ImGui::Spacing();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)chap.verses.size(); i++) {
        auto& verse = chap.verses[i];
        ImGui::PushID(i);

        ImVec2 textSz = ImGui::CalcTextSize(verse.text.c_str(), nullptr, false, verseTextW);
        float  rowH   = textSz.y + rowPadV * 2.0f;
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + availW, rowMin.y + rowH);

        if (m_ScrollToVerse == i) { ImGui::SetScrollHereY(0.3f); m_ScrollToVerse = -1; }

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##vRow", ImVec2(availW, rowH));
        bool clicked   = ImGui::IsItemClicked();
        bool dblClick  = ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered();
        bool hovered   = ImGui::IsItemHovered();

        bool isFavorite = Favorites::IsFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number);

        if (ImGui::BeginPopupContextItem("verse_ctx")) {
            if (ImGui::MenuItem(isFavorite ? "Quitar de favoritos" : "Agregar a favoritos", nullptr, isFavorite))
                Favorites::ToggleFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number, verse.text);
            ImGui::EndPopup();
        }

        bool isProjected = (i == m_ProjectedVerseIdx
                         && m_SelectedBook    == m_ProjectedBookIdx
                         && m_SelectedChapter == m_ProjectedChapIdx);
        bool isSelected  = (i == m_SelectedVerse);

        if (isProjected && isSelected)
            dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r*0.35f, g*0.35f, bv*0.35f, 0.65f)), 4.0f);
        else if (isProjected)
            dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r*0.20f, g*0.20f, bv*0.20f, 0.50f)), 4.0f);
        else if (isSelected)
            dl->AddRectFilled(rowMin, rowMax, ColA(DS::AccentColor, 100), 4.0f);
        else if (hovered)
            dl->AddRectFilled(rowMin, rowMax, Col(1.0f, 1.0f, 1.0f, 0.04f), 4.0f);

        if (isProjected) {
            dl->AddRectFilled(ImVec2(rowMin.x + 2.0f, rowMin.y + 2.0f),
                              ImVec2(rowMin.x + 6.0f, rowMax.y - 2.0f),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 1.0f)), 2.0f);

            auto it = StyleGeneralApp::Icons.find("izquierda");
            if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
                float iconSize = ImGui::GetFontSize() * 0.85f;
                ImVec2 iconPos = ImVec2(rowMin.x + 8.0f, rowMin.y + rowPadV + 2.0f);

                dl->AddImage(
                    it->second.textureID,
                    iconPos,
                    ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    Col(r, g, bv, 0.95f)
                );
            }
        }

        if (verse.edited)
            dl->AddCircleFilled(ImVec2(rowMin.x + marginH - 4.0f, rowMin.y + rowPadV + 4.0f),
                3.0f, Col(0.95f, 0.72f, 0.20f, 0.90f));

        {
            std::string numStr = std::to_string(verse.number);
            float boxH = 20.0f;
            float boxW = std::max(22.0f, ImGui::CalcTextSize(numStr.c_str()).x + 10.0f);
            ImVec2 boxMin(rowMin.x + marginH, rowMin.y + (rowH - boxH) * 0.5f);
            ImVec2 boxMax(boxMin.x + boxW, boxMin.y + boxH);

            ImU32 boxFill = isProjected
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.30f))
                : DS::BtnDefaultFill;
            ImU32 boxBord = isProjected
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.80f))
                : DS::GlassBorder;

            dl->AddRectFilled(boxMin, boxMax, boxFill, DS::RadiusMedium);
            dl->AddRect(boxMin, boxMax, boxBord, DS::RadiusMedium, 0, 1.0f);

            ImVec2 numSz = ImGui::CalcTextSize(numStr.c_str());
            dl->AddText(ImVec2(boxMin.x + (boxW - numSz.x) * 0.5f, boxMin.y + (boxH - numSz.y) * 0.5f),
                isProjected ? Col(r, g, bv, 1.0f) : DS::TextSecondary,
                numStr.c_str());
        }

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(textAbsX, rowMin.y + rowPadV),
            ColA(DS::TextPrimary, (isSelected || isProjected) ? 255 : 219),
            verse.text.c_str(), nullptr, verseTextW);

        if (hovered || isSelected) {
            float editBtnX = rowMax.x - 26.0f;
            float favBtnX  = editBtnX - 24.0f;
            float btnY     = rowMin.y + (rowH - 20.0f) * 0.5f;

            ImGui::PushStyleColor(ImGuiCol_Button,        ToVec4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::AccentColor));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(3.0f, 2.0f));

            ImGui::SetCursorScreenPos(ImVec2(favBtnX, btnY));
            bool btnFavClicked = ImGui::Button("  ##fav", ImVec2(20.0f, 20.0f));
            ImVec2 favMin = ImGui::GetItemRectMin();
            auto itFavRow = StyleGeneralApp::Icons.find("favorite");
            if (itFavRow != StyleGeneralApp::Icons.end() && itFavRow->second.textureID) {
                float iconSize = 13.0f;
                ImVec2 iconPos = ImVec2(favMin.x + (20.0f - iconSize) * 0.5f, favMin.y + (20.0f - iconSize) * 0.5f);
                ImGui::GetWindowDrawList()->AddImage(
                    itFavRow->second.textureID,
                    iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    isFavorite ? DS::AccentColor : DS::TextHint
                );
                if (!isFavorite)
                    DrawPlusBadge(ImGui::GetWindowDrawList(), ImVec2(favMin.x + 17.0f, favMin.y + 3.0f));
            }
            if (btnFavClicked)
                Favorites::ToggleFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number, verse.text);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(isFavorite ? "Quitar de favoritos" : "Agregar a favoritos");

            ImGui::SetCursorScreenPos(ImVec2(editBtnX, btnY));
            bool btnEditClicked = ImGui::Button("  ##edit", ImVec2(20.0f, 20.0f));

            ImVec2 btnMin = ImGui::GetItemRectMin();
            auto itEdit = StyleGeneralApp::Icons.find("editar");
            if (itEdit != StyleGeneralApp::Icons.end() && itEdit->second.textureID) {
                float iconSize = 14.0f;
                ImVec2 iconPos = ImVec2(btnMin.x + (20.0f - iconSize) * 0.5f, btnMin.y + (20.0f - iconSize) * 0.5f);

                ImGui::GetWindowDrawList()->AddImage(
                    itEdit->second.textureID,
                    iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    DS::TextSecondary
                );
            } else {
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(btnMin.x + 6.0f, btnMin.y + 2.0f),
                    DS::TextPrimary, "E"
                );
            }

            if (btnEditClicked) {
                m_EditBookIdx  = m_SelectedBook;
                m_EditChapIdx  = m_SelectedChapter;
                m_EditVerseIdx = i;
                size_t len = std::min(verse.text.size(), sizeof(m_EditBuffer) - 1);
                memcpy(m_EditBuffer, verse.text.c_str(), len);
                m_EditBuffer[len] = '\0';
                m_EditStatus.clear();
                m_ShowEditModal = true;
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        dl->AddLine(ImVec2(rowMin.x + marginH, rowMax.y),
                    ImVec2(rowMax.x - marginH,  rowMax.y),
                    Col(1.0f, 1.0f, 1.0f, 0.04f));

        if (clicked)  { m_SelectedVerse = i; ProjectVerse(m_SelectedBook, m_SelectedChapter, i); }
        if (dblClick)   ProjectVerse(m_SelectedBook, m_SelectedChapter, i);

        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y));
        ImGui::PopID();
    }
}

void BibleView::RenderEditModal() {
    if (!m_ShowEditModal) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f, 280.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.0f, 14.0f));

    bool open = true;
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("Editar versiculo##editModal", &open, kFlags)) {

        if (m_EditBookIdx  >= 0 && m_EditBookIdx  < (int)m_CurrentBible.books.size()
         && m_EditChapIdx  >= 0 && m_EditChapIdx  < (int)m_CurrentBible.books[m_EditBookIdx].chapters.size()
         && m_EditVerseIdx >= 0 && m_EditVerseIdx < (int)m_CurrentBible.books[m_EditBookIdx].chapters[m_EditChapIdx].verses.size()) {

            auto& b = m_CurrentBible.books[m_EditBookIdx];
            auto& c = b.chapters[m_EditChapIdx];
            auto& v = c.verses[m_EditVerseIdx];

            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
            ImGui::Text("%s %d:%d", b.name.c_str(), c.number, v.number);
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextMultiline("##editVerse", m_EditBuffer, sizeof(m_EditBuffer),
                                  ImVec2(-1.0f, 120.0f));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        if (!m_EditStatus.empty()) {
            bool ok = (m_EditStatus.find("Error") == std::string::npos);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ok ? ToVec4(DS::SuccessColor) : ToVec4(DS::DangerColor));
            ImGui::TextUnformatted(m_EditStatus.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        float btnW = 110.0f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 2.0f - 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("Cancelar", ImVec2(btnW, 32.0f))) {
            m_ShowEditModal = false;
            m_EditStatus.clear();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ToVec4(ColA(DS::AccentColor, 217)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::AccentColorHov));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("Guardar", ImVec2(btnW, 32.0f))) {
            SaveVerseToXML(m_EditBookIdx, m_EditChapIdx, m_EditVerseIdx);
            if (m_EditBookIdx  == m_ProjectedBookIdx
             && m_EditChapIdx  == m_ProjectedChapIdx
             && m_EditVerseIdx == m_ProjectedVerseIdx) {
                ProjectVerse(m_ProjectedBookIdx, m_ProjectedChapIdx, m_ProjectedVerseIdx);
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!open) {
        m_ShowEditModal = false;
        m_EditStatus.clear();
    }
}

void BibleView::Render() {
    auto selection = Core::PresentationCore::Get().PeekSelection();

    if (selection.type == Core::ItemType::Bible && selection.title != m_LastSelectedFile) {
        int oldBookNum = 1, oldChapNum = 1, oldVerseNum = 1;
        if (!m_CurrentBible.books.empty() && m_SelectedBook >= 0
            && m_SelectedBook < (int)m_CurrentBible.books.size()) {
            oldBookNum = m_CurrentBible.books[m_SelectedBook].canonicalNumber;
            if (m_SelectedChapter < (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) {
                oldChapNum = m_CurrentBible.books[m_SelectedBook].chapters[m_SelectedChapter].number;
                auto& vers = m_CurrentBible.books[m_SelectedBook].chapters[m_SelectedChapter].verses;
                if (m_SelectedVerse < (int)vers.size())
                    oldVerseNum = vers[m_SelectedVerse].number;
            }
        }

        m_LastSelectedFile = selection.title;
        if (!selection.title.empty())
            LoadXMLBible(BiblesPath() + selection.title);

        m_SelectedBook = 0; m_SelectedChapter = 0; m_SelectedVerse = 0;
        if (!m_CurrentBible.books.empty()) {
            for (int i = 0; i < (int)m_CurrentBible.books.size(); i++) {
                if (m_CurrentBible.books[i].canonicalNumber != oldBookNum) continue;
                m_SelectedBook = i;
                for (int j = 0; j < (int)m_CurrentBible.books[i].chapters.size(); j++) {
                    if (m_CurrentBible.books[i].chapters[j].number != oldChapNum) continue;
                    m_SelectedChapter = j;
                    for (int k = 0; k < (int)m_CurrentBible.books[i].chapters[j].verses.size(); k++) {
                        if (m_CurrentBible.books[i].chapters[j].verses[k].number == oldVerseNum)
                            { m_SelectedVerse = k; break; }
                    }
                    break;
                }
                break;
            }
            if (!m_CurrentBible.books.empty()) {
                auto& selBook = m_CurrentBible.books[m_SelectedBook];
                if (m_SelectedChapter >= (int)selBook.chapters.size()) m_SelectedChapter = 0;
                if (!selBook.chapters.empty()
                    && m_SelectedVerse >= (int)selBook.chapters[m_SelectedChapter].verses.size())
                    m_SelectedVerse = 0;
            }
        }

        m_ScrollToVerse = m_SelectedVerse;

        ReprojectInCurrentBible();
    }

    if (!m_SearchFocused && !m_QuickNav.IsOpen() && !m_WordSearch.IsOpen() && m_BibleLoaded) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  false)) NavigateVerse(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) NavigateVerse(+1);
    }
 UpdateModifierTaps();

    RenderTopBar();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (ImGui::BeginTable("##BibleLayout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Nav",    ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Verses", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusLarge);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::BeginChild("##NavChild", ImVec2(0.0f, 0.0f), true);
        if (m_BibleLoaded) {
            RenderBookGrid();
            RenderChapterGrid();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
            ImGui::TextWrapped("Selecciona una Biblia en la biblioteca para comenzar.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(ColA(DS::GlassFillTop, 235)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::BeginChild("##VersesChild", ImVec2(0.0f, 0.0f), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (m_BibleLoaded)
            RenderVerseList();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::EndTable();
    }

    RenderEditModal();

     if (m_BibleLoaded && !m_ShowEditModal && m_JumpMode == JumpKind::None) {
        if (m_QuickNav.Update(m_CurrentBible))
            HandleQuickNavConfirm();
        m_QuickNav.Render(m_CurrentBible);
    }

    UpdateJumpOverlay();
    RenderJumpOverlay();
}

void BibleView::UpdateModifierTaps() {
    bool overlaysBlocked = !m_BibleLoaded || m_ShowEditModal || m_QuickNav.IsOpen() || m_WordSearch.IsOpen();

    ImGuiIO& io  = ImGui::GetIO();
    double   now = ImGui::GetTime();
    constexpr double kTapMaxHold = 0.35;

    bool ctrlDown = io.KeyCtrl;
    if (ctrlDown && m_CtrlDownSince < 0.0) {
        m_CtrlDownSince  = now;
        m_CtrlComboFired = false;
    }
    if (ctrlDown && ImGui::IsKeyPressed(ImGuiKey_F, false))
        m_CtrlComboFired = true;

    if (!ctrlDown && m_CtrlDownSince >= 0.0) {
        double heldFor = now - m_CtrlDownSince;
        if (!overlaysBlocked && !m_CtrlComboFired && heldFor < kTapMaxHold) {
            if (m_JumpMode == JumpKind::Chapter) CloseJump();
            else if (m_JumpMode == JumpKind::None) OpenJump(JumpKind::Chapter);
        }
        m_CtrlDownSince = -1.0;
    }

    bool altDown = io.KeyAlt;
    if (altDown && m_AltDownSince < 0.0) {
        m_AltDownSince  = now;
        m_AltComboFired = false;
    }
    if (!altDown && m_AltDownSince >= 0.0) {
        double heldFor = now - m_AltDownSince;
        if (!overlaysBlocked && !m_AltComboFired && heldFor < kTapMaxHold) {
            if (m_JumpMode == JumpKind::Verse) CloseJump();
            else if (m_JumpMode == JumpKind::None) OpenJump(JumpKind::Verse);
        }
        m_AltDownSince = -1.0;
    }
}

void BibleView::OpenJump(JumpKind kind) {
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    if (kind == JumpKind::Verse) {
        auto& book = m_CurrentBible.books[m_SelectedBook];
        if (m_SelectedChapter < 0 || m_SelectedChapter >= (int)book.chapters.size()) return;
    }
    m_JumpMode = kind;
    m_JumpBuffer.clear();
    m_JumpStatus.clear();
}

void BibleView::CloseJump() {
    m_JumpMode = JumpKind::None;
    m_JumpBuffer.clear();
    m_JumpStatus.clear();
}

void BibleView::ConfirmJump() {
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) {
        CloseJump();
        return;
    }
    auto& book = m_CurrentBible.books[m_SelectedBook];

    if (m_JumpMode == JumpKind::Chapter) {
        if (m_JumpBuffer.empty()) { m_JumpStatus = "Escribe un número de capítulo"; return; }
        int chapNum = 0;
        try { chapNum = std::stoi(m_JumpBuffer); }
        catch (...) { m_JumpStatus = "Número invalido"; return; }

        for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
            if (book.chapters[ci].number == chapNum) {
                m_SelectedChapter = ci;
                m_SelectedVerse   = 0;
                CloseJump();
                return;
            }
        }
        m_JumpStatus = "Ese capítulo no existe en " + book.name;
    }
    else if (m_JumpMode == JumpKind::Verse) {
        if (m_SelectedChapter < 0 || m_SelectedChapter >= (int)book.chapters.size()) {
            CloseJump();
            return;
        }
        auto& chap = book.chapters[m_SelectedChapter];
        if (m_JumpBuffer.empty()) { m_JumpStatus = "Escribe un número de versiculo"; return; }
        int verseNum = 0;
        try { verseNum = std::stoi(m_JumpBuffer); }
        catch (...) { m_JumpStatus = "Número invalido"; return; }

        for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
            if (chap.verses[vi].number == verseNum) {
                m_SelectedVerse = vi;
                m_ScrollToVerse = vi;
                ProjectVerse(m_SelectedBook, m_SelectedChapter, vi);
                CloseJump();
                return;
            }
        }
        m_JumpStatus = "Ese versiculo no existe en este capítulo";
    }
}

void BibleView::UpdateJumpOverlay() {
    if (m_JumpMode == JumpKind::None) return;

    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mp = io.MousePos;
        bool inside = mp.x >= m_JumpCardMin.x && mp.x <= m_JumpCardMax.x &&
                      mp.y >= m_JumpCardMin.y && mp.y <= m_JumpCardMax.y;
        if (!inside) { CloseJump(); return; }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && !m_JumpBuffer.empty())
        m_JumpBuffer.pop_back();

    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        ImWchar wc = io.InputQueueCharacters[i];
        if (wc >= '0' && wc <= '9')
            m_JumpBuffer += (char)wc;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        ConfirmJump();
}

void BibleView::RenderJumpOverlay() {
    if (m_JumpMode == JumpKind::None) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 cardSize = ImVec2(300.0f, 150.0f);
    ImVec2 center   = vp->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(cardSize, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(ColA(DS::GlassFillTop, 250)));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(20.0f, 18.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##JumpOverlay", nullptr, kFlags);

    m_JumpCardMin = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    m_JumpCardMax = ImVec2(m_JumpCardMin.x + winSize.x, m_JumpCardMin.y + winSize.y);

    const char* label = (m_JumpMode == JumpKind::Chapter) ? "Ir a capítulo" : "Ir a versiculo";
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(m_JumpBuffer.empty() ? "_" : m_JumpBuffer.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    if (!m_JumpStatus.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::TextUnformatted(m_JumpStatus.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
    ImGui::TextUnformatted(m_JumpMode == JumpKind::Chapter
        ? "Enter: confirmar    Ctrl de nuevo / clic afuera: cerrar"
        : "Enter: confirmar    Alt de nuevo / clic afuera: cerrar");
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
}

