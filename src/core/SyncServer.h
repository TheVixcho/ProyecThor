#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <vector>

#include "ui/views/biblia/BibleTypes.h"

namespace ProyecThor::Core {

// ── SyncServer ────────────────────────────────────────────────────────────────
// Servidor LAN para que la app movil companion (ProyecThor Mobile) mantenga
// sincronizada su copia de los datos con este PC, Y controle la presentacion
// en vivo como un remoto (estilo Holyrics). Vive en su propio hilo httplib
// (mismo patron que NetworkStreamServer), mas un segundo hilo UDP que
// responde al broadcast de descubrimiento del celular. Comparte UN solo
// puerto para ambos grupos de endpoints (mismo criterio que
// NetworkStreamServer multiplexando Streaming+Chat: un solo permiso de
// firewall, un solo pairing).
//
// Endpoints de sincronizacion de archivos:
//   GET  /sync/hello              -> identifica el equipo (requiere token)
//   GET  /sync/manifest           -> [{path,size,mtime}, ...] de RootDir
//   GET  /sync/file?path=...      -> descarga un archivo (bytes crudos)
//   PUT  /sync/file?path=...      -> sube/sobreescribe un archivo
// Esta parte es deliberadamente "tonta": no decide que sincronizar ni
// resuelve conflictos -- esa logica vive enteramente en la app movil. Nunca
// borra archivos, para no perder datos por un bug de sincronizacion.
//
// Endpoints de control remoto (ver seccion REMOTE en SyncServer.cpp): listar
// canciones/biblias/fondos, seleccionar cancion+diapositiva o libro/capitulo/
// verso y empujarlo a Layer2 (mismo camino que SongView::GoToStanza /
// BibleView::ProjectVerse), Siguiente/Anterior, Blank/Clear/Publico on-off,
// y elegir fondo/imagen/video -- todo llamando directo a
// Core::PresentationCore::Get(), igual que hace la UI de escritorio.
//
// Todos los endpoints requieren el header "X-Sync-Token" == GetPairingToken().
class SyncServer {
public:
    SyncServer();
    ~SyncServer();

    SyncServer(const SyncServer&)            = delete;
    SyncServer& operator=(const SyncServer&) = delete;

    // Carpeta raiz a sincronizar (normalmente AppPaths::GetAppDataRoot()).
    // Debe llamarse antes de Start().
    void SetRootDir(const std::string& dir);

    // PIN/token de emparejamiento. Puede cambiarse en caliente (ej. al
    // apretar "Regenerar PIN" en SyncPanel) sin reiniciar el servidor.
    void SetPairingToken(const std::string& token);
    std::string GetPairingToken() const;

    bool Start(int port = 8090);
    void Stop();

    bool        IsRunning() const { return m_Running.load(); }
    int         GetPort()   const { return m_Port; }
    std::string GetBaseURL() const { return m_BaseURL; }

    // Puerto UDP fijo donde este server escucha el broadcast de descubrimiento
    // de la app movil (ver DiscoveryThreadFunc).
    static constexpr int kDiscoveryPort = 47990;

private:
    void ServerThreadFunc(int port, std::promise<bool> startedPromise);
    void DiscoveryThreadFunc();
    static std::string DetectLocalIP();

    std::atomic<bool> m_Running{false};
    int                m_Port{8090};
    std::string        m_BaseURL;
    std::thread        m_HttpThread;

    std::atomic<bool> m_DiscoveryRunning{false};
    std::thread        m_DiscoveryThread;

    mutable std::mutex m_ConfigMutex;
    std::string         m_RootDir;
    std::string         m_PairingToken;

    // ── Estado de "control remoto" ────────────────────────────────────────
    // Que cancion/biblia esta actualmente seleccionada desde el celular, para
    // que Siguiente/Anterior no dependan de que el cliente reenvie todo cada
    // vez -- mismo rol que m_ActiveStanzaIndex en SongView (PC), pero del
    // lado del servidor en vez de un ImGui widget.
    enum class RemoteMode { None, Song, Bible };

    mutable std::mutex       m_RemoteMutex;
    RemoteMode                m_RemoteMode = RemoteMode::None;

    std::string               m_CurrentSongFile;
    std::vector<std::string>  m_CurrentSlides;
    int                       m_CurrentSlideIndex = -1;

    std::string               m_CachedBibleFile;
    ProyecThor::UI::BibleData m_CachedBible;
    int                       m_CurrentBibleBook    = -1;
    int                       m_CurrentBibleChapter = -1;
    int                       m_CurrentVerseIndex   = -1;
};

} // namespace ProyecThor::Core
