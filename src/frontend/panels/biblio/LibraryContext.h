#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ProyecThor::UI { class MonitorView; }

namespace ProyecThor::Library {

struct LibraryContext
{
    // ── Categoria y lista ─────────────────────────────────────────────────
    int&                      currentCategoryInt;
    // Modo de vista del grupo aparte "Red"/"Reloj" (ver UI::LibrarySideMode
    // en LibraryPanel.h) — 0=Categorias, 1=Red, 2=Reloj. Independiente de
    // currentCategoryInt.
    int&                      sideModeInt;
    std::vector<std::string>& items;
    int&                      selectedIndex;
    char*                     searchBuffer;
    int                       searchBufferSize;

    // ── Stream URLs ───────────────────────────────────────────────────────
    std::vector<std::string>& streamURLs;
    int&                      selectedURLIndex;
    char*                     urlInputBuffer;
    int                       urlInputBufferSize;

    // ── Editor de canciones ───────────────────────────────────────────────
    bool&  showSongEditor;
    char*  editTitle;
    char*  editContent;
    char*  editAuthor;

    // ── Modal de renombrar ────────────────────────────────────────────────
    bool&        showRenameModal;
    std::string& renameOldName;
    std::string& renameExtension;
    char*        renameBuffer;
    bool&        renameIsURL;
    int&         renameURLIndex;

    // ── Documento cargado ─────────────────────────────────────────────────
    std::string& loadedDocPath;

    // ── Referencia al monitor (para cola de videos) ───────────────────────
    UI::MonitorView* monitorRef;

    // ── Callbacks hacia LibraryPanel ──────────────────────────────────────
    std::function<void()>                                       refreshList;
    std::function<void()>                                       deleteSelectedItem;
    std::function<void()>                                       importFile;
    std::function<void()>                                       createNewSong;
    std::function<void(const std::string&, const std::string&, const std::string&)> saveSong;
    std::function<void()>                                       loadStreamURLs;
    std::function<void()>                                       saveStreamURLs;
    std::function<std::vector<std::string>(const std::string&)> loadSongVerses;

    // Aplica un estilo guardado por nombre al canvas activo.
    // La lambda la registra LibraryPanel, que tiene acceso a PresentationCore
    // y sabe como cargar y aplicar un StyleData por nombre.
    // Si no hay estilo configurado o el nombre no existe, la lambda no hace nada.
    std::function<void(const std::string&)>                     applyStyle;

    // ── Playlists ─────────────────────────────────────────────────────────
    bool&        showPlaylistsTab;     // true = pestaña "Playlists" activa dentro de Canciones
    std::string& activePlaylistName;   // "" si no hay playlist activa
    int&         activePlaylistIndex;  // indice de la cancion activa dentro de esa playlist

    std::function<std::vector<std::string>()>                    listPlaylists;
    std::function<std::vector<std::string>(const std::string&)>  loadPlaylistSongs;
    std::function<bool(const std::string&)>                       createPlaylist;
    std::function<void(const std::string&)>                       deletePlaylist;
    std::function<bool(const std::string&, const std::string&)>   renamePlaylist;
    std::function<void(const std::string&, const std::string&)>   addSongToPlaylist;
    std::function<void(const std::string&, int)>                  removeSongFromPlaylist;
    std::function<void(const std::string&, int, int)>             movePlaylistSong;
    std::function<void(const std::string&, int)>                  selectPlaylistSong;

    // ── Etiquetas ─────────────────────────────────────────────────────────
    char* editTags; // buffer del modal de nueva cancion, separado por comas
    std::function<std::vector<std::string>(const std::string&)>                  getSongTags;
    std::function<void(const std::string&, const std::vector<std::string>&)>     setSongTags;
    std::function<void()>                                                        openPanelPicker;
};

} // namespace ProyecThor::Library