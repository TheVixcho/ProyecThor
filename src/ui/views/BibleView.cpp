#include "BibleView.h"
#include "core/PresentationCore.h"
#include "core/AppPaths.h"
#include "core/FileDeletionManager.h"
#include "core/settings/SettingsManager.h"
#include "biblia/BibleTextUtils.h"
#include "biblia/BibleBookData.h"
#include "biblia/BibleNameUtils.h"
#include "core/io/BibleXmlIO.h"
#include "biblia/BibleSearch.h"
#include "biblia/BibleFavorites.h"
#include "ui/framework/FilePicker.h"
#include "ui/framework/TextEffectsRenderer.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <thread>
#include <unordered_map>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "ui/framework/bin/StyleGeneralApp.h"
#include "ui/framework/DesignSystem.h"
#include "ControlWidgets.h"

namespace ProyecThor::UI {

using TextUtils::Col;

namespace {

std::string PathToUtf8Local(const std::filesystem::path& p) {
#ifdef _WIN32
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

std::filesystem::path U8ToPathLocal(const std::string& u) {
#ifdef _WIN32
    if (u.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), w.data(), n);
    return std::filesystem::path(w);
#else
    return std::filesystem::path(u);
#endif
}

// ── Crisp ImDrawList Vector Icons (zero monochrome emoji artifacts) ─────────
void DrawBookIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float hw = s * 0.48f;
    float hh = s * 0.40f;
    float dip = s * 0.10f;
    // Left page
    ImVec2 lp[4] = {
        { c.x,      c.y - hh + dip },
        { c.x - hw, c.y - hh },
        { c.x - hw, c.y + hh - dip },
        { c.x,      c.y + hh }
    };
    dl->AddPolyline(lp, 4, col, ImDrawFlags_Closed, 1.5f);
    // Right page
    ImVec2 rp[4] = {
        { c.x,      c.y - hh + dip },
        { c.x + hw, c.y - hh },
        { c.x + hw, c.y + hh - dip },
        { c.x,      c.y + hh }
    };
    dl->AddPolyline(rp, 4, col, ImDrawFlags_Closed, 1.5f);
    // Center spine
    dl->AddLine({ c.x, c.y - hh + dip }, { c.x, c.y + hh }, col, 1.4f);
}

void DrawMagnifierIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float r = s * 0.34f;
    ImVec2 cc(c.x - s * 0.08f, c.y - s * 0.08f);
    dl->AddCircle(cc, r, col, 16, 1.6f);
    float d = r * 0.72f;
    dl->AddLine({ cc.x + d, cc.y + d }, { c.x + s * 0.40f, c.y + s * 0.40f }, col, 1.8f);
}

void DrawClockIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float r = s * 0.44f;
    dl->AddCircle(c, r, col, 18, 1.5f);
    dl->AddLine(c, { c.x, c.y - r * 0.56f }, col, 1.5f);
    dl->AddLine(c, { c.x + r * 0.44f, c.y + r * 0.22f }, col, 1.5f);
}

void DrawHeartOrFavIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col, bool filled = false) {
    auto it = StyleGeneralApp::Icons.find("favorite");
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
        float hs = s * 0.5f;
        dl->AddImage(it->second.textureID, { c.x - hs, c.y - hs }, { c.x + hs, c.y + hs },
                     { 0, 0 }, { 1, 1 }, col);
        return;
    }
    // Crisp bookmark ribbon vector fallback
    float hw = s * 0.32f;
    float hh = s * 0.44f;
    ImVec2 pts[5] = {
        { c.x - hw, c.y - hh },
        { c.x + hw, c.y - hh },
        { c.x + hw, c.y + hh },
        { c.x,      c.y + hh * 0.45f },
        { c.x - hw, c.y + hh }
    };
    if (filled)
        dl->AddConvexPolyFilled(pts, 4, col);
    dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, 1.5f);
}

void DrawEditPencilIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    auto it = StyleGeneralApp::Icons.find("editar");
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
        float hs = s * 0.5f;
        dl->AddImage(it->second.textureID, { c.x - hs, c.y - hs }, { c.x + hs, c.y + hs },
                     { 0, 0 }, { 1, 1 }, col);
        return;
    }
    float d = s * 0.36f;
    dl->AddLine({ c.x - d, c.y + d }, { c.x + d, c.y - d }, col, 1.8f);
    dl->AddCircleFilled({ c.x - d, c.y + d }, 1.2f, col);
}

// dir: 0 = left, 1 = right, 2 = down
void DrawChevronIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col, int dir) {
    float h = s * 0.34f;
    float w = s * 0.22f;
    if (dir == 0) {
        ImVec2 pts[3] = { { c.x + w, c.y - h }, { c.x - w, c.y }, { c.x + w, c.y + h } };
        dl->AddPolyline(pts, 3, col, ImDrawFlags_None, 1.7f);
    } else if (dir == 1) {
        ImVec2 pts[3] = { { c.x - w, c.y - h }, { c.x + w, c.y }, { c.x - w, c.y + h } };
        dl->AddPolyline(pts, 3, col, ImDrawFlags_None, 1.7f);
    } else {
        ImVec2 pts[3] = { { c.x - h, c.y - w * 0.7f }, { c.x, c.y + w * 0.9f }, { c.x + h, c.y - w * 0.7f } };
        dl->AddPolyline(pts, 3, col, ImDrawFlags_None, 1.6f);
    }
}

void DrawCloseXIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float d = s * 0.28f;
    dl->AddLine({ c.x - d, c.y - d }, { c.x + d, c.y + d }, col, 1.5f);
    dl->AddLine({ c.x + d, c.y - d }, { c.x - d, c.y + d }, col, 1.5f);
}

void DrawPlusIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float d = s * 0.36f;
    dl->AddLine({ c.x - d, c.y }, { c.x + d, c.y }, col, 1.7f);
    dl->AddLine({ c.x, c.y - d }, { c.x, c.y + d }, col, 1.7f);
}

void DrawTrashIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float hw = s * 0.30f;
    float hh = s * 0.38f;
    // Tapa superior + asa
    dl->AddLine({ c.x - hw * 1.25f, c.y - hh * 0.55f }, { c.x + hw * 1.25f, c.y - hh * 0.55f }, col, 1.4f);
    dl->AddLine({ c.x - hw * 0.48f, c.y - hh * 0.95f }, { c.x + hw * 0.48f, c.y - hh * 0.95f }, col, 1.4f);
    // Cuerpo
    ImVec2 body[4] = {
        { c.x - hw,         c.y - hh * 0.55f },
        { c.x + hw,         c.y - hh * 0.55f },
        { c.x + hw * 0.84f, c.y + hh },
        { c.x - hw * 0.84f, c.y + hh }
    };
    dl->AddPolyline(body, 4, col, ImDrawFlags_Closed, 1.35f);
    dl->AddLine({ c.x, c.y - hh * 0.20f }, { c.x, c.y + hh * 0.68f }, col, 1.2f);
}

void DrawPlusBadge(ImDrawList* dl, ImVec2 corner) {
    dl->AddCircleFilled(corner, 5.5f, ImGui::ColorConvertFloat4ToU32(ToVec4(DS::AccentColor)));
    dl->AddLine({ corner.x - 2.6f, corner.y }, { corner.x + 2.6f, corner.y }, IM_COL32(10, 10, 12, 255), 1.3f);
    dl->AddLine({ corner.x, corner.y - 2.6f }, { corner.x, corner.y + 2.6f }, IM_COL32(10, 10, 12, 255), 1.3f);
}

// Word-wrap text into individual lines using ImGui 1.92 CalcWordWrapPosition so each line can be centered cleanly
std::vector<std::string> WrapTextLines(ImFont* font, float fontSize, const std::string& text, float maxWidth) {
    std::vector<std::string> lines;
    if (!font || text.empty() || maxWidth <= 8.0f) return lines;

    const char* s = text.c_str();
    const char* textEnd = s + text.size();

    while (s < textEnd) {
        const char* wrapEnd = font->CalcWordWrapPosition(fontSize, s, textEnd, maxWidth);
        if (wrapEnd <= s) {
            unsigned char c = (unsigned char)*s;
            int step = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            wrapEnd = std::min(textEnd, s + step);
        }

        const char* trimEnd = wrapEnd;
        while (trimEnd > s && (trimEnd[-1] == ' ' || trimEnd[-1] == '\r' || trimEnd[-1] == '\n'))
            --trimEnd;

        lines.emplace_back(s, trimEnd);

        s = wrapEnd;
        while (s < textEnd && (*s == ' ' || *s == '\r')) ++s;
        if (s < textEnd && *s == '\n') ++s;
    }
    return lines;
}

} // namespace

BibleView::BibleView() {
    auto& gen = Settings::SettingsManager::Get().GetSettings().general;
    m_ClassicListView = gen.bibleClassicListView;
    m_VerseCardZoom   = (gen.bibleVerseCardZoom >= 0.5f && gen.bibleVerseCardZoom <= 2.5f) ? gen.bibleVerseCardZoom : 1.0f;
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
    std::string code = BibleNames::ParseBibleName(bibleName).shortCode;
    std::string ref  = BuildVerseRef(book, chap, verse) + (code.empty() ? "" : (" (" + code + ")"));
    return ref + "\n" + verse.text;
}

void BibleView::RefreshAvailableBibles() {
    m_AvailableBibleFiles.clear();
    try {
        std::filesystem::path dir = U8ToPathLocal(BiblesPath());
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::string fname = PathToUtf8Local(entry.path().filename());
                if (fname.size() > 4) {
                    std::string ext = BibleNames::ToUpperAscii(fname.substr(fname.size() - 4));
                    if (ext == ".XML")
                        m_AvailableBibleFiles.push_back(fname);
                }
            }
            std::sort(m_AvailableBibleFiles.begin(), m_AvailableBibleFiles.end());
        }
    } catch (...) {
    }
}

void BibleView::ImportBibleFiles() {
    if (m_BibleImportRunning.exchange(true))
        return;

    m_ShowVersionPicker  = false;
    m_DeleteBibleMode    = false;
    m_ConfirmDeleteBible.clear();

    const std::string biblesDirUtf8 = BiblesPath();

    std::thread([this, biblesDirUtf8]() {
        std::vector<UI::FileFilterItem> filters = {
            {"Biblias XML (*.xml)", {"*.xml", "*.XML"}},
            {"Todos los archivos",  {"*"}}
        };
        std::vector<std::string> chosen = UI::PickMultipleFiles("Importar Biblia XML", filters);
        std::string lastImported;

        if (!chosen.empty()) {
            std::error_code ec;
            std::filesystem::path destDir = U8ToPathLocal(biblesDirUtf8);
            std::filesystem::create_directories(destDir, ec);

            for (const auto& rawPath : chosen) {
                if (rawPath.empty()) continue;
                std::error_code fileEc;
                std::filesystem::path src = U8ToPathLocal(rawPath);
                if (!std::filesystem::exists(src, fileEc)) {
                    fileEc.clear();
                    src = std::filesystem::path(rawPath);
                    if (!std::filesystem::exists(src, fileEc))
                        continue;
                }
                std::filesystem::path dst = destDir / src.filename();
                std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, fileEc);
                if (!fileEc)
                    lastImported = PathToUtf8Local(src.filename());
            }
        }

        if (!lastImported.empty()) {
            {
                std::lock_guard<std::mutex> lk(m_ImportedBibleMutex);
                m_LastImportedBibleFile = lastImported;
            }
            m_NeedsRefreshBibles = true;
        }
        m_BibleImportRunning = false;
    }).detach();
}

