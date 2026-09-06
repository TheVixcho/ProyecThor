#pragma once
#include <string>
#include <vector>

namespace ProyecThor::Library {

// =============================================================================
//  Playlists — listas ordenadas de canciones, persistidas en
//  assets/playlists/<nombre>.playlist (una linea = un archivo de cancion).
//  Pensado para armar el set list del dia, como las Playlists de ProPresenter.
// =============================================================================

struct Playlist
{
    std::string              name;
    std::vector<std::string> songs; // nombres de archivo, en orden
};

std::vector<std::string> ListPlaylists();
Playlist                 LoadPlaylist(const std::string& name);
void                     SavePlaylist(const Playlist& playlist);

bool CreatePlaylist(const std::string& name);
void DeletePlaylist(const std::string& name);
bool RenamePlaylist(const std::string& oldName, const std::string& newName);

void AddSongToPlaylist(const std::string& playlistName, const std::string& songFilename);
void RemoveSongFromPlaylist(const std::string& playlistName, int index);
void MovePlaylistSong(const std::string& playlistName, int index, int delta); // delta: -1 sube, +1 baja

// Reemplaza <oldFilename> por <newFilename> en TODAS las playlists que lo
// referencien (usado al renombrar una cancion) — sin esto, renombrar un
// archivo de cancion deja la referencia vieja en cada playlist apuntando a
// un archivo que ya no existe, y la canción "desaparece" de esa playlist.
void RenameSongInAllPlaylists(const std::string& oldFilename, const std::string& newFilename);

} // namespace ProyecThor::Library