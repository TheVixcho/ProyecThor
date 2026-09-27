#include "BibleXmlIO.h"
#include "BibleBookData.h"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace ProyecThor::UI::XmlIO {

static std::string ExtractAttr(const std::string& xml, size_t searchStart, size_t searchEnd,
                                const std::string& attr) {
    std::string needle = attr + "=\"";
    size_t pos = xml.find(needle, searchStart);
    if (pos == std::string::npos || pos >= searchEnd) return "";
    pos += needle.size();
    size_t end = xml.find("\"", pos);
    if (end == std::string::npos || end > searchEnd) return "";
    return xml.substr(pos, end - pos);
}

bool LoadBible(const std::string& path, BibleData& outBible) {
    outBible = BibleData();

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xml = buffer.str();

    size_t biblePos = xml.find("<bible");
    if (biblePos != std::string::npos) {
        size_t bibleTagEnd = xml.find(">", biblePos);
        if (bibleTagEnd == std::string::npos) bibleTagEnd = xml.size();
        outBible.translation = ExtractAttr(xml, biblePos, bibleTagEnd, "translation");
        outBible.info        = ExtractAttr(xml, biblePos, bibleTagEnd, "info");
        outBible.link        = ExtractAttr(xml, biblePos, bibleTagEnd, "link");
    }

    size_t bookPos = 0;
    while ((bookPos = xml.find("<book", bookPos)) != std::string::npos) {
        BookData book;
        int bookNum = 0;
        size_t numStart = xml.find("number=\"", bookPos);
        if (numStart != std::string::npos) {
            numStart += 8;
            size_t numEnd = xml.find("\"", numStart);
            try { bookNum = std::stoi(xml.substr(numStart, numEnd - numStart)); } catch (...) {}
        }
        book.canonicalNumber = bookNum;
        const char* canonicalName = BibleBooks::GetCanonicalBookName(bookNum);
        book.name = (canonicalName != nullptr)
            ? canonicalName
            : "Libro " + std::to_string(bookNum);

        size_t nextBookPos = xml.find("<book", bookPos + 5);
        if (nextBookPos == std::string::npos) nextBookPos = xml.length();

        size_t chapPos = bookPos;
        while ((chapPos = xml.find("<chapter", chapPos)) != std::string::npos
               && chapPos < nextBookPos) {
            ChapterData chapter;
            size_t cnumStart = xml.find("number=\"", chapPos);
            if (cnumStart != std::string::npos && cnumStart < nextBookPos) {
                cnumStart += 8;
                size_t cnumEnd = xml.find("\"", cnumStart);
                try { chapter.number = std::stoi(xml.substr(cnumStart, cnumEnd - cnumStart)); } catch (...) {}
            }
            size_t nextChapPos = xml.find("<chapter", chapPos + 8);
            if (nextChapPos == std::string::npos) nextChapPos = nextBookPos;

            size_t versPos = chapPos;
            while ((versPos = xml.find("<verse", versPos)) != std::string::npos
                   && versPos < nextChapPos) {
                VerseData verse;
                size_t vnumStart = xml.find("number=\"", versPos);
                if (vnumStart != std::string::npos && vnumStart < nextChapPos) {
                    vnumStart += 8;
                    size_t vnumEnd = xml.find("\"", vnumStart);
                    try { verse.number = std::stoi(xml.substr(vnumStart, vnumEnd - vnumStart)); } catch (...) {}
                }
                size_t textStart = xml.find(">", versPos) + 1;
                size_t textEnd   = xml.find("</verse>", textStart);
                if (textStart != std::string::npos && textEnd != std::string::npos
                    && textStart < textEnd)
                    verse.text = xml.substr(textStart, textEnd - textStart);
                chapter.verses.push_back(verse);
                versPos = textEnd;
            }
            if (!chapter.verses.empty()) book.chapters.push_back(chapter);
            chapPos = nextChapPos;
        }
        if (!book.chapters.empty()) outBible.books.push_back(book);
        bookPos = nextBookPos;
    }

    outBible.name = fs::path(path).stem().string();
    return !outBible.books.empty();
}

bool SaveBible(const std::string& path, const BibleData& bible) {
    if (path.empty()) return false;

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<bible";
    if (!bible.translation.empty()) out << " translation=\"" << bible.translation << "\"";
    if (!bible.info.empty())        out << " info=\""        << bible.info        << "\"";
    if (!bible.link.empty())        out << " link=\""        << bible.link        << "\"";
    out << ">\n";

    bool inTestament   = false;
    bool testamentIsNew = false;
    for (const auto& b : bible.books) {
        bool isNew = b.canonicalNumber >= 40;
        if (!inTestament || isNew != testamentIsNew) {
            if (inTestament) out << "  </testament>\n";
            out << "  <testament name=\"" << (isNew ? "New" : "Old") << "\">\n";
            inTestament    = true;
            testamentIsNew = isNew;
        }

        out << "    <book number=\"" << b.canonicalNumber << "\" name=\"" << b.name << "\">\n";
        for (const auto& c : b.chapters) {
            out << "      <chapter number=\"" << c.number << "\">\n";
            for (const auto& v : c.verses) {
                out << "        <verse number=\"" << v.number << "\">"
                    << v.text
                    << "</verse>\n";
            }
            out << "      </chapter>\n";
        }
        out << "    </book>\n";
    }
    if (inTestament) out << "  </testament>\n";

    out << "</bible>\n";
    out.close();

    return true;
}

}