bool BibleView::DeleteBibleFile(const std::string& filename) {
    if (filename.empty()) return false;

    const std::string fullPath = BiblesPath() + filename;
    const bool wasActive = (filename == m_LastSelectedFile) ||
                           (!m_CurrentBible.name.empty() && filename == (m_CurrentBible.name + ".xml")) ||
                           (m_LoadedBiblePath == fullPath);

    if (!Core::FileDeletionManager::ForceDeleteFile(fullPath))
        return false;

    m_ConfirmDeleteBible.clear();
    RefreshAvailableBibles();
    ProyecThor::Library::ForceLibraryRescan() = true;

    if (m_AvailableBibleFiles.empty()) {
        m_DeleteBibleMode      = false;
        m_TriedAutoSelectBible = true;
        m_CurrentBible         = BibleData();
        m_BibleLoaded          = false;
        m_LoadedBiblePath.clear();
        m_LastSelectedFile.clear();

        auto& core = Core::PresentationCore::Get();
        Core::LibrarySelection sel;
        sel.type  = Core::ItemType::Bible;
        sel.title = "";
        core.SetSelection(sel);
        return true;
    }

    if (wasActive) {
        const std::string nextFile = m_AvailableBibleFiles.front();
        Core::LibrarySelection sel;
        sel.type  = Core::ItemType::Bible;
        sel.title = nextFile;
        Core::PresentationCore::Get().SetSelection(sel);
    }

    return true;
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

    std::string nextBody, nextRef;
    std::string prevBody, prevRef;
    PeekNextVerse(bookIdx, chapIdx, verseIdx, nextBody, nextRef);
    PeekPrevVerse(bookIdx, chapIdx, verseIdx, prevBody, prevRef);

    auto& core = Core::PresentationCore::Get();
    core.SetNextContent(nextBody, nextRef);
    core.SetPrevContent(prevBody, prevRef);
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

bool BibleView::PeekNextVerse(int bookIdx, int chapIdx, int verseIdx, std::string& outBody, std::string& outRef) const {
    if (bookIdx < 0 || bookIdx >= (int)m_CurrentBible.books.size()) return false;
    const auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx < 0 || chapIdx >= (int)book.chapters.size()) return false;
    if (verseIdx < 0 || verseIdx >= (int)book.chapters[chapIdx].verses.size()) return false;

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
            return false;
        }
    }

    const auto& nextBook = m_CurrentBible.books[nextBookIdx];
    if (nextChapIdx < 0 || nextChapIdx >= (int)nextBook.chapters.size()) return false;
    const auto& nextChap = nextBook.chapters[nextChapIdx];
    if (nextVerseIdx < 0 || nextVerseIdx >= (int)nextChap.verses.size()) return false;

    outBody = nextChap.verses[nextVerseIdx].text;
    outRef  = BuildVerseRef(nextBook, nextChap, nextChap.verses[nextVerseIdx]);
    return true;
}

bool BibleView::PeekPrevVerse(int bookIdx, int chapIdx, int verseIdx, std::string& outBody, std::string& outRef) const {
    if (bookIdx < 0 || bookIdx >= (int)m_CurrentBible.books.size()) return false;
    const auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx < 0 || chapIdx >= (int)book.chapters.size()) return false;
    if (verseIdx < 0 || verseIdx >= (int)book.chapters[chapIdx].verses.size()) return false;

    int prevBookIdx  = bookIdx;
    int prevChapIdx  = chapIdx;
    int prevVerseIdx = verseIdx - 1;

    if (prevVerseIdx < 0) {
        if (chapIdx - 1 >= 0) {
            prevChapIdx  = chapIdx - 1;
            prevVerseIdx = (int)book.chapters[prevChapIdx].verses.size() - 1;
        } else if (bookIdx - 1 >= 0) {
            prevBookIdx  = bookIdx - 1;
            const auto& pBook = m_CurrentBible.books[prevBookIdx];
            if (pBook.chapters.empty()) return false;
            prevChapIdx  = (int)pBook.chapters.size() - 1;
            prevVerseIdx = (int)pBook.chapters[prevChapIdx].verses.size() - 1;
        } else {
            return false;
        }
    }

    const auto& prevBook = m_CurrentBible.books[prevBookIdx];
    if (prevChapIdx < 0 || prevChapIdx >= (int)prevBook.chapters.size()) return false;
    const auto& prevChap = prevBook.chapters[prevChapIdx];
    if (prevVerseIdx < 0 || prevVerseIdx >= (int)prevChap.verses.size()) return false;

    outBody = prevChap.verses[prevVerseIdx].text;
    outRef  = BuildVerseRef(prevBook, prevChap, prevChap.verses[prevVerseIdx]);
    return true;
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

