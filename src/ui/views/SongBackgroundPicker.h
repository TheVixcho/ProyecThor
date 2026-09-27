#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

struct SongBgEntry {
    std::string fullPath;
    std::string label;
    bool        isImage = false;
};

std::vector<SongBgEntry> ListSongBackgrounds();

}

