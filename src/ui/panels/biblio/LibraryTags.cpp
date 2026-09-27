#include "LibraryTags.h"
#include "LibraryHelpers.h"
#include "LibrarySongs.h" // GetSongTags

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ProyecThor::Library {

static std::string SongTagGroupsFilePath()
{
    return GetAssetsPath() + "/../song_tag_groups.ini";
}

std::string NormalizeTagId(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isalnum(ch)) out.push_back((char)std::tolower(ch));
        else if (ch == ' ' || ch == '_' || ch == '-' || ch == '/' || ch == '\\') {
            if (!out.empty() && out.back() != '_') out.push_back('_');
        }
    }
    if (out.empty()) out = "tag";
    return out;
}

std::vector<SongTagGroup> LoadSongTagGroups()
{
    std::vector<SongTagGroup> groups;
    std::ifstream f(U8Path(SongTagGroupsFilePath()));
    if (!f.is_open()) return groups;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        const std::string id = line.substr(0, sep);
        const std::string payload = line.substr(sep + 1);
        auto pipe = payload.find('|');
        SongTagGroup group;
        group.id = id;
        if (pipe != std::string::npos) {
            group.name = payload.substr(0, pipe);
            std::string rgb = payload.substr(pipe + 1);
            std::stringstream ss(rgb);
            std::string part;
            std::vector<float> values;
            while (std::getline(ss, part, ',')) {
                if (!part.empty()) values.push_back(std::stof(part));
            }
            if (values.size() >= 3) {
                group.color = ImVec4(values[0], values[1], values[2], 1.0f);
            }
        } else {
            group.name = payload;
        }
        groups.push_back(group);
    }

    std::sort(groups.begin(), groups.end(), [](const SongTagGroup& a, const SongTagGroup& b) {
        return a.name < b.name;
    });
    return groups;
}

void SaveSongTagGroups(const std::vector<SongTagGroup>& groups)
{
    std::ofstream f(U8Path(SongTagGroupsFilePath()));
    if (!f.is_open()) return;
    for (const auto& g : groups) {
        f << g.id << "=" << g.name << "|"
          << g.color.x << "," << g.color.y << "," << g.color.z << "\n";
    }
}

void SaveSongTagGroup(const SongTagGroup& group)
{
    auto groups = LoadSongTagGroups();
    auto it = std::find_if(groups.begin(), groups.end(),
        [&](const SongTagGroup& x){ return x.id == group.id; });
    if (it != groups.end()) *it = group;
    else                     groups.push_back(group);
    SaveSongTagGroups(groups);
}

void DeleteSongTagGroup(const std::string& id)
{
    auto groups = LoadSongTagGroups();
    groups.erase(std::remove_if(groups.begin(), groups.end(),
        [&](const SongTagGroup& g){ return g.id == id; }), groups.end());
    SaveSongTagGroups(groups);
}

SongTagGroup FindSongTagGroup(const std::string& id)
{
    SongTagGroup fallback;
    fallback.id = id;
    fallback.name = id;
    auto groups = LoadSongTagGroups();
    for (const auto& g : groups) {
        if (g.id == id) return g;
    }
    return fallback;
}

bool SongHasTag(const std::string& filename, const std::string& tagId)
{
    auto tags = GetSongTags(filename);
    return std::find(tags.begin(), tags.end(), tagId) != tags.end();
}

} // namespace ProyecThor::Library