void BibleView::RenderVersionPopup() {
    if (!m_ShowVersionPicker) return;

    const float popupW = 368.0f;
    ImVec2 winPos(m_VersionBtnPos.x, m_VersionBtnPos.y + m_VersionBtnSize.y + 4.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (winPos.x + popupW > vp->Pos.x + vp->Size.x - 8.0f)
        winPos.x = vp->Pos.x + vp->Size.x - popupW - 8.0f;
    if (winPos.x < vp->Pos.x + 8.0f)
        winPos.x = vp->Pos.x + 8.0f;

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(popupW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 88.0f), ImVec2(410.0f, 430.0f));
    ImGui::SetNextWindowBgAlpha(0.98f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(6.0f, 4.0f));

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    bool windowOpen = true;
    bool picked     = false;

    if (ImGui::Begin("##BibleVersionPickerWin", &windowOpen, kFlags)) {
        ImDrawList* hdrDl = ImGui::GetWindowDrawList();
        const float hdrBtnH   = 25.0f;
        const float importBtnW = 88.0f;
        const float deleteBtnW = 78.0f;
        const float btnGap    = 6.0f;
        const float hdrStartY = ImGui::GetCursorPosY();

        ImGui::SetCursorPosY(hdrStartY + (hdrBtnH - ImGui::GetTextLineHeight()) * 0.5f);
        if (m_DeleteBibleMode) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::DangerColor));
            ImGui::TextUnformatted("  Elige cuál borrar");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
            ImGui::TextUnformatted("  Versión bíblica");
            ImGui::PopStyleColor();
        }

        // Dos botones de gestión en la cabecera del menú: [ + Importar ] y [ Borrar ]
        const float rightBtnsX = ImGui::GetWindowWidth() - 10.0f - (importBtnW + btnGap + deleteBtnW);
        ImGui::SameLine(rightBtnsX);
        ImGui::SetCursorPosY(hdrStartY);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // Botón 1: + Importar
        {
            const bool busy = m_BibleImportRunning.load();
            ImGui::PushStyleColor(ImGuiCol_Button,        ToVec4(ColA(DS::AccentColor, busy ? 20 : 48)));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(ColA(DS::AccentColor, 95)));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ToVec4(ColA(DS::AccentColor, 130)));
            ImGui::PushStyleColor(ImGuiCol_Border,        ToVec4(ColA(DS::AccentColor, 135)));

            if (ImGui::Button("##importBibleBtn", ImVec2(importBtnW, hdrBtnH)) && !busy) {
                ImportBibleFiles();
                picked = true;
            }
            const bool hov = ImGui::IsItemHovered();
            const ImVec2 bMin = ImGui::GetItemRectMin();
            const float  midY = bMin.y + hdrBtnH * 0.5f;
            const ImU32  fg   = hov ? IM_COL32(255, 255, 255, 255) : DS::AccentLight;

            DrawPlusIcon(hdrDl, ImVec2(bMin.x + 13.0f, midY), 11.5f, fg);
            const char* lbl = "Importar";
            const ImVec2 tSz = ImGui::CalcTextSize(lbl);
            hdrDl->AddText(ImVec2(bMin.x + 24.0f, midY - tSz.y * 0.5f), fg, lbl);

            if (hov)
                ImGui::SetTooltip("Importar Biblia XML (.xml)");

            ImGui::PopStyleColor(4);
        }

        ImGui::SameLine(0.0f, btnGap);
        ImGui::SetCursorPosY(hdrStartY);

        // Botón 2: Borrar / Listo
        {
            const bool canDelete = !m_AvailableBibleFiles.empty();
            const ImVec4 bgCol  = m_DeleteBibleMode
                ? ImVec4(0.50f, 0.14f, 0.14f, 0.95f)
                : ImVec4(0.26f, 0.10f, 0.10f, canDelete ? 0.72f : 0.30f);
            const ImVec4 hovCol = m_DeleteBibleMode
                ? ImVec4(0.62f, 0.18f, 0.18f, 1.0f)
                : ImVec4(0.42f, 0.14f, 0.14f, canDelete ? 0.92f : 0.30f);
            const ImVec4 brdCol = m_DeleteBibleMode
                ? ToVec4(DS::DangerColor)
                : ToVec4(ColA(DS::DangerColor, canDelete ? 110 : 45));

            ImGui::PushStyleColor(ImGuiCol_Button,        bgCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  hovCol);
            ImGui::PushStyleColor(ImGuiCol_Border,        brdCol);

            if (ImGui::Button("##deleteBibleModeBtn", ImVec2(deleteBtnW, hdrBtnH)) && canDelete) {
                m_DeleteBibleMode = !m_DeleteBibleMode;
                m_ConfirmDeleteBible.clear();
            }
            const bool hov = ImGui::IsItemHovered();
            const ImVec2 bMin = ImGui::GetItemRectMin();
            const float  midY = bMin.y + hdrBtnH * 0.5f;
            const ImU32  fg   = !canDelete ? DS::TextHint
                              : (m_DeleteBibleMode ? IM_COL32(255, 235, 235, 255) : DS::DangerColor);

            if (m_DeleteBibleMode)
                DrawCloseXIcon(hdrDl, ImVec2(bMin.x + 14.0f, midY), 11.0f, fg);
            else
                DrawTrashIcon(hdrDl, ImVec2(bMin.x + 13.5f, midY), 12.0f, fg);

            const char* lbl = m_DeleteBibleMode ? "Listo" : "Borrar";
            const ImVec2 tSz = ImGui::CalcTextSize(lbl);
            hdrDl->AddText(ImVec2(bMin.x + 25.0f, midY - tSz.y * 0.5f), fg, lbl);

            if (hov) {
                ImGui::SetTooltip("%s", !canDelete
                    ? "No hay Biblias para borrar"
                    : (m_DeleteBibleMode ? "Salir del modo borrar" : "Seleccionar y borrar Biblias de la lista"));
            }

            ImGui::PopStyleColor(4);
        }

        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::Separator();

        if (m_AvailableBibleFiles.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
            ImGui::TextWrapped("No hay Biblias instaladas. Usa el botón \"+ Importar\" de arriba para añadir un archivo XML.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        } else {
            // Detectar nombres visibles duplicados para mostrar el nombre de archivo y distinguirlos
            std::unordered_map<std::string, int> displayCounts;
            for (const auto& f : m_AvailableBibleFiles) {
                auto inf = BibleNames::ParseBibleName(f);
                displayCounts[inf.langCode + "|" + inf.shortCode + "|" + inf.fullTitle]++;
            }

            const float rowH  = 31.0f;
            const float listH = std::min((float)m_AvailableBibleFiles.size() * (rowH + 4.0f) + 2.0f, 340.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("##BibleVerListScroll", ImVec2(0.0f, listH), false);

            std::string fileToDeleteNow;

            for (int i = 0; i < (int)m_AvailableBibleFiles.size(); i++) {
                const std::string& fname = m_AvailableBibleFiles[i];
                BibleNames::BibleDisplayInfo info = BibleNames::ParseBibleName(fname);
                const bool isCurrent = (fname == m_LastSelectedFile) ||
                                       (!m_CurrentBible.name.empty() && fname == (m_CurrentBible.name + ".xml"));
                const bool isConfirmingDelete = m_DeleteBibleMode && (m_ConfirmDeleteBible == fname);
                const std::string dupKey = info.langCode + "|" + info.shortCode + "|" + info.fullTitle;
                const bool hasDuplicateLabel = (displayCounts[dupKey] > 1);

                ImGui::PushID(i);
                const float rowW = ImGui::GetContentRegionAvail().x;
                const ImVec2 rMin = ImGui::GetCursorScreenPos();
                const ImVec2 rMax(rMin.x + rowW, rMin.y + rowH);

                const bool clicked = ImGui::InvisibleButton("##verRow", ImVec2(rowW, rowH));
                const bool hovered = ImGui::IsItemHovered();

                ImDrawList* dl = ImGui::GetWindowDrawList();
                if (m_DeleteBibleMode) {
                    if (isConfirmingDelete) {
                        dl->AddRectFilled(rMin, rMax, IM_COL32(145, 32, 32, 130), 6.0f);
                        dl->AddRect(rMin, rMax, DS::DangerColor, 6.0f, 0, 1.2f);
                    } else if (hovered) {
                        dl->AddRectFilled(rMin, rMax, IM_COL32(120, 30, 30, 85), 6.0f);
                        dl->AddRect(rMin, rMax, ColA(DS::DangerColor, 120), 6.0f, 0, 1.0f);
                    } else if (isCurrent) {
                        dl->AddRectFilled(rMin, rMax, ColA(DS::AccentColor, 32), 6.0f);
                    }
                } else {
                    if (isCurrent) {
                        dl->AddRectFilled(rMin, rMax, ColA(DS::AccentColor, 55), 6.0f);
                        dl->AddRect(rMin, rMax, ColA(DS::AccentColor, 150), 6.0f, 0, 1.0f);
                    } else if (hovered) {
                        dl->AddRectFilled(rMin, rMax, DS::BtnHoverFill, 6.0f);
                    }
                }

                float curX = rMin.x + 8.0f;
                const float textY = rMin.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;

                if (!info.langCode.empty()) {
                    ImVec2 lSz = ImGui::CalcTextSize(info.langCode.c_str());
                    ImVec2 bMin(curX, rMin.y + (rowH - 18.0f) * 0.5f);
                    ImVec2 bMax(curX + lSz.x + 10.0f, bMin.y + 18.0f);
                    dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 18), 4.0f);
                    dl->AddText(ImVec2(curX + 5.0f, bMin.y + (18.0f - lSz.y) * 0.5f),
                                isCurrent ? DS::AccentLight : DS::TextSecondary, info.langCode.c_str());
                    curX = bMax.x + 8.0f;
                }

                dl->AddText(ImVec2(curX, textY), isCurrent ? IM_COL32(255, 255, 255, 255) : DS::TextPrimary,
                            info.shortCode.c_str());
                curX += ImGui::CalcTextSize(info.shortCode.c_str()).x + 8.0f;

                float rightClipX = rMax.x - 8.0f;
                if (m_DeleteBibleMode) {
                    const char* pillTxt = isConfirmingDelete ? "Confirmar" : "Borrar";
                    const ImVec2 pSz = ImGui::CalcTextSize(pillTxt);
                    const float pillW = pSz.x + (isConfirmingDelete ? 16.0f : 26.0f);
                    const float pillH = 21.0f;
                    const ImVec2 pMin(rMax.x - pillW - 6.0f, rMin.y + (rowH - pillH) * 0.5f);
                    const ImVec2 pMax(rMax.x - 6.0f, pMin.y + pillH);
                    rightClipX = pMin.x - 6.0f;

                    dl->AddRectFilled(pMin, pMax,
                                      isConfirmingDelete ? IM_COL32(215, 50, 50, 235) : IM_COL32(110, 28, 28, 175), 5.0f);
                    dl->AddRect(pMin, pMax,
                                isConfirmingDelete ? IM_COL32(255, 140, 140, 240) : ColA(DS::DangerColor, 140), 5.0f, 0, 1.0f);

                    const ImU32 pFg = isConfirmingDelete ? IM_COL32(255, 255, 255, 255) : DS::DangerColor;
                    if (isConfirmingDelete) {
                        dl->AddText(ImVec2(pMin.x + (pillW - pSz.x) * 0.5f, pMin.y + (pillH - pSz.y) * 0.5f), pFg, pillTxt);
                    } else {
                        DrawTrashIcon(dl, ImVec2(pMin.x + 10.0f, (pMin.y + pMax.y) * 0.5f), 10.5f, pFg);
                        dl->AddText(ImVec2(pMin.x + 19.0f, pMin.y + (pillH - pSz.y) * 0.5f), pFg, pillTxt);
                    }
                }

                std::string subtitle = (info.fullTitle != info.shortCode) ? info.fullTitle : "";
                if (hasDuplicateLabel || m_DeleteBibleMode) {
                    std::string stem = (fname.size() > 4) ? fname.substr(0, fname.size() - 4) : fname;
                    if (stem != info.shortCode && stem != info.fullTitle) {
                        subtitle = subtitle.empty() ? ("[" + stem + "]") : (subtitle + " · " + stem);
                    }
                }

                if (!subtitle.empty() && rightClipX > curX + 12.0f) {
                    dl->PushClipRect(ImVec2(curX, rMin.y), ImVec2(rightClipX, rMax.y), true);
                    dl->AddText(ImVec2(curX, textY), DS::TextSecondary, subtitle.c_str());
                    dl->PopClipRect();
                }

                if (hovered && (m_DeleteBibleMode || hasDuplicateLabel)) {
                    ImGui::SetTooltip("Archivo: %s%s",
                                      fname.c_str(),
                                      isConfirmingDelete ? "\nHaz clic de nuevo para eliminar definitivamente" : "");
                }

                ImGui::PopID();

                if (clicked) {
                    if (m_DeleteBibleMode) {
                        if (isConfirmingDelete) {
                            fileToDeleteNow = fname;
                        } else {
                            m_ConfirmDeleteBible = fname;
                        }
                    } else {
                        Core::LibrarySelection sel;
                        sel.type  = Core::ItemType::Bible;
                        sel.title = fname;
                        Core::PresentationCore::Get().SetSelection(sel);
                        m_ShowVersionPicker = false;
                        m_DeleteBibleMode   = false;
                        m_ConfirmDeleteBible.clear();
                        picked = true;
                        break;
                    }
                }
            }

            if (!fileToDeleteNow.empty()) {
                DeleteBibleFile(fileToDeleteNow);
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        if (!picked
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            bool onBtn = mp.x >= m_VersionBtnPos.x && mp.x <= (m_VersionBtnPos.x + m_VersionBtnSize.x)
                      && mp.y >= m_VersionBtnPos.y && mp.y <= (m_VersionBtnPos.y + m_VersionBtnSize.y);
            if (!onBtn) {
                m_ShowVersionPicker = false;
                m_DeleteBibleMode   = false;
                m_ConfirmDeleteBible.clear();
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!windowOpen) {
        m_ShowVersionPicker = false;
        m_DeleteBibleMode   = false;
        m_ConfirmDeleteBible.clear();
    }
}

void BibleView::RenderTopBar() {
    const float barH    = 48.0f;
    const float btnH    = 32.0f;
    const float centerY = (barH - btnH) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,  ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("##BibleTopBar", ImVec2(0.0f, barH), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float totalBarW = ImGui::GetWindowWidth();

    // 1. Selector interactivo de versión bíblica
    ImGui::SetCursorPos(ImVec2(10.0f, centerY));
    {
        BibleNames::BibleDisplayInfo bInfo = BibleNames::ParseBibleName(m_CurrentBible.name);
        std::string codeLabel = m_BibleLoaded ? bInfo.shortCode : "Seleccionar Biblia";
        bool showFullName = m_BibleLoaded && (totalBarW > 760.0f) && (bInfo.fullTitle != bInfo.shortCode);

        float codeW = ImGui::CalcTextSize(codeLabel.c_str()).x;
        float fullW = showFullName ? (ImGui::CalcTextSize(bInfo.fullTitle.c_str()).x + 8.0f) : 0.0f;
        float langW = (!bInfo.langCode.empty() && m_BibleLoaded) ? (ImGui::CalcTextSize(bInfo.langCode.c_str()).x + 12.0f) : 0.0f;
        float verBtnW = 30.0f + codeW + fullW + langW + 22.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, m_ShowVersionPicker ? ToVec4(ColA(DS::AccentColor, 65)) : ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
        ImGui::PushStyleColor(ImGuiCol_Border, m_ShowVersionPicker ? ToVec4(DS::AccentColor) : ToVec4(DS::GlassBorder));

        if (ImGui::Button("##bibleVersionBtn", ImVec2(verBtnW, btnH))) {
            m_ShowVersionPicker = !m_ShowVersionPicker;
            m_DeleteBibleMode   = false;
            m_ConfirmDeleteBible.clear();
            if (m_ShowVersionPicker)
                RefreshAvailableBibles();
        }
        bool verHov = ImGui::IsItemHovered();
        m_VersionBtnPos  = ImGui::GetItemRectMin();
        m_VersionBtnSize = ImGui::GetItemRectSize();

        float midY = m_VersionBtnPos.y + btnH * 0.5f;
        float textY = m_VersionBtnPos.y + (btnH - ImGui::GetTextLineHeight()) * 0.5f;
        DrawBookIcon(dl, ImVec2(m_VersionBtnPos.x + 15.0f, midY), 15.0f, DS::AccentLight);

        float cx = m_VersionBtnPos.x + 28.0f;
        dl->AddText(ImVec2(cx, textY), IM_COL32(242, 244, 250, 255), codeLabel.c_str());
        cx += codeW + 6.0f;

        if (showFullName) {
            dl->AddText(ImVec2(cx, textY), DS::TextSecondary, bInfo.fullTitle.c_str());
            cx += ImGui::CalcTextSize(bInfo.fullTitle.c_str()).x + 6.0f;
        }

        if (!bInfo.langCode.empty() && m_BibleLoaded) {
            ImVec2 lSz = ImGui::CalcTextSize(bInfo.langCode.c_str());
            ImVec2 p0(cx, midY - 8.5f);
            ImVec2 p1(cx + lSz.x + 8.0f, midY + 8.5f);
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18), 3.5f);
            dl->AddText(ImVec2(cx + 4.0f, midY - lSz.y * 0.5f), DS::AccentLight, bInfo.langCode.c_str());
        }

        DrawChevronIcon(dl, ImVec2(m_VersionBtnPos.x + verBtnW - 11.0f, midY), 11.0f,
                        verHov ? DS::TextPrimary : DS::TextSecondary, 2);

        if (verHov) {
            ImGui::SetTooltip("Versión activa: %s (%s)\nClic para cambiar rápidamente de traducción",
                              bInfo.fullTitle.c_str(),
                              m_CurrentBible.name.empty() ? "ninguna" : m_CurrentBible.name.c_str());
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    ImGui::SameLine(0.0f, 10.0f);

    // Calcular espacio requerido a la derecha para que nunca se desborde
    bool hasProjectedPill = false;
    std::string projRefText;
    float projR = 0.4f, projG = 0.6f, projB = 1.0f;
    if (m_ProjectedBookIdx >= 0 && m_ProjectedBookIdx < (int)m_CurrentBible.books.size()) {
        auto& pb = m_CurrentBible.books[m_ProjectedBookIdx];
        if (m_ProjectedChapIdx >= 0 && m_ProjectedChapIdx < (int)pb.chapters.size()) {
            auto& pc = pb.chapters[m_ProjectedChapIdx];
            if (m_ProjectedVerseIdx >= 0 && m_ProjectedVerseIdx < (int)pc.verses.size()) {
                hasProjectedPill = true;
                projRefText = pb.name + " " + std::to_string(pc.number) + ":" + std::to_string(pc.verses[m_ProjectedVerseIdx].number);
                BibleBooks::GetSectionColor(BibleBooks::GetBookSection(pb.canonicalNumber), projR, projG, projB);
            }
        }
    }

    float projPillW = hasProjectedPill ? (ImGui::CalcTextSize(projRefText.c_str()).x + 32.0f) : 0.0f;
    float histBtnW  = m_History.empty() ? btnH : (btnH + 18.0f);
    float rightGroupW = 54.0f + 6.0f + histBtnW + 6.0f + btnH + (hasProjectedPill ? (8.0f + projPillW) : 0.0f) + 12.0f;

    float availForSearch = ImGui::GetContentRegionAvail().x - rightGroupW - 10.0f;
    float searchW = std::clamp(availForSearch, 140.0f, 440.0f);

    // 2. Buscador inteligente de citas con ícono vectorial integrado
    ImGui::SetCursorPosY(centerY);
    ImGui::PushItemWidth(searchW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(30.0f, (btnH - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.065f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, m_SearchFocused ? ToVec4(DS::AccentColor) : ToVec4(DS::GlassBorder));

    if (m_NeedsFocusSearch) {
        ImGui::SetKeyboardFocusHere();
        m_NeedsFocusSearch = false;
    }

    bool enterPressed = ImGui::InputTextWithHint("##bibleLiveSearch",
        "Buscar cita (ej: Jn 3:16, Sal 23, Rom 8)...",
        m_LiveSearch, sizeof(m_LiveSearch),
        ImGuiInputTextFlags_EnterReturnsTrue);

    m_SearchFocused = ImGui::IsItemActive();
    ImVec2 sMin = ImGui::GetItemRectMin();
    ImVec2 sMax = ImGui::GetItemRectMax();

    DrawMagnifierIcon(dl, ImVec2(sMin.x + 15.0f, (sMin.y + sMax.y) * 0.5f), 13.5f,
                      m_SearchFocused ? DS::AccentLight : DS::TextHint);

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

    // Botón limpiar dentro de la caja de búsqueda o insignia Ctrl+F cuando está vacía
    if (m_LiveSearch[0] != '\0') {
        ImGui::SameLine(0.0f, -26.0f);
        ImGui::SetCursorPosY(centerY + 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("##clearSearch", ImVec2(22.0f, 24.0f))) {
            m_LiveSearch[0] = '\0';
            m_FilteredBook = -1;
            m_FilteredChapter = -1;
        }
        ImVec2 cMin = ImGui::GetItemRectMin();
        ImVec2 cMax = ImGui::GetItemRectMax();
        DrawCloseXIcon(dl, ImVec2((cMin.x + cMax.x) * 0.5f, (cMin.y + cMax.y) * 0.5f), 11.0f,
                       ImGui::IsItemHovered() ? DS::TextPrimary : DS::TextHint);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    } else if (searchW > 290.0f && !m_SearchFocused) {
        const char* hk = "Ctrl+F";
        ImVec2 hkSz = ImGui::CalcTextSize(hk);
        ImVec2 hkMin(sMax.x - hkSz.x - 14.0f, (sMin.y + sMax.y - 18.0f) * 0.5f);
        ImVec2 hkMax(sMax.x - 6.0f, hkMin.y + 18.0f);
        dl->AddRectFilled(hkMin, hkMax, IM_COL32(255, 255, 255, 12), 4.0f);
        dl->AddRect(hkMin, hkMax, IM_COL32(255, 255, 255, 22), 4.0f, 0, 1.0f);
        dl->AddText(ImVec2(hkMin.x + 4.0f, (hkMin.y + hkMax.y - hkSz.y) * 0.5f), DS::TextHint, hk);
    }

    // 3. Herramientas alineadas limpiamente a la derecha
    float rightStartX = std::max(ImGui::GetCursorPosX() + 8.0f, totalBarW - rightGroupW);
    ImGui::SameLine(rightStartX);
    ImGui::SetCursorPosY(centerY);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));

    // Botón Buscar por palabras (Aa)
    {
        bool active = m_WordSearch.IsOpen();
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ToVec4(ColA(DS::AccentColor, 85)) : ToVec4(DS::BtnDefaultFill));
        bool clicked = ImGui::Button("##wordsearch", ImVec2(54.0f, btnH));
        if (clicked && m_BibleLoaded)
            m_WordSearch.Open();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Buscar palabras dentro de los versículos");

        m_WordSearchBtnPos  = ImGui::GetItemRectMin();
        m_WordSearchBtnSize = ImGui::GetItemRectSize();
        ImU32 icCol = active ? DS::AccentLight : DS::TextSecondary;
        float midY = m_WordSearchBtnPos.y + btnH * 0.5f;
        DrawMagnifierIcon(dl, ImVec2(m_WordSearchBtnPos.x + 15.0f, midY), 13.0f, icCol);
        ImVec2 aaSz = ImGui::CalcTextSize("Aa");
        dl->AddText(ImVec2(m_WordSearchBtnPos.x + 27.0f, midY - aaSz.y * 0.5f), icCol, "Aa");
        ImGui::PopStyleColor();
    }

    // Botón Historial
    ImGui::SameLine(0.0f, 6.0f);
    {
        bool hasHistory = !m_History.empty();
        bool active = m_ShowHistory;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ToVec4(ColA(DS::AccentColor, 85)) : ToVec4(DS::BtnDefaultFill));
        bool clicked = ImGui::Button("##history", ImVec2(histBtnW, btnH));
        if (clicked && hasHistory)
            m_ShowHistory = !m_ShowHistory;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(hasHistory ? "Historial de versículos proyectados (%d)" : "Historial vacío", (int)m_History.size());

        m_HistoryBtnPos  = ImGui::GetItemRectMin();
        m_HistoryBtnSize = ImGui::GetItemRectSize();
        ImU32 icCol = active ? DS::AccentLight : (hasHistory ? DS::TextSecondary : DS::TextHint);
        float midY = m_HistoryBtnPos.y + btnH * 0.5f;
        if (hasHistory) {
            DrawClockIcon(dl, ImVec2(m_HistoryBtnPos.x + 14.0f, midY), 13.5f, icCol);
            std::string cnt = std::to_string(m_History.size());
            ImVec2 cSz = ImGui::CalcTextSize(cnt.c_str());
            dl->AddText(ImVec2(m_HistoryBtnPos.x + 26.0f, midY - cSz.y * 0.5f), DS::AccentLight, cnt.c_str());
        } else {
            DrawClockIcon(dl, ImVec2(m_HistoryBtnPos.x + histBtnW * 0.5f, midY), 14.0f, icCol);
        }
        ImGui::PopStyleColor();
    }

    // Botón Favoritos
    ImGui::SameLine(0.0f, 6.0f);
    {
        bool active = m_ShowFavorites;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ToVec4(ColA(DS::AccentColor, 85)) : ToVec4(DS::BtnDefaultFill));
        if (ImGui::Button("##favorites", ImVec2(btnH, btnH)))
            m_ShowFavorites = !m_ShowFavorites;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Versículos favoritos");

        m_FavoritesBtnPos  = ImGui::GetItemRectMin();
        m_FavoritesBtnSize = ImGui::GetItemRectSize();
        DrawHeartOrFavIcon(dl,
            ImVec2(m_FavoritesBtnPos.x + btnH * 0.5f, m_FavoritesBtnPos.y + btnH * 0.5f),
            14.5f, active ? DS::AccentLight : DS::TextSecondary, active);
        ImGui::PopStyleColor();
    }

    // Indicador de Versículo en Vivo (On-Air Pill)
    if (hasProjectedPill) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(projR * 0.22f + 0.06f, projG * 0.22f + 0.06f, projB * 0.22f + 0.06f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(projR * 0.34f + 0.08f, projG * 0.34f + 0.08f, projB * 0.34f + 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(projR, projG, projB, 0.80f));

        if (ImGui::Button("##projectedPill", ImVec2(projPillW, btnH))) {
            m_SelectedBook    = m_ProjectedBookIdx;
            m_SelectedChapter = m_ProjectedChapIdx;
            m_SelectedVerse   = m_ProjectedVerseIdx;
            m_ScrollToVerse   = m_ProjectedVerseIdx;
        }
        ImVec2 pMin = ImGui::GetItemRectMin();
        float midY = pMin.y + btnH * 0.5f;
        dl->AddCircleFilled(ImVec2(pMin.x + 12.0f, midY), 4.0f, IM_COL32(245, 75, 75, 255));
        dl->AddCircle(ImVec2(pMin.x + 12.0f, midY), 6.0f, IM_COL32(245, 75, 75, 95), 12, 1.0f);
        ImVec2 tSz = ImGui::CalcTextSize(projRefText.c_str());
        dl->AddText(ImVec2(pMin.x + 22.0f, midY - tSz.y * 0.5f), IM_COL32(255, 255, 255, 255), projRefText.c_str());

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Proyectando en vivo: %s\n(Clic para ir a este versículo)", projRefText.c_str());

        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (m_ShowVersionPicker)
        RenderVersionPopup();
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

    ImVec2 winPos = ImVec2(m_HistoryBtnPos.x - 160.0f,
                           m_HistoryBtnPos.y + m_HistoryBtnSize.y + 4.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (winPos.x + 320.0f > vp->Pos.x + vp->Size.x - 8.0f)
        winPos.x = vp->Pos.x + vp->Size.x - 328.0f;
    if (winPos.x < vp->Pos.x + 8.0f)
        winPos.x = vp->Pos.x + 8.0f;

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 60.0f), ImVec2(400.0f, 420.0f));
    ImGui::SetNextWindowBgAlpha(0.98f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(8.0f, 8.0f));
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

        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextUnformatted("  Historial reciente");
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 54.0f);
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
                isActive ? ToVec4(DS::AccentLight) : ToVec4(DS::TextPrimary));

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

    ImVec2 winPos = ImVec2(m_FavoritesBtnPos.x - 220.0f,
                           m_FavoritesBtnPos.y + m_FavoritesBtnSize.y + 4.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (winPos.x + 340.0f > vp->Pos.x + vp->Size.x - 8.0f)
        winPos.x = vp->Pos.x + vp->Size.x - 348.0f;
    if (winPos.x < vp->Pos.x + 8.0f)
        winPos.x = vp->Pos.x + 8.0f;

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 60.0f), ImVec2(420.0f, 460.0f));
    ImGui::SetNextWindowBgAlpha(0.98f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 10.0f));
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

        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::Text("  Versículos favoritos (%d)", (int)favorites.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (favorites.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
            ImGui::TextWrapped("Todavía no marcaste ningún versículo. Usa el ícono de favorito en una diapositiva o clic derecho sobre un versículo.");
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < (int)favorites.size(); i++) {
            auto& fv = favorites[i];
            ImGui::PushID(i);

            const char* bookName = BibleBooks::GetCanonicalBookName(fv.bookNum);
            std::string shortB = BibleNames::ParseBibleName(fv.bible).shortCode;
            std::string ref = (bookName ? bookName : ("Libro " + std::to_string(fv.bookNum)))
                             + " " + std::to_string(fv.chapterNum) + ":" + std::to_string(fv.verseNum)
                             + (shortB.empty() ? "" : (" (" + shortB + ")"));

            float removeW = 24.0f;
            float rowW    = ImGui::GetContentRegionAvail().x - removeW - 4.0f;
            float rowH    = ImGui::GetTextLineHeight() * 2.0f + 8.0f;

            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::TextPrimary));

            bool selected = ImGui::Selectable(("##fav" + std::to_string(i)).c_str(), false,
                                              ImGuiSelectableFlags_None, ImVec2(rowW, rowH));
            ImGui::PopStyleColor(3);

            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            ImDrawList* fdl = ImGui::GetWindowDrawList();
            fdl->PushClipRect(rMin, rMax, true);
            fdl->AddText({ rMin.x + 6.0f, rMin.y + 3.0f }, DS::TextPrimary, ref.c_str());
            std::string snippet = TextUtils::Utf8SafeSubstr(fv.text, 56);
            if (snippet.size() < fv.text.size()) snippet += "...";
            fdl->AddText({ rMin.x + 6.0f, rMin.y + 4.0f + ImGui::GetTextLineHeight() },
                         DS::TextSecondary, snippet.c_str());
            fdl->PopClipRect();

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.10f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::DangerColor));
            if (ImGui::Button("x", ImVec2(22.0f, rowH))) {
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
    if (!bible.empty() && bible != m_CurrentBible.name)
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
    float fullAvailW = ImGui::GetContentRegionAvail().x;
    ImDrawList* parentDl = ImGui::GetWindowDrawList();

    // Encabezado limpio + control segmentado moderno [ Todos | AT | NT ]
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::TextUnformatted("LIBROS");
    ImGui::PopStyleColor();
    {
        std::string countStr = std::to_string(m_CurrentBible.books.size());
        float cw = ImGui::CalcTextSize(countStr.c_str()).x;
        ImGui::SameLine(std::max(60.0f, fullAvailW - cw));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
        ImGui::TextUnformatted(countStr.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    // Control segmentado de testamento a ancho completo
    {
        const float segH = 26.0f;
        const float pad  = 2.0f;
        ImVec2 tMin = ImGui::GetCursorScreenPos();
        ImVec2 tMax(tMin.x + fullAvailW, tMin.y + segH);
        parentDl->AddRectFilled(tMin, tMax, IM_COL32(12, 13, 17, 235), 6.0f);
        parentDl->AddRect(tMin, tMax, DS::GlassBorder, 6.0f, 0, 1.0f);

        float segW = std::floor((fullAvailW - pad * 2.0f) / 3.0f);
        const char* labels[3] = { "Todos", "Antiguo", "Nuevo" };
        if (fullAvailW < 185.0f) {
            labels[1] = "AT";
            labels[2] = "NT";
        }

        for (int f = 0; f < 3; f++) {
            ImVec2 bMin(tMin.x + pad + f * segW, tMin.y + pad);
            ImVec2 bMax(f == 2 ? (tMax.x - pad) : (bMin.x + segW), tMax.y - pad);
            ImGui::SetCursorScreenPos(bMin);
            ImGui::PushID(f);
            bool clicked = ImGui::InvisibleButton("##segBtn", ImVec2(bMax.x - bMin.x, bMax.y - bMin.y));
            bool hov     = ImGui::IsItemHovered();
            ImGui::PopID();

            bool sel = (m_TestamentFilter == f);
            if (sel) {
                parentDl->AddRectFilled(bMin, bMax, ColA(DS::AccentColor, 110), 4.5f);
                parentDl->AddRect(bMin, bMax, ColA(DS::AccentColor, 190), 4.5f, 0, 1.0f);
            } else if (hov) {
                parentDl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 14), 4.5f);
            }

            ImVec2 lSz = ImGui::CalcTextSize(labels[f]);
            parentDl->AddText(
                ImVec2((bMin.x + bMax.x - lSz.x) * 0.5f, (bMin.y + bMax.y - lSz.y) * 0.5f),
                sel ? IM_COL32(255, 255, 255, 255) : (hov ? DS::TextPrimary : DS::TextSecondary),
                labels[f]);

            if (clicked)
                m_TestamentFilter = f;
        }
        ImGui::SetCursorScreenPos(ImVec2(tMin.x, tMax.y + 6.0f));
    }

    ImGui::BeginChild("##BookGrid",
        ImVec2(0.0f, 0.0f), false);

    float availX = ImGui::GetContentRegionAvail().x;
    const float spacing = 5.0f;
    const float cellH   = 30.0f;
    int cols = std::max(2, (int)((availX + spacing) / (54.0f + spacing)));
    float cellW = std::floor((availX - spacing * (cols - 1)) / (float)cols);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

    const QuickNavResolution& quickRes = m_QuickNav.GetResolution();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int drawnCol = 0;

    for (int i = 0; i < (int)m_CurrentBible.books.size(); i++) {
        auto& b = m_CurrentBible.books[i];

        if (m_TestamentFilter == 1 && b.canonicalNumber > 39) continue;
        if (m_TestamentFilter == 2 && b.canonicalNumber <= 39) continue;

        float r, g, bv;
        BibleBooks::GetSectionColor(BibleBooks::GetBookSection(b.canonicalNumber), r, g, bv);

        bool selected    = (m_SelectedBook == i);
        bool isProjected = (m_ProjectedBookIdx == i);
        bool filtered    = (m_FilteredBook < 0) || (i == m_FilteredBook);
        bool quickHit    = m_QuickNav.IsOpen() && quickRes.hasBook && quickRes.bookIdx == i;
        float alpha      = filtered ? 1.0f : 0.22f;

        const char* abbrev = BibleBooks::GetBookShortAbbrev(b.canonicalNumber);
        std::string btnLabel = abbrev ? abbrev : TextUtils::Utf8SafeSubstr(b.name, 4);

        ImGui::PushID(i);
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax(cMin.x + cellW, cMin.y + cellH);

        bool clicked = ImGui::InvisibleButton("##bkCell", ImVec2(cellW, cellH));
        bool hovered = ImGui::IsItemHovered();

        if (clicked) {
            m_SelectedBook    = i;
            m_SelectedChapter = 0;
            m_SelectedVerse   = 0;
            m_ScrollToVerse   = 0;
            m_LiveSearch[0]   = '\0';
            m_FilteredBook    = -1;
            m_FilteredChapter = -1;
        }

        // Superficie sobria profesional + barra lateral de sección bíblica
        ImU32 bgCol;
        if (selected) {
            bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r * 0.26f + 0.08f, g * 0.26f + 0.08f, bv * 0.26f + 0.08f, 1.0f));
        } else if (hovered) {
            bgCol = IM_COL32(42, 46, 56, (int)(245 * alpha));
        } else {
            bgCol = IM_COL32(25, 27, 34, (int)(225 * alpha));
        }

        dl->AddRectFilled(cMin, cMax, bgCol, 5.0f);

        // Barra de color canónico sutil a la izquierda
        ImU32 barCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, (selected ? 1.0f : 0.72f) * alpha));
        dl->AddRectFilled(ImVec2(cMin.x, cMin.y + 3.0f), ImVec2(cMin.x + 3.0f, cMax.y - 3.0f), barCol, 1.5f);

        // Borde limpio
        ImU32 bordCol = selected
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.90f))
            : (hovered ? IM_COL32(255, 255, 255, (int)(55 * alpha)) : IM_COL32(255, 255, 255, (int)(18 * alpha)));
        dl->AddRect(cMin, cMax, bordCol, 5.0f, 0, selected ? 1.5f : 1.0f);

        if (quickHit) {
            dl->AddRect(cMin, cMax, Col(0.55f, 0.95f, 0.65f, 0.95f), 5.0f, 0, 2.0f);
        }

        if (isProjected) {
            dl->AddCircleFilled(ImVec2(cMax.x - 5.5f, cMin.y + 5.5f), 2.5f, IM_COL32(245, 80, 80, 240));
        }

        ImVec2 lSz = ImGui::CalcTextSize(btnLabel.c_str());
        ImU32 txtCol = selected
            ? IM_COL32(255, 255, 255, 255)
            : IM_COL32(215, 220, 230, (int)(240 * alpha));
        dl->AddText(ImVec2(cMin.x + 2.0f + (cellW - 2.0f - lSz.x) * 0.5f, cMin.y + (cellH - lSz.y) * 0.5f),
                    txtCol, btnLabel.c_str());

        if (hovered)
            ImGui::SetTooltip("%s (%s · %d cap.)", b.name.c_str(),
                              b.canonicalNumber <= 39 ? "Antiguo Testamento" : "Nuevo Testamento",
                              (int)b.chapters.size());
        ImGui::PopID();

        drawnCol++;
        if (drawnCol % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void BibleView::RenderChapterGrid() {
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    auto& book = m_CurrentBible.books[m_SelectedBook];

    float headAvailW = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::TextUnformatted("CAPÍTULOS");
    ImGui::PopStyleColor();

    {
        std::string bookTag = book.name;
        float tw = ImGui::CalcTextSize(bookTag.c_str()).x;
        if (headAvailW - tw > 82.0f) {
            ImGui::SameLine(headAvailW - tw);
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
            ImGui::TextUnformatted(bookTag.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::BeginChild("##ChapterGrid", ImVec2(0.0f, 0.0f), false);

    float availX = ImGui::GetContentRegionAvail().x;
    const float spacing = 5.0f;
    const float cellH   = 28.0f;
    int cols = std::max(3, (int)((availX + spacing) / (34.0f + spacing)));
    float cellW = std::floor((availX - spacing * (cols - 1)) / (float)cols);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

    float r, g, bv;
    BibleBooks::GetSectionColor(BibleBooks::GetBookSection(book.canonicalNumber), r, g, bv);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)book.chapters.size(); i++) {
        bool selected    = (m_SelectedChapter == i);
        bool isProjected = (m_ProjectedBookIdx == m_SelectedBook && m_ProjectedChapIdx == i);
        bool filteredHit = (m_FilteredBook == m_SelectedBook && m_FilteredChapter == i);

        ImGui::PushID(1000 + i);
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax(cMin.x + cellW, cMin.y + cellH);

        bool clicked = ImGui::InvisibleButton("##chCell", ImVec2(cellW, cellH));
        bool hovered = ImGui::IsItemHovered();

        if (clicked) {
            m_SelectedChapter = i;
            m_SelectedVerse   = 0;
            m_ScrollToVerse   = 0;
        }

        ImU32 bgCol = selected
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(r * 0.30f + 0.08f, g * 0.30f + 0.08f, bv * 0.30f + 0.08f, 1.0f))
            : (hovered ? IM_COL32(42, 46, 56, 245) : IM_COL32(25, 27, 34, 225));
        ImU32 bordCol = selected
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.90f))
            : (filteredHit ? ColA(DS::AccentColor, 200) : IM_COL32(255, 255, 255, hovered ? 55 : 18));

        dl->AddRectFilled(cMin, cMax, bgCol, 5.0f);
        dl->AddRect(cMin, cMax, bordCol, 5.0f, 0, selected ? 1.5f : 1.0f);

        if (isProjected) {
            dl->AddCircleFilled(ImVec2(cMax.x - 4.5f, cMin.y + 4.5f), 2.2f, IM_COL32(245, 80, 80, 240));
        }

        std::string numStr = std::to_string(book.chapters[i].number);
        ImVec2 nSz = ImGui::CalcTextSize(numStr.c_str());
        dl->AddText(ImVec2(cMin.x + (cellW - nSz.x) * 0.5f, cMin.y + (cellH - nSz.y) * 0.5f),
                    selected ? IM_COL32(255, 255, 255, 255) : DS::TextSecondary,
                    numStr.c_str());

        ImGui::PopID();

        if ((i + 1) % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void BibleView::RenderVerseList() {
    if (m_SelectedBook    < 0 || m_SelectedBook    >= (int)m_CurrentBible.books.size()) return;
    if (m_SelectedChapter < 0 || m_SelectedChapter >= (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) return;

    auto& book = m_CurrentBible.books[m_SelectedBook];
    auto& chap = book.chapters[m_SelectedChapter];

    float r, g, bv;
    BibleBooks::GetSectionColor(BibleBooks::GetBookSection(book.canonicalNumber), r, g, bv);

    const float marginH = 14.0f;
    const float btnH    = 26.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Barra superior del capítulo: título + navegación fija + vista/zoom a la derecha ──
    ImVec2 rowStart = ImGui::GetCursorScreenPos();
    float  paneMaxX = ImGui::GetWindowContentRegionMax().x;

    // Acento vertical de sección + Título del capítulo
    dl->AddRectFilled(ImVec2(rowStart.x + marginH, rowStart.y + 3.0f),
                      ImVec2(rowStart.x + marginH + 3.5f, rowStart.y + btnH - 3.0f),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 1.0f)), 2.0f);

    ImGui::SetCursorPosX(marginH + 11.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(245, 247, 252, 255));
    ImGui::Text("%s %d", book.name.c_str(), chap.number);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
    ImGui::Text("· %d vers.", (int)chap.verses.size());
    ImGui::PopStyleColor();

    // Botones compactos Anterior / Siguiente en posición fija (sin saltos al llegar al cap. 1)
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));

    bool canPrev = (m_SelectedChapter > 0);
    ImGui::BeginDisabled(!canPrev);
    if (ImGui::Button("##prevChap", ImVec2(btnH, btnH)) && canPrev) {
        m_SelectedChapter--;
        m_SelectedVerse = 0;
        m_ScrollToVerse = 0;
    }
    {
        ImVec2 bMin = ImGui::GetItemRectMin();
        DrawChevronIcon(dl, ImVec2(bMin.x + btnH * 0.5f, bMin.y + btnH * 0.5f), 12.0f,
                        canPrev ? DS::TextPrimary : DS::TextHint, 0);
    }
    if (canPrev && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Capítulo anterior");
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 4.0f);

    bool canNext = (m_SelectedChapter + 1 < (int)book.chapters.size());
    ImGui::BeginDisabled(!canNext);
    if (ImGui::Button("##nextChap", ImVec2(btnH, btnH)) && canNext) {
        m_SelectedChapter++;
        m_SelectedVerse = 0;
        m_ScrollToVerse = 0;
    }
    {
        ImVec2 bMin = ImGui::GetItemRectMin();
        DrawChevronIcon(dl, ImVec2(bMin.x + btnH * 0.5f, bMin.y + btnH * 0.5f), 12.0f,
                        canNext ? DS::TextPrimary : DS::TextHint, 1);
    }
    if (canNext && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Capítulo siguiente");
    ImGui::EndDisabled();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    // Controles de vista ("Lista clásica" + Zoom) alineados a la derecha sin desbordar jamás
    const float checkboxW = ImGui::GetFrameHeight() + ImGui::CalcTextSize("Lista clásica").x + 10.0f;
    const float sliderW   = 88.0f;
    const float zoomGrpW  = m_ClassicListView ? 0.0f : (ImGui::CalcTextSize("Zoom").x + 8.0f + sliderW + 16.0f);
    const float rightControlsW = zoomGrpW + checkboxW;

    float targetRightX = paneMaxX - marginH - rightControlsW;
    if (targetRightX > ImGui::GetCursorPosX() + 12.0f) {
        ImGui::SameLine(targetRightX);
    } else {
        ImGui::SetCursorPosX(marginH);
    }

    if (!m_ClassicListView) {
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextUnformatted("Zoom");
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 6.0f);
        if (DS::ModernSlider("##verseZoom", &m_VerseCardZoom, 0.65f, 1.75f, sliderW)) {
            auto& gen = Settings::SettingsManager::Get().GetSettings().general;
            gen.bibleVerseCardZoom = m_VerseCardZoom;
            Settings::SettingsManager::Get().Save();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Tamaño de las diapositivas de previsualización");
        ImGui::SameLine(0.0f, 14.0f);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ToVec4(DS::AccentLight));
    ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
    if (ImGui::Checkbox("Lista clásica", &m_ClassicListView)) {
        auto& gen = Settings::SettingsManager::Get().GetSettings().general;
        gen.bibleClassicListView = m_ClassicListView;
        Settings::SettingsManager::Get().Save();
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Alternar entre vista de diapositivas (con fondo y fuente) y lista clásica");

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(p.x + marginH - 4.0f, p.y),
                    ImVec2(p.x + ImGui::GetContentRegionAvail().x - marginH + 4.0f, p.y),
                    DS::GlassBorder);
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (m_ClassicListView) {
        RenderClassicVerseList(book, chap, r, g, bv, marginH);
    } else {
        RenderPreviewVerseGrid(book, chap, r, g, bv, marginH);
    }
}

