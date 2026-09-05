#pragma once

#include <string>
#include <vector>

namespace ProyecThor::UI::Favorites {

struct FavoriteVerse {
    std::string bible;
    int         bookNum    = 0;
    int         chapterNum = 0;
    int         verseNum   = 0;
    std::string text;
};

bool IsFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum);
void ToggleFavorite(const std::string& bible, int bookNum, int chapterNum, int verseNum,
                     const std::string& text);
std::vector<FavoriteVerse> GetAll();

}

