#include "BibleFavorites.h"
#include "backend/core/AppPaths.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace ProyecThor::UI::Favorites {
namespace {

std::string FavoritesFilePath() {
    return ProyecThor::GetAssetsPath() + "/../bible_favorites.ini";
}

std::string MakeKey(const std::string& bible, int bookNum, int chapterNum, int verseNum) {
    return bible + "#" + std::to_string(bookNum) + "#" + std::to_string(chapterNum)
         + "#" + std::to_string(verseNum);
}

bool SplitKey(const std::string& key, std::string& bible, int& bookNum, int& chapterNum, int& verseNum) {
    size_t p1 = key.find('#');
    if (p1 == std::string::npos) return false;
    size_t p2 = key.find('#', p1 + 1);
    if (p2 == std::string::npos) return false;
    size_t p3 = key.find('#', p2 + 1);
    if (p3 == std::string::npos) return false;

    bible = key.substr(0, p1);
    try {
        bookNum    = std::stoi(key.substr(p1 + 1, p2 - p1 - 1));
        chapterNum = std::stoi(key.substr(p2 + 1, p3 - p2 - 1));
        verseNum   = std::stoi(key.substr(p3 + 1));
    } catch (...) { return false; }
    return true;
}

struct Cache {
    std::unordered_map<std::string, std::string> entries;
    bool loaded = false;
};

Cache& GetCache() {
    static Cache cache;
    if (!cache.loaded) {
        cache.loaded = true;
        std::ifstream f(FavoritesFilePath());
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto sep = line.find('=');
            if (sep == std::string::npos) continue;
            cache.entries[line.substr(0, sep)] = line.substr(sep + 1);
        }
    }
    return cache;
}

void Persist(const Cache& cache) {
    std::ofstream f(FavoritesFilePath(), std::ios::trunc);
    if (!f.is_open()) return;
    for (const auto& [k, v] : cache.entries)
        f << k << "=" << v << "\n";
}

}

bool IsFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum) {
    auto& cache = GetCache();
    return cache.entries.count(MakeKey(bible, bookNum, chapterNum, verseNum)) != 0;
}

void ToggleFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum,
                     const std::string& text) {
    auto& cache = GetCache();
    std::string key = MakeKey(bible, bookNum, chapterNum, verseNum);
    auto it = cache.entries.find(key);
    if (it != cache.entries.end())
        cache.entries.erase(it);
    else
        cache.entries[key] = text;
    Persist(cache);
}

std::vector<FavoriteVerse> GetAll() {
    auto& cache = GetCache();
    std::vector<FavoriteVerse> out;
    for (const auto& [key, text] : cache.entries) {
        FavoriteVerse fv;
        fv.text = text;
        if (!SplitKey(key, fv.bible, fv.bookNum, fv.chapterNum, fv.verseNum)) continue;
        out.push_back(std::move(fv));
    }
    std::sort(out.begin(), out.end(), [](const FavoriteVerse& a, const FavoriteVerse& b) {
        if (a.bible != b.bible) return a.bible < b.bible;
        if (a.bookNum != b.bookNum) return a.bookNum < b.bookNum;
        if (a.chapterNum != b.chapterNum) return a.chapterNum < b.chapterNum;
        return a.verseNum < b.verseNum;
    });
    return out;
}

}