void BibleView::RenderClassicVerseList(const BookData& book, const ChapterData& chap, float r, float g, float bv, float marginH) {
    const float numColW     = 38.0f;
    const float gap         = 10.0f;
    const float rowPadV     = 8.0f;
    const float availW      = ImGui::GetContentRegionAvail().x;
    const float verseTextW  = std::max(80.0f, availW - marginH * 2.0f - numColW - gap - 48.0f);
    const float textAbsX    = ImGui::GetCursorScreenPos().x + marginH + numColW + gap;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)chap.verses.size(); i++) {
        auto& verse = chap.verses[i];
        ImGui::PushID(i);

        ImVec2 textSz = ImGui::CalcTextSize(verse.text.c_str(), nullptr, false, verseTextW);
        float  rowH   = std::max(32.0f, textSz.y + rowPadV * 2.0f);
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
            if (ImGui::MenuItem("Editar versículo")) {
                m_EditBookIdx  = m_SelectedBook;
                m_EditChapIdx  = m_SelectedChapter;
                m_EditVerseIdx = i;
                size_t len = std::min(verse.text.size(), sizeof(m_EditBuffer) - 1);
                memcpy(m_EditBuffer, verse.text.c_str(), len);
                m_EditBuffer[len] = '\0';
                m_EditStatus.clear();
                m_ShowEditModal = true;
            }
            ImGui::EndPopup();
        }

        bool isProjected = (i == m_ProjectedVerseIdx
                         && m_SelectedBook    == m_ProjectedBookIdx
                         && m_SelectedChapter == m_ProjectedChapIdx);
        bool isSelected  = (i == m_SelectedVerse);

        ImVec2 bgMin(rowMin.x + 4.0f, rowMin.y + 1.0f);
        ImVec2 bgMax(rowMax.x - 4.0f, rowMax.y - 1.0f);

        if (isProjected) {
            dl->AddRectFilled(bgMin, bgMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r * 0.25f + 0.04f, g * 0.25f + 0.04f, bv * 0.25f + 0.04f, 0.78f)), 6.0f);
            dl->AddRect(bgMin, bgMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.55f)), 6.0f, 0, 1.0f);
        } else if (isSelected) {
            dl->AddRectFilled(bgMin, bgMax, ColA(DS::AccentColor, 55), 6.0f);
            dl->AddRect(bgMin, bgMax, ColA(DS::AccentColor, 120), 6.0f, 0, 1.0f);
        } else if (hovered) {
            dl->AddRectFilled(bgMin, bgMax, IM_COL32(255, 255, 255, 11), 6.0f);
        } else if (i % 2 == 1) {
            dl->AddRectFilled(bgMin, bgMax, IM_COL32(255, 255, 255, 4), 6.0f);
        }

        if (isProjected) {
            dl->AddRectFilled(ImVec2(bgMin.x, bgMin.y + 3.0f),
                              ImVec2(bgMin.x + 3.5f, bgMax.y - 3.0f),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 1.0f)), 2.0f);
        }

        if (verse.edited)
            dl->AddCircleFilled(ImVec2(rowMin.x + marginH - 4.0f, rowMin.y + rowPadV + 4.0f),
                3.0f, Col(0.95f, 0.72f, 0.20f, 0.90f));

        {
            std::string numStr = std::to_string(verse.number);
            float boxH = 22.0f;
            float boxW = std::max(26.0f, ImGui::CalcTextSize(numStr.c_str()).x + 12.0f);
            ImVec2 boxMin(rowMin.x + marginH, rowMin.y + (rowH - boxH) * 0.5f);
            ImVec2 boxMax(boxMin.x + boxW, boxMin.y + boxH);

            ImU32 boxFill = isProjected
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.32f))
                : (isSelected ? ColA(DS::AccentColor, 85) : DS::BtnDefaultFill);
            ImU32 boxBord = isProjected
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.85f))
                : DS::GlassBorder;

            dl->AddRectFilled(boxMin, boxMax, boxFill, 5.0f);
            dl->AddRect(boxMin, boxMax, boxBord, 5.0f, 0, 1.0f);

            ImVec2 numSz = ImGui::CalcTextSize(numStr.c_str());
            dl->AddText(ImVec2(boxMin.x + (boxW - numSz.x) * 0.5f, boxMin.y + (boxH - numSz.y) * 0.5f),
                (isProjected || isSelected) ? IM_COL32(255, 255, 255, 255) : DS::TextSecondary,
                numStr.c_str());
        }

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(textAbsX, rowMin.y + rowPadV),
            ColA(DS::TextPrimary, (isSelected || isProjected) ? 255 : 228),
            verse.text.c_str(), nullptr, verseTextW);

        if (hovered || isSelected || isFavorite) {
            float editBtnX = rowMax.x - 30.0f;
            float favBtnX  = editBtnX - 24.0f;
            float btnY     = rowMin.y + (rowH - 22.0f) * 0.5f;

            ImGui::PushStyleColor(ImGuiCol_Button,        ToVec4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(2.0f, 2.0f));

            ImGui::SetCursorScreenPos(ImVec2(favBtnX, btnY));
            bool btnFavClicked = ImGui::Button("##fav", ImVec2(22.0f, 22.0f));
            ImVec2 favMin = ImGui::GetItemRectMin();
            DrawHeartOrFavIcon(dl, ImVec2(favMin.x + 11.0f, favMin.y + 11.0f), 13.0f,
                               isFavorite ? DS::AccentLight : DS::TextHint, isFavorite);
            if (!isFavorite && (hovered || isSelected))
                DrawPlusBadge(dl, ImVec2(favMin.x + 18.0f, favMin.y + 4.0f));

            if (btnFavClicked)
                Favorites::ToggleFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number, verse.text);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(isFavorite ? "Quitar de favoritos" : "Agregar a favoritos");

            if (hovered || isSelected) {
                ImGui::SetCursorScreenPos(ImVec2(editBtnX, btnY));
                bool btnEditClicked = ImGui::Button("##edit", ImVec2(22.0f, 22.0f));
                ImVec2 btnMin = ImGui::GetItemRectMin();
                DrawEditPencilIcon(dl, ImVec2(btnMin.x + 11.0f, btnMin.y + 11.0f), 13.0f, DS::TextSecondary);

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
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Editar versículo");
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
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

void BibleView::RenderPreviewVerseGrid(const BookData& book, const ChapterData& chap, float r, float g, float bv, float marginH) {
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    float availW = ImGui::GetContentRegionAvail().x - marginH * 2.0f;
    if (availW < 120.0f) availW = 120.0f;

    ImGui::SetCursorPosX(marginH);

    float baseColW = 285.0f * m_VerseCardZoom;
    int columns = std::clamp(static_cast<int>((availW + 12.0f) / (baseColW + 12.0f)), 1, 6);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Resolver fuentes una sola vez (evitando "Predeterminada" que es ProggyClean bitmap sin glifos Unicode)
    ImFont* uiFont = ImGui::GetFont();
    const std::string& lyricsFontName = state.lyricsBox.fontName;
    const std::string& indexFontName  = state.indexBox.fontName;

    ImFont* lyricsFont = (!lyricsFontName.empty() && lyricsFontName != "Predeterminada")
                         ? core.GetImGuiFont(lyricsFontName, 20.0f)
                         : uiFont;
    if (!lyricsFont) lyricsFont = uiFont;

    ImFont* refFont = (!indexFontName.empty() && indexFontName != "Predeterminada")
                      ? core.GetImGuiFont(indexFontName, 14.0f)
                      : lyricsFont;
    if (!refFont) refFont = uiFont;

    const bool hasCurlyQuotes = lyricsFont->IsGlyphInFont((ImWchar)0x201C)
                             && lyricsFont->IsGlyphInFont((ImWchar)0x201D);

    // Obtener textura de fondo y overlay UNA sola vez por frame (rendimiento óptimo)
    void* bgTex = core.GetPreviewBackgroundTexture(384, 216);
    if (!bgTex) bgTex = core.GetProcessedBackgroundTexture(384, 216);
    void* ovTex = core.GetOverlayTexture();

    const std::string shortBibleCode = BibleNames::ParseBibleName(m_CurrentBible.name).shortCode;
    const char* bookShort = BibleBooks::GetBookShortAbbrev(book.canonicalNumber);

    ImU32 baseTextCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(state.lyricsBox.color[0], state.lyricsBox.color[1], state.lyricsBox.color[2], state.lyricsBox.color[3]));
    if ((baseTextCol & 0x00FFFFFF) == 0 || (baseTextCol >> 24) < 50)
        baseTextCol = IM_COL32(246, 247, 250, 255);

    const bool needsReadableShadow = !state.lyricsBox.effects.shadowEnabled && !state.lyricsBox.effects.borderEnabled;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 14.0f));

    if (ImGui::BeginTable("##BiblePreviewGridTable", columns, ImGuiTableFlags_SizingStretchSame)) {
        for (int i = 0; i < (int)chap.verses.size(); i++) {
            auto& verse = chap.verses[i];
            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImVec2 p_min = ImGui::GetCursorScreenPos();
            float cardW = ImGui::GetContentRegionAvail().x;
            if (cardW < 90.0f) cardW = 90.0f;

            float slideH  = std::round(cardW * (9.0f / 16.0f));
            float footerH = 26.0f;
            float totalH  = slideH + footerH;

            ImVec2 p_max     = ImVec2(p_min.x + cardW, p_min.y + totalH);
            ImVec2 slideMax  = ImVec2(p_max.x, p_min.y + slideH);
            ImVec2 footerMin = ImVec2(p_min.x, slideMax.y);

            if (m_ScrollToVerse == i) {
                ImGui::SetScrollHereY(0.3f);
                m_ScrollToVerse = -1;
            }

            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("##vCardBtn", ImVec2(cardW, totalH));
            bool clicked   = ImGui::IsItemClicked();
            bool dblClick  = ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered();
            bool hovered   = ImGui::IsItemHovered();

            bool isFavorite  = Favorites::IsFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number);
            bool isProjected = (i == m_ProjectedVerseIdx
                             && m_SelectedBook    == m_ProjectedBookIdx
                             && m_SelectedChapter == m_ProjectedChapIdx);
            bool isSelected  = (i == m_SelectedVerse);

            if (ImGui::BeginPopupContextItem("verse_card_ctx")) {
                if (ImGui::MenuItem(isFavorite ? "Quitar de favoritos" : "Agregar a favoritos", nullptr, isFavorite)) {
                    Favorites::ToggleFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number, verse.text);
                }
                if (ImGui::MenuItem("Editar versículo")) {
                    m_EditBookIdx  = m_SelectedBook;
                    m_EditChapIdx  = m_SelectedChapter;
                    m_EditVerseIdx = i;
                    size_t len = std::min(verse.text.size(), sizeof(m_EditBuffer) - 1);
                    memcpy(m_EditBuffer, verse.text.c_str(), len);
                    m_EditBuffer[len] = '\0';
                    m_EditStatus.clear();
                    m_ShowEditModal = true;
                }
                ImGui::EndPopup();
            }

            const float rounding = 8.0f;

            // Sombra exterior suave debajo de cada tarjeta
            dl->AddRectFilled(ImVec2(p_min.x + 1.0f, p_min.y + 3.0f),
                              ImVec2(p_max.x + 1.0f, p_max.y + 3.0f),
                              IM_COL32(0, 0, 0, 95), rounding + 1.0f);

            // 1. Lienzo 16:9 de la diapositiva
            dl->PushClipRect(p_min, slideMax, true);

            if (bgTex) {
                dl->AddImageRounded((ImTextureID)(intptr_t)bgTex, p_min, slideMax,
                                    ImVec2(0, 0), ImVec2(1, 1),
                                    IM_COL32(255, 255, 255, 255), rounding, ImDrawFlags_RoundCornersTop);
                dl->AddRectFilled(p_min, slideMax, IM_COL32(0, 0, 0, 56), rounding, ImDrawFlags_RoundCornersTop);
            } else if (state.bgType == Core::PresentationState::BackgroundType::SolidColor && state.isProjecting) {
                ImU32 col = IM_COL32((int)(state.bgColor[0] * 255.0f),
                                     (int)(state.bgColor[1] * 255.0f),
                                     (int)(state.bgColor[2] * 255.0f), 255);
                dl->AddRectFilled(p_min, slideMax, col, rounding, ImDrawFlags_RoundCornersTop);
            } else {
                // Fondo estudio oscuro con degradado suave (sin tablero ruidoso)
                dl->AddRectFilled(p_min, slideMax, IM_COL32(18, 20, 28, 255), rounding, ImDrawFlags_RoundCornersTop);
                dl->AddRectFilledMultiColor(
                    ImVec2(p_min.x, p_min.y + rounding * 0.5f), slideMax,
                    IM_COL32(28, 32, 46, 200), IM_COL32(20, 23, 34, 200),
                    IM_COL32(11, 12, 18, 255), IM_COL32(14, 16, 24, 255));
                auto bgIt = StyleGeneralApp::Icons.find("song_card_bg");
                if (bgIt != StyleGeneralApp::Icons.end() && bgIt->second.textureID != nullptr) {
                    dl->AddImageRounded(bgIt->second.textureID, p_min, slideMax,
                                        ImVec2(0, 0), ImVec2(1, 1),
                                        IM_COL32(255, 255, 255, 42), rounding, ImDrawFlags_RoundCornersTop);
                }
            }

            if (ovTex) {
                dl->AddImage((ImTextureID)(intptr_t)ovTex, p_min, slideMax, ImVec2(0, 0), ImVec2(1, 1));
            }

            // 2. Tipografía nítida centrada línea por línea + cita auto-ajustada
            float padX  = std::clamp(cardW * 0.075f, 10.0f, 22.0f);
            float padY  = std::clamp(slideH * 0.085f, 8.0f, 18.0f);
            float safeW = std::max(40.0f, cardW - padX * 2.0f);
            float safeH = std::max(30.0f, slideH - padY * 2.0f);

            std::string displayText;
            bool startsQuoted = !verse.text.empty() &&
                                (verse.text.front() == '"' || (unsigned char)verse.text.front() == 0xE2 || (unsigned char)verse.text.front() == 0xC2);
            if (startsQuoted) {
                displayText = verse.text;
            } else if (hasCurlyQuotes) {
                displayText = "\xE2\x80\x9C" + verse.text + "\xE2\x80\x9D";
            } else {
                displayText = "\"" + verse.text + "\"";
            }

            float fontSize = std::clamp(slideH * 0.122f, 11.0f, 21.0f);
            while (fontSize > 9.5f) {
                ImVec2 ts = lyricsFont->CalcTextSizeA(fontSize, FLT_MAX, safeW, displayText.c_str());
                if (ts.y <= safeH * 0.62f) break;
                fontSize -= 0.75f;
            }

            std::vector<std::string> wrappedLines = WrapTextLines(lyricsFont, fontSize, displayText, safeW);
            float lineAdvance = fontSize * 1.18f;
            float textBlockH  = wrappedLines.empty() ? 0.0f : ((float)(wrappedLines.size() - 1) * lineAdvance + fontSize);

            std::string inSlideRef = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
            if (!shortBibleCode.empty())
                inSlideRef += " (" + shortBibleCode + ")";

            float refFontSize = std::clamp(fontSize * 0.72f, 9.5f, 14.0f);
            ImVec2 refSz = refFont->CalcTextSizeA(refFontSize, FLT_MAX, 0.0f, inSlideRef.c_str());
            if (refSz.x > safeW && refSz.x > 1.0f) {
                refFontSize = std::max(8.0f, refFontSize * (safeW / refSz.x));
                refSz = refFont->CalcTextSizeA(refFontSize, FLT_MAX, 0.0f, inSlideRef.c_str());
            }

            float gapLine     = std::clamp(slideH * 0.045f, 4.0f, 8.0f);
            float divLineW    = std::clamp(safeW * 0.32f, 34.0f, 96.0f);
            float totalBlockH = textBlockH + gapLine + 1.0f + gapLine + refSz.y;

            float curY = p_min.y + std::max(padY * 0.65f, (slideH - totalBlockH) * 0.5f);
            float fxScale = cardW / 1920.0f;

            for (const auto& ln : wrappedLines) {
                if (!ln.empty()) {
                    ImVec2 lnSz = lyricsFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ln.c_str());
                    float lx = p_min.x + (cardW - lnSz.x) * 0.5f;
                    if (state.lyricsBox.hAlign == 0)      lx = p_min.x + padX;
                    else if (state.lyricsBox.hAlign == 2) lx = p_min.x + cardW - padX - lnSz.x;

                    if (needsReadableShadow) {
                        dl->AddText(lyricsFont, fontSize, ImVec2(lx + 1.0f, curY + 1.2f), IM_COL32(0, 0, 0, 175), ln.c_str());
                    }
                    DrawStyledText(dl, lyricsFont, fontSize, ImVec2(lx, curY), baseTextCol,
                                   ln.c_str(), 0.0f, fxScale, state.lyricsBox.effects);
                }
                curY += lineAdvance;
            }

            curY = (curY - lineAdvance + fontSize) + gapLine;
            float divX = p_min.x + (cardW - divLineW) * 0.5f;
            dl->AddLine(ImVec2(divX, curY), ImVec2(divX + divLineW, curY), IM_COL32(255, 255, 255, 95), 1.0f);
            curY += 1.0f + gapLine;

            float refX = p_min.x + (cardW - refSz.x) * 0.5f;
            dl->AddText(refFont, refFontSize, ImVec2(refX + 0.8f, curY + 1.0f), IM_COL32(0, 0, 0, 165), inSlideRef.c_str());
            dl->AddText(refFont, refFontSize, ImVec2(refX, curY), IM_COL32(226, 230, 240, 230), inSlideRef.c_str());

            dl->PopClipRect();

            // 3. Barra inferior (Footer profesional estilo ProPresenter / Holyrics)
            ImU32 footerCol;
            if (isProjected) {
                footerCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r * 0.42f + 0.06f, g * 0.42f + 0.06f, bv * 0.42f + 0.06f, 0.98f));
            } else if (isSelected) {
                footerCol = IM_COL32(42, 49, 78, 250);
            } else {
                footerCol = IM_COL32(20, 22, 29, 250);
            }

            dl->AddRectFilled(footerMin, p_max, footerCol, rounding, ImDrawFlags_RoundCornersBottom);
            dl->AddLine(footerMin, ImVec2(p_max.x, footerMin.y), IM_COL32(255, 255, 255, 24), 1.0f);

            float fMidY  = footerMin.y + footerH * 0.5f;
            float fTextY = footerMin.y + (footerH - ImGui::GetFontSize()) * 0.5f;

            // Píldora del número de versículo a la izquierda
            std::string numStr = std::to_string(verse.number);
            ImVec2 numSz = ImGui::CalcTextSize(numStr.c_str());
            float pillW  = std::max(22.0f, numSz.x + 10.0f);
            ImVec2 nMin(p_min.x + 6.0f, fMidY - 9.0f);
            ImVec2 nMax(nMin.x + pillW, fMidY + 9.0f);
            dl->AddRectFilled(nMin, nMax, isProjected ? IM_COL32(0, 0, 0, 75) : IM_COL32(255, 255, 255, 16), 4.0f);
            dl->AddText(ImVec2(nMin.x + (pillW - numSz.x) * 0.5f, fMidY - numSz.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), numStr.c_str());

            float leftUsedX = nMax.x + 6.0f;

            if (isProjected) {
                const char* liveTxt = "EN VIVO";
                ImVec2 lvSz = ImGui::CalcTextSize(liveTxt);
                ImVec2 lvMin(leftUsedX, fMidY - 8.5f);
                ImVec2 lvMax(leftUsedX + lvSz.x + 18.0f, fMidY + 8.5f);
                dl->AddRectFilled(lvMin, lvMax, IM_COL32(220, 42, 42, 235), 4.0f);
                dl->AddCircleFilled(ImVec2(lvMin.x + 6.5f, fMidY), 2.8f, IM_COL32(255, 255, 255, 255));
                dl->AddText(ImVec2(lvMin.x + 12.5f, fMidY - lvSz.y * 0.5f), IM_COL32(255, 255, 255, 255), liveTxt);
                leftUsedX = lvMax.x + 6.0f;
            }

            if (verse.edited) {
                dl->AddCircleFilled(ImVec2(leftUsedX + 3.0f, fMidY), 3.0f, IM_COL32(245, 185, 50, 240));
                leftUsedX += 10.0f;
            }

            // Referencia compacta a la derecha del footer (sin solaparse nunca)
            float rightEdgeX = p_max.x - 8.0f;
            if (isFavorite) {
                DrawHeartOrFavIcon(dl, ImVec2(rightEdgeX - 6.0f, fMidY), 12.0f, DS::AccentLight, true);
                rightEdgeX -= 18.0f;
            }

            std::string footerRef = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
            if (!shortBibleCode.empty())
                footerRef += " · " + shortBibleCode;

            float fRefW = ImGui::CalcTextSize(footerRef.c_str()).x;
            if (rightEdgeX - fRefW < leftUsedX + 4.0f) {
                footerRef = (bookShort ? std::string(bookShort) : TextUtils::Utf8SafeSubstr(book.name, 4))
                          + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
                fRefW = ImGui::CalcTextSize(footerRef.c_str()).x;
            }
            if (rightEdgeX - fRefW >= leftUsedX + 2.0f) {
                dl->AddText(ImVec2(rightEdgeX - fRefW, fTextY),
                            isProjected ? IM_COL32(245, 247, 252, 245) : IM_COL32(175, 182, 198, 225),
                            footerRef.c_str());
            }

            // 4. Bordes de selección / proyección / hover
            if (isProjected) {
                dl->AddRect(ImVec2(p_min.x - 1.5f, p_min.y - 1.5f), ImVec2(p_max.x + 1.5f, p_max.y + 1.5f),
                            ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.38f)), rounding + 1.5f, 0, 1.5f);
                dl->AddRect(p_min, p_max, ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bv, 0.98f)), rounding, 0, 2.0f);
            } else if (isSelected) {
                dl->AddRect(p_min, p_max, ColA(DS::AccentColor, 225), rounding, 0, 1.6f);
            } else if (hovered) {
                dl->AddRectFilled(p_min, slideMax, IM_COL32(255, 255, 255, 12), rounding, ImDrawFlags_RoundCornersTop);
                dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 85), rounding, 0, 1.2f);
            } else {
                dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 24), rounding, 0, 1.0f);
            }

            // 5. Acciones rápidas flotantes al pasar el cursor (Favorito / Editar)
            if (hovered) {
                float btnSize  = 24.0f;
                float editBtnX = p_max.x - btnSize - 6.0f;
                float favBtnX  = editBtnX - btnSize - 4.0f;
                float btnY     = p_min.y + 6.0f;

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.07f, 0.08f, 0.11f, 0.88f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(DS::BtnHoverFill));
                ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(1.0f, 1.0f, 1.0f, 0.18f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));

                ImGui::SetCursorScreenPos(ImVec2(favBtnX, btnY));
                if (ImGui::Button("##cardFav", ImVec2(btnSize, btnSize))) {
                    Favorites::ToggleFavorite(m_CurrentBible.name, book.canonicalNumber, chap.number, verse.number, verse.text);
                }
                ImVec2 favRectMin = ImGui::GetItemRectMin();
                DrawHeartOrFavIcon(dl, ImVec2(favRectMin.x + btnSize * 0.5f, favRectMin.y + btnSize * 0.5f),
                                   13.0f, isFavorite ? DS::AccentLight : DS::TextSecondary, isFavorite);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(isFavorite ? "Quitar de favoritos" : "Agregar a favoritos");

                ImGui::SetCursorScreenPos(ImVec2(editBtnX, btnY));
                if (ImGui::Button("##cardEdit", ImVec2(btnSize, btnSize))) {
                    m_EditBookIdx  = m_SelectedBook;
                    m_EditChapIdx  = m_SelectedChapter;
                    m_EditVerseIdx = i;
                    size_t len = std::min(verse.text.size(), sizeof(m_EditBuffer) - 1);
                    memcpy(m_EditBuffer, verse.text.c_str(), len);
                    m_EditBuffer[len] = '\0';
                    m_EditStatus.clear();
                    m_ShowEditModal = true;
                }
                ImVec2 editRectMin = ImGui::GetItemRectMin();
                DrawEditPencilIcon(dl, ImVec2(editRectMin.x + btnSize * 0.5f, editRectMin.y + btnSize * 0.5f),
                                   13.0f, DS::TextSecondary);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Editar versículo");

                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(3);
            }

            if (clicked) {
                m_SelectedVerse = i;
                ProjectVerse(m_SelectedBook, m_SelectedChapter, i);
            }
            if (dblClick) {
                ProjectVerse(m_SelectedBook, m_SelectedChapter, i);
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

void BibleView::RenderEditModal() {
    if (!m_ShowEditModal) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f, 280.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.0f, 14.0f));

    bool open = true;
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("Editar versículo##editModal", &open, kFlags)) {

        if (m_EditBookIdx  >= 0 && m_EditBookIdx  < (int)m_CurrentBible.books.size()
         && m_EditChapIdx  >= 0 && m_EditChapIdx  < (int)m_CurrentBible.books[m_EditBookIdx].chapters.size()
         && m_EditVerseIdx >= 0 && m_EditVerseIdx < (int)m_CurrentBible.books[m_EditBookIdx].chapters[m_EditChapIdx].verses.size()) {

            auto& b = m_CurrentBible.books[m_EditBookIdx];
            auto& c = b.chapters[m_EditChapIdx];
            auto& v = c.verses[m_EditVerseIdx];

            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
            ImGui::Text("%s %d:%d", b.name.c_str(), c.number, v.number);
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
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
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
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
    if (m_NeedsRefreshBibles.exchange(false)) {
        RefreshAvailableBibles();
        ProyecThor::Library::ForceLibraryRescan() = true;

        std::string importedFile;
        {
            std::lock_guard<std::mutex> lk(m_ImportedBibleMutex);
            importedFile = m_LastImportedBibleFile;
            m_LastImportedBibleFile.clear();
        }
        if (!importedFile.empty()) {
            Core::LibrarySelection sel;
            sel.type  = Core::ItemType::Bible;
            sel.title = importedFile;
            Core::PresentationCore::Get().SetSelection(sel);
        }
    }

    auto selection = Core::PresentationCore::Get().PeekSelection();

    // Si se entro a la seccion Biblia con la lista izquierda colapsada, restaurar
    // la ultima Biblia cargada o cargar automaticamente RVR1960 / la primera disponible.
    if (selection.type == Core::ItemType::Bible && selection.title.empty()) {
        if (m_BibleLoaded && !m_LastSelectedFile.empty()) {
            selection.title = m_LastSelectedFile;
            Core::PresentationCore::Get().SetSelection(selection);
        } else if (!m_BibleLoaded && !m_TriedAutoSelectBible) {
            m_TriedAutoSelectBible = true;
            RefreshAvailableBibles();
            if (!m_AvailableBibleFiles.empty()) {
                std::string preferred = m_AvailableBibleFiles.front();
                for (const auto& f : m_AvailableBibleFiles) {
                    if (f == "SpanishRVR1960Bible.xml") { preferred = f; break; }
                }
                selection.title = preferred;
                Core::PresentationCore::Get().SetSelection(selection);
            }
        }
    }

    int jumpBook = -1, jumpChap = -1, jumpVerse = -1;
    if (Core::PresentationCore::Get().ConsumeBibleJump(jumpBook, jumpChap, jumpVerse)) {
        if (!m_BibleLoaded || m_CurrentBible.books.empty()) {
            LoadXMLBible(BiblesPath() + "SpanishRVR1960Bible.xml");
        }
        JumpToFavorite(m_CurrentBible.name, jumpBook, jumpChap, jumpVerse);
        ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
    }

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

    int navCue = Core::PresentationCore::Get().ConsumeNavigateCue();
    if (m_BibleLoaded) {
        if (navCue > 0) {
            NavigateVerse(+1);
        } else if (navCue < 0) {
            NavigateVerse(-1);
        } else if (!m_SearchFocused && !m_QuickNav.IsOpen() && !m_WordSearch.IsOpen() && !ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) || ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) || ImGui::IsKeyPressed(ImGuiKey_PageDown, false))
                NavigateVerse(+1);
            else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) || ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) || ImGui::IsKeyPressed(ImGuiKey_PageUp, false))
                NavigateVerse(-1);
        }
    }
    UpdateModifierTaps();

    RenderTopBar();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (ImGui::BeginTable("##BibleLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Books",    ImGuiTableColumnFlags_WidthFixed, 225.0f);
        ImGui::TableSetupColumn("Chapters", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Verses",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(DS::GlassBorder));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusLarge);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        // ── Columna 0: Libros de arriba a abajo ──
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::BeginChild("##BooksChild", ImVec2(0.0f, 0.0f), true);
        if (m_BibleLoaded) {
            RenderBookGrid();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
            ImGui::TextWrapped("Selecciona una Biblia arriba o en la Biblioteca para comenzar.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ── Columna 1: Capítulos de arriba a abajo ──
        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 10.0f));
        ImGui::BeginChild("##ChaptersChild", ImVec2(0.0f, 0.0f), true);
        if (m_BibleLoaded) {
            RenderChapterGrid();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ── Columna 2: Versículos ──
        ImGui::TableSetColumnIndex(2);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(ColA(DS::GlassFillTop, 235)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 10.0f));
        ImGui::BeginChild("##VersesChild", ImVec2(0.0f, 0.0f), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (m_BibleLoaded) {
            RenderVerseList();
        } else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImDrawList* edl = ImGui::GetWindowDrawList();
            ImVec2 center(ImGui::GetCursorScreenPos().x + avail.x * 0.5f,
                          ImGui::GetCursorScreenPos().y + avail.y * 0.45f);
            DrawBookIcon(edl, ImVec2(center.x, center.y - 18.0f), 34.0f, DS::TextHint);
            const char* msg = "Elige una versión bíblica para visualizar los versículos";
            ImVec2 mSz = ImGui::CalcTextSize(msg);
            edl->AddText(ImVec2(center.x - mSz.x * 0.5f, center.y + 10.0f), DS::TextSecondary, msg);
        }
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

    ImGui::SetNextFrameWantCaptureKeyboard(true);
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CloseJump();
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mp = io.MousePos;
        bool inside = mp.x >= m_JumpCardMin.x && mp.x <= m_JumpCardMax.x &&
                      mp.y >= m_JumpCardMin.y && mp.y <= m_JumpCardMax.y;
        if (!inside) { CloseJump(); return; }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && !m_JumpBuffer.empty())
        m_JumpBuffer.pop_back();

    if (!io.KeyCtrl && !io.KeyAlt) {
        for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
            ImWchar wc = io.InputQueueCharacters[i];
            if (wc >= '0' && wc <= '9')
                m_JumpBuffer += (char)wc;
        }
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   DS::RadiusLarge);
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

} // namespace ProyecThor::UI
