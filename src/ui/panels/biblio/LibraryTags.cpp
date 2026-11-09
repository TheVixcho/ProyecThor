#include "LibraryTags.h"
#include "LibraryHelpers.h"
#include "LibrarySongs.h"
#include "ui/views/biblia/BibleTextUtils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace ProyecThor::Library {

static std::string SongTagGroupsFilePath()
{
    return GetAssetsPath() + "/../song_tag_groups.ini";
}

std::string NormalizeTagId(const std::string& raw)
{
    std::string clean = ProyecThor::UI::TextUtils::StripAccents(ProyecThor::UI::TextUtils::ToLowerUTF8(raw));
    std::string out;
    out.reserve(clean.size());
    for (char ch : clean) {
        if (std::isalnum((unsigned char)ch)) {
            out.push_back(ch);
        } else if (ch == ' ' || ch == '_' || ch == '-' || ch == '/' || ch == '\\') {
            if (!out.empty() && out.back() != '_') out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
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
                if (!part.empty()) {
                    try {
                        values.push_back(std::stof(part));
                    } catch (...) {}
                }
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
    std::string path = SongTagGroupsFilePath();
    std::error_code ec;
    fs::create_directories(U8Path(path).parent_path(), ec);

    std::ofstream f(U8Path(path));
    if (!f.is_open()) return;
    for (const auto& g : groups) {
        f << g.id << "=" << g.name << "|"
          << g.color.x << "," << g.color.y << "," << g.color.z << "\n";
    }
    f.close();
    ClearSongIndexCache();
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

    // Limpiar referencias huérfanas en songs_tags.ini
    std::string path = GetAssetsPath() + "/../songs_tags.ini";
    std::ifstream fi(U8Path(path));
    if (fi.is_open()) {
        std::vector<std::pair<std::string, std::string>> lines;
        std::string line;
        bool changed = false;
        while (std::getline(fi, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto sep = line.find('=');
            if (sep == std::string::npos) continue;
            std::string file = line.substr(0, sep);
            std::string tagsStr = line.substr(sep + 1);

            std::stringstream ss(tagsStr);
            std::string t;
            std::vector<std::string> remaining;
            while (std::getline(ss, t, ',')) {
                size_t a = t.find_first_not_of(' ');
                size_t b = t.find_last_not_of(' ');
                if (a != std::string::npos) {
                    std::string trimmed = t.substr(a, b - a + 1);
                    if (trimmed != id) remaining.push_back(trimmed);
                    else changed = true;
                }
            }
            if (!remaining.empty()) {
                std::string remStr;
                for (size_t i = 0; i < remaining.size(); ++i) {
                    if (i) remStr += ",";
                    remStr += remaining[i];
                }
                lines.push_back({file, remStr});
            } else {
                changed = true;
            }
        }
        fi.close();

        if (changed) {
            std::ofstream fo(U8Path(path));
            if (fo.is_open()) {
                for (const auto& [f, ts] : lines) {
                    fo << f << "=" << ts << "\n";
                }
            }
        }
    }
    ClearSongIndexCache();
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