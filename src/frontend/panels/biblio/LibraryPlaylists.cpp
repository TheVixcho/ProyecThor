#include "LibraryPlaylists.h"
#include "LibraryHelpers.h"
#include "backend/core/FileDeletionManager.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace ProyecThor::Library {

static std::string PlaylistsDir()      { return GetAssetsPath() + "/playlists"; }
static std::string PlaylistFilePath(const std::string& name) { return PlaylistsDir() + "/" + name + ".playlist"; }

static void EnsurePlaylistsDir()
{
    std::error_code ec;
    fs::create_directories(U8Path(PlaylistsDir()), ec);
}

std::vector<std::string> ListPlaylists()
{
    EnsurePlaylistsDir();
    std::vector<std::string> names;

    std::error_code ec;
    fs::path dir = U8Path(PlaylistsDir());
    if (!fs::exists(dir, ec)) return names;

    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".playlist") continue;
        names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

Playlist LoadPlaylist(const std::string& name)
{
    Playlist pl;
    pl.name = name;

    std::ifstream f(U8Path(PlaylistFilePath(name)));
    if (!f.is_open()) return pl;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) pl.songs.push_back(line);
    }
    return pl;
}

void SavePlaylist(const Playlist& playlist)
{
    EnsurePlaylistsDir();
    std::ofstream f(U8Path(PlaylistFilePath(playlist.name)));
    if (!f.is_open()) {
        std::cerr << "[LibraryPlaylists] No se pudo guardar: " << playlist.name << '\n';
        return;
    }
    for (const auto& s : playlist.songs) f << s << '\n';
}

bool CreatePlaylist(const std::string& name)
{
    if (name.empty()) return false;
    EnsurePlaylistsDir();

    std::error_code ec;
    if (fs::exists(U8Path(PlaylistFilePath(name)), ec)) return false;

    Playlist pl; pl.name = name;
    SavePlaylist(pl);
    return true;
}

void DeletePlaylist(const std::string& name)
{
    Core::FileDeletionManager::ForceDeleteFile(PlaylistFilePath(name));
}

bool RenamePlaylist(const std::string& oldName, const std::string& newName)
{
    if (oldName.empty() || newName.empty() || oldName == newName) return false;

    std::error_code ec;
    fs::path oldPath = U8Path(PlaylistFilePath(oldName));
    fs::path newPath = U8Path(PlaylistFilePath(newName));

    if (!fs::exists(oldPath, ec) || fs::exists(newPath, ec)) return false;

    fs::rename(oldPath, newPath, ec);
    return !ec;
}

void AddSongToPlaylist(const std::string& playlistName, const std::string& songFilename)
{
    Playlist pl = LoadPlaylist(playlistName);
    if (std::find(pl.songs.begin(), pl.songs.end(), songFilename) != pl.songs.end())
        return;
    pl.songs.push_back(songFilename);
    SavePlaylist(pl);
}

void RemoveSongFromPlaylist(const std::string& playlistName, int index)
{
    Playlist pl = LoadPlaylist(playlistName);
    if (index < 0 || index >= (int)pl.songs.size()) return;
    pl.songs.erase(pl.songs.begin() + index);
    SavePlaylist(pl);
}

void MovePlaylistSong(const std::string& playlistName, int index, int delta)
{
    Playlist pl = LoadPlaylist(playlistName);
    int target = index + delta;
    if (index < 0 || index >= (int)pl.songs.size()) return;
    if (target < 0 || target >= (int)pl.songs.size()) return;
    std::swap(pl.songs[index], pl.songs[target]);
    SavePlaylist(pl);
}

void RenameSongInAllPlaylists(const std::string& oldFilename, const std::string& newFilename)
{
    if (oldFilename.empty() || newFilename.empty() || oldFilename == newFilename) return;

    for (const auto& name : ListPlaylists()) {
        Playlist pl = LoadPlaylist(name);
        bool changed = false;
        for (auto& s : pl.songs) {
            if (s == oldFilename) { s = newFilename; changed = true; }
        }
        if (changed) SavePlaylist(pl);
    }
}

} // namespace ProyecThor::Library