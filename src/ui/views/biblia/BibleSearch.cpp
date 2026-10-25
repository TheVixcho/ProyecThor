#include "BibleSearch.h"
#include "BibleTextUtils.h"
#include "BibleBookData.h"

#include <algorithm>
#include <cctype>

namespace ProyecThor::UI::Search {

bool ParseSmartQuery(const BibleData& bible, const std::string& rawQuery,
                      int& outBook, int& outChap, int& outVerse) {
    outBook = outChap = outVerse = -1;
    if (bible.books.empty() || rawQuery.empty()) return false;

    std::string q = TextUtils::StripAccents(TextUtils::ToLowerUTF8(rawQuery));

    for (size_t i = 1; i + 1 < q.size(); ++i) {
        if (q[i] == '.' && std::isdigit((unsigned char)q[i-1]) && std::isdigit((unsigned char)q[i+1])) {
            q[i] = ':';
        }
    }

    int chapNum = -1, verseNum = -1;

    size_t colonPos = q.rfind(':');
    if (colonPos != std::string::npos) {
        std::string afterColon = q.substr(colonPos + 1);
        afterColon.erase(std::remove_if(afterColon.begin(), afterColon.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), afterColon.end());
        if (!afterColon.empty()) try { verseNum = std::stoi(afterColon); } catch (...) {}

        std::string beforeColon = q.substr(0, colonPos);
        size_t lastSp = beforeColon.rfind(' ');
        std::string chapStr = (lastSp != std::string::npos) ? beforeColon.substr(lastSp + 1) : beforeColon;
        chapStr.erase(std::remove_if(chapStr.begin(), chapStr.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), chapStr.end());
        if (!chapStr.empty()) try { chapNum = std::stoi(chapStr); } catch (...) {}

        q = (lastSp != std::string::npos) ? beforeColon.substr(0, lastSp) : beforeColon;
    } else {
        std::vector<std::string> tokens;
        std::string cur;
        for (char c : q) {
            if (c == ' ') {
                if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) tokens.push_back(cur);

        if (!tokens.empty()) {
            bool lastNumeric = true;
            for (char c : tokens.back()) if (!std::isdigit((unsigned char)c)) { lastNumeric = false; break; }

            if (lastNumeric) {
                int lastVal = 0;
                try { lastVal = std::stoi(tokens.back()); } catch (...) {}
                tokens.pop_back();

                bool secondNumeric = false;
                if (!tokens.empty()) {
                    secondNumeric = true;
                    for (char c : tokens.back()) if (!std::isdigit((unsigned char)c)) { secondNumeric = false; break; }
                }

                if (secondNumeric && !tokens.empty()) {
                    int secVal = 0;
                    try { secVal = std::stoi(tokens.back()); } catch (...) {}
                    tokens.pop_back();
                    chapNum = secVal;
                    verseNum = lastVal;
                } else {
                    chapNum = lastVal;
                }

                q.clear();
                for (size_t t = 0; t < tokens.size(); ++t) {
                    if (t > 0) q += " ";
                    q += tokens[t];
                }
            }
        }
    }

    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back()  == ' ') q.pop_back();

    int resolvedBookNum = -1;
    {
        std::string nosp = TextUtils::Normalize(q);
        if (nosp == "salmo") nosp = "salmos";

        resolvedBookNum = BibleBooks::ResolveAbbrev(nosp);
        if (resolvedBookNum <= 0) {
            std::vector<int> candidates = BibleBooks::FindBookCandidates(nosp);
            if (!candidates.empty()) resolvedBookNum = candidates.front();
        }
    }
    if (resolvedBookNum <= 0) return false;

    for (int bi = 0; bi < (int)bible.books.size(); bi++) {
        if (bible.books[bi].canonicalNumber != resolvedBookNum) continue;
        outBook = bi;
        const BookData& book = bible.books[bi];
        if (chapNum > 0) {
            for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
                if (book.chapters[ci].number == chapNum) {
                    outChap = ci;
                    if (verseNum > 0) {
                        for (int vi = 0; vi < (int)book.chapters[ci].verses.size(); vi++) {
                            if (book.chapters[ci].verses[vi].number == verseNum)
                                { outVerse = vi; break; }
                        }
                        if (outVerse < 0) outVerse = 0;
                    }
                    break;
                }
            }
            if (outChap < 0) outChap = 0;
        } else {
            outChap = 0;
        }
        break;
    }
    return outBook >= 0;
}

}
