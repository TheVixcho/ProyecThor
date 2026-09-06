// SyncServer.cpp
//
// Ver SyncServer.h para el contrato de la API. Implementacion basada en
// cpp-httplib (igual que NetworkStreamServer.cpp) para el servidor HTTP, y
// en sockets UDP crudos (igual que OSCReceiver.cpp) para el descubrimiento.

#include "SyncServer.h"
#include "httplib.h"
#include "AppPaths.h"
#include "PresentationCore.h"
#include "frontend/panels/biblio/LibrarySongs.h"
#include "frontend/panels/biblio/LibrarySongMeta.h"
#include "frontend/views/biblia/BibleXmlIO.h"
#include "frontend/views/biblia/BibleSearch.h"
#include "frontend/views/biblia/BibleTextUtils.h"
#include "frontend/views/Audio.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "frontend/panels/overlay/OverlayRecipeIO.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#   pragma comment(lib, "iphlpapi.lib")
#   pragma comment(lib, "ws2_32.lib")
    using SocketHandle = SOCKET;
    static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#   include <ifaddrs.h>
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <sys/socket.h>
#   include <unistd.h>
    using SocketHandle = int;
    static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace ProyecThor::Core {

// Archivos claramente transitorios/de esta maquina que no tiene sentido
// llevar de un lado a otro (estado de splash, contador de builds, historial
// de rendimiento). Todo lo demas bajo RootDir se sincroniza.
static const char* kExcludedFiles[] = {
    "build_count.txt",
    "splash_state.txt",
    "app_performance_history.txt",
};

static bool IsExcludedFile(const std::string& filename) {
    for (const char* ex : kExcludedFiles)
        if (filename == ex) return true;
    return false;
}

// ── Conversion file_time_type <-> epoch ms ────────────────────────────────────
// std::filesystem::file_time_type no es directamente convertible a
// system_clock antes de C++20 (sin clock_cast). Este es el workaround
// estandar: usar el offset entre "ahora" de ambos relojes.
static int64_t FileTimeToEpochMs(fs::file_time_type ftime) {
    using namespace std::chrono;
    auto sctp = system_clock::now() + duration_cast<system_clock::duration>(ftime - fs::file_time_type::clock::now());
    return duration_cast<milliseconds>(sctp.time_since_epoch()).count();
}

static fs::file_time_type EpochMsToFileTime(int64_t ms) {
    using namespace std::chrono;
    system_clock::time_point sctp{milliseconds(ms)};
    auto delta = sctp - system_clock::now();
    return fs::file_time_type::clock::now() + duration_cast<fs::file_time_type::duration>(delta);
}

// ── Path traversal safety ─────────────────────────────────────────────────────
// Resuelve relPath contra root y verifica que el resultado canonico siga
// estando DENTRO de root -- rechaza "..", rutas absolutas, etc.
static bool ResolveSafePath(const std::string& root, const std::string& relIn, std::string& outFull) {
    if (relIn.empty()) return false;

    std::string rel = relIn;
    for (auto& c : rel) if (c == '\\') c = '/';
    if (rel.front() == '/') return false;                 // ruta absoluta unix
    if (rel.size() > 1 && rel[1] == ':') return false;     // "C:\..." / "C:/..."

    fs::path full = fs::path(root) / fs::path(rel);

    std::error_code ec;
    fs::path canonRoot = fs::weakly_canonical(root, ec);
    if (ec) return false;
    fs::path canonFull = fs::weakly_canonical(full, ec);
    if (ec) return false;

    std::string rootStr = canonRoot.generic_string();
    std::string fullStr = canonFull.generic_string();
    if (fullStr.size() < rootStr.size()) return false;
    if (fullStr.compare(0, rootStr.size(), rootStr) != 0) return false;
    // Debe ser el propio root o un hijo (root + '/' + algo), no un prefijo
    // parcial casual como "root_otro".
    if (fullStr.size() > rootStr.size() && fullStr[rootStr.size()] != '/') return false;

    outFull = full.string();
    return true;
}

// ── REMOTE: helpers de listado/formato (control remoto estilo Holyrics) ──────
namespace {

struct RemoteSongEntry { std::string file; std::string title; };

std::vector<RemoteSongEntry> ListRemoteSongs() {
    std::vector<RemoteSongEntry> out;
    std::error_code ec;
    fs::path dir = ProyecThor::SongsPath();
    if (!fs::exists(dir, ec)) return out;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        std::error_code fec;
        if (!entry.is_regular_file(fec) || fec) continue;
        if (entry.path().extension() != ".txt") continue;
        std::string file = entry.path().filename().string();
        out.push_back({ file, ProyecThor::Library::GetSongDisplayName(file) });
    }
    std::sort(out.begin(), out.end(), [](const RemoteSongEntry& a, const RemoteSongEntry& b) {
        return a.title < b.title;
    });
    return out;
}

std::vector<std::string> ListRemoteBibleFiles() {
    std::vector<std::string> out;
    std::error_code ec;
    fs::path dir = ProyecThor::BiblesPath();
    if (!fs::exists(dir, ec)) return out;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        std::error_code fec;
        if (!entry.is_regular_file(fec) || fec) continue;
        if (entry.path().extension() != ".xml") continue;
        out.push_back(entry.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

struct RemoteMediaEntry { std::string folder; std::string filename; bool isVideo; };

bool IsVideoExt(const std::string& ext) {
    static const std::vector<std::string> kVideo = { ".mp4", ".mov", ".mkv", ".avi", ".webm" };
    return std::find(kVideo.begin(), kVideo.end(), ext) != kVideo.end();
}

std::vector<RemoteMediaEntry> ListRemoteBackgrounds() {
    std::vector<RemoteMediaEntry> out;
    for (const char* folder : { "backgrounds", "images" }) {
        std::error_code ec;
        fs::path dir = fs::path(ProyecThor::GetAssetsPath()) / folder;
        if (!fs::exists(dir, ec)) continue;
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            std::error_code fec;
            if (!entry.is_regular_file(fec) || fec) continue;
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            out.push_back({ folder, entry.path().filename().string(), IsVideoExt(ext) });
        }
    }
    return out;
}

// ── Multimedia (video/audio/imagen) — mismo criterio de carpetas/extensiones
// que LibraryMultimedia.cpp (ScanFolder/VideoFolder/ImageFolder/AudioFolder),
// duplicado aca a proposito: ese archivo guarda sus listas en variables
// estaticas privadas del modulo, sin una funcion exportada para consultarlas
// desde afuera.
struct RemoteMultimediaEntry { std::string filename; std::string type; };

std::vector<RemoteMultimediaEntry> ListRemoteMultimedia() {
    std::vector<RemoteMultimediaEntry> out;

    auto scan = [&out](const std::string& folder, const std::vector<std::string>& exts, const char* type) {
        std::error_code ec;
        fs::path dir(folder);
        if (!fs::exists(dir, ec)) return;
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            std::error_code fec;
            if (!entry.is_regular_file(fec) || fec) continue;
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;
            out.push_back({ entry.path().filename().string(), type });
        }
    };

    scan(ProyecThor::GetAssetsPath() + "/videos", { ".mp4", ".mkv", ".avi", ".mov" }, "video");
    scan(ProyecThor::GetAssetsPath() + "/images", { ".jpg", ".jpeg", ".png" }, "image");
    scan(ProyecThor::Audio::GetAudioPath(),
         { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" }, "audio");

    return out;
}

// ── Overlays — mismo criterio que OverlayLibraryTab::ReloadList (requiere
// ".overlay" + ".png" existentes), pero usando OverlayRecipeIO directamente
// en vez de una instancia de OverlayLibraryTab (que es dueño de la UI, uno
// por proceso, y no expone su lista). El PNG de cada uno se sirve con el
// endpoint generico ya existente GET /sync/file?path=assets/overlays/<name>.png.
struct RemoteOverlayEntry { std::string name; int canvasW = 1920, canvasH = 1080; bool hasClock = false; };

std::vector<RemoteOverlayEntry> ListRemoteOverlays() {
    std::vector<RemoteOverlayEntry> out;
    fs::path dir = ProyecThor::OverlaysPath();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        std::error_code fec;
        if (!entry.is_regular_file(fec) || fec) continue;
        if (entry.path().extension() != ".overlay") continue;

        std::string name = entry.path().stem().string();
        if (!fs::exists(dir / (name + ".png"), fec)) continue;

        RemoteOverlayEntry re;
        re.name = name;
        ProyecThor::UI::OverlayDoc doc;
        if (ProyecThor::UI::LoadOverlayRecipe(dir, name, doc)) {
            re.canvasW  = doc.canvasW;
            re.canvasH  = doc.canvasH;
            re.hasClock = ProyecThor::UI::FindClockLayer(doc) != nullptr;
        }
        out.push_back(std::move(re));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

// Mismo formato que BibleView::BuildVerseRef (PC): solo "Libro Cap:Verso",
// sin el nombre de la Biblia -- es lo que se manda a SetCurrentRef, que
// dibuja SOLO la referencia en la caja del Indice (opcional, independiente
// de la caja de Letras donde va el cuerpo del versiculo).
std::string BuildVerseRef(const ProyecThor::UI::BookData& book,
                           const ProyecThor::UI::ChapterData& chap,
                           const ProyecThor::UI::VerseData& verse) {
    return book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
}

// Formato concatenado legacy "Libro Cap:Verso (Biblia)\ntexto" -- se sigue
// usando SOLO para el "siguiente" del Stage Display (SetNextText), que no
// pasa por las cajas de Letras/Indice.
std::string BuildProjectedVerseText(const ProyecThor::UI::BookData& book,
                                     const ProyecThor::UI::ChapterData& chap,
                                     const ProyecThor::UI::VerseData& verse,
                                     const std::string& bibleName) {
    return BuildVerseRef(book, chap, verse) + " (" + bibleName + ")\n" + verse.text;
}

// ── Formato de "estilo" para la app movil ─────────────────────────────────
// Mismos campos que Core::SavedStyle/TextEffectsData (ver PresentationCore.h)
// pero como JSON explicito en vez de las lineas "key=value" de los .theme en
// disco -- asi la app movil no tiene que parsear el formato interno del PC,
// solo consumir un objeto con nombres de campo estables. Usado tanto por
// /remote/styles (catalogo completo) como por /remote/songs/style (estilo
// resuelto de una cancion puntual).
json TextEffectsToJson(const Core::TextEffectsData& e) {
    auto colorArr = [](const float c[4]) { return json::array({ c[0], c[1], c[2], c[3] }); };
    json fx;
    fx["background"]  = { {"enabled", e.bgEnabled},     {"color", colorArr(e.bgColor)} };
    fx["border"]      = { {"enabled", e.borderEnabled}, {"color", colorArr(e.borderColor)}, {"width", e.borderWidth} };
    fx["shadow"]      = { {"enabled", e.shadowEnabled}, {"color", colorArr(e.shadowColor)}, {"intensity", e.shadowIntensity} };
    fx["chromaticAberration"] = { {"enabled", e.chromaticAberrationEnabled}, {"intensity", e.chromaticAberrationIntensity} };
    fx["glow"]        = { {"enabled", e.glowEnabled},      {"color", colorArr(e.glowColor)},      {"intensity", e.glowIntensity} };
    fx["neon"]        = { {"enabled", e.neonEnabled},      {"color", colorArr(e.neonColor)},      {"intensity", e.neonIntensity} };
    fx["underline"]   = { {"enabled", e.underlineEnabled}, {"color", colorArr(e.underlineColor)}, {"thickness", e.underlineThickness} };
    return fx;
}

json StyleToJson(const std::string& name, const Core::SavedStyle& s) {
    json j;
    j["name"]      = name;
    j["size"]      = s.size;
    j["color"]     = json::array({ s.color[0], s.color[1], s.color[2], s.color[3] });
    j["hAlign"]    = s.hAlign; // 0=izq, 1=centro, 2=der (mismo enum que el editor de Estilos)
    j["vAlign"]    = s.vAlign; // 0=arriba, 1=centro, 2=abajo
    j["margins"]   = json::array({ s.margins[0], s.margins[1], s.margins[2], s.margins[3] }); // top,bottom,left,right
    j["autoScale"] = s.autoScale;
    j["font"]      = s.fontName; // "Predeterminada" o el nombre de archivo (sin extension) en assets/fonts
    j["effects"]   = TextEffectsToJson(s.effects);
    return j;
}

} // namespace (anonimo)

// ── Constructor / Destructor ──────────────────────────────────────────────────
SyncServer::SyncServer()  = default;
SyncServer::~SyncServer() { Stop(); }

void SyncServer::SetRootDir(const std::string& dir) {
    std::lock_guard<std::mutex> lk(m_ConfigMutex);
    m_RootDir = dir;
}

void SyncServer::SetPairingToken(const std::string& token) {
    std::lock_guard<std::mutex> lk(m_ConfigMutex);
    m_PairingToken = token;
}

std::string SyncServer::GetPairingToken() const {
    std::lock_guard<std::mutex> lk(m_ConfigMutex);
    return m_PairingToken;
}

// ── Start / Stop ───────────────────────────────────────────────────────────────
bool SyncServer::Start(int port) {
    if (m_Running.load()) return true;

    {
        std::lock_guard<std::mutex> lk(m_ConfigMutex);
        if (m_RootDir.empty()) m_RootDir = ProyecThor::GetAppDataRoot();
        std::error_code ec;
        fs::create_directories(m_RootDir, ec);
    }

    m_Port    = port;
    m_BaseURL = "http://" + DetectLocalIP() + ":" + std::to_string(port);

    std::promise<bool> startedPromise;
    std::future<bool>  startedFuture = startedPromise.get_future();

    m_Running.store(true);
    m_HttpThread = std::thread([this, port, p = std::move(startedPromise)]() mutable {
        ServerThreadFunc(port, std::move(p));
    });

    bool ok = startedFuture.wait_for(std::chrono::seconds(3)) ==
              std::future_status::ready && startedFuture.get();

    if (!ok) {
        m_Running.store(false);
        if (m_HttpThread.joinable()) m_HttpThread.join();
        std::cerr << "[Sync] No se pudo escuchar en puerto " << port << "\n";
        return false;
    }

    m_DiscoveryRunning.store(true);
    m_DiscoveryThread = std::thread(&SyncServer::DiscoveryThreadFunc, this);

    std::cout << "[Sync] Servidor iniciado en " << m_BaseURL << "\n";
    return true;
}

void SyncServer::Stop() {
    if (!m_Running.load() && !m_DiscoveryRunning.load()) return;

    m_Running.store(false);
    if (m_HttpThread.joinable()) m_HttpThread.join();

    m_DiscoveryRunning.store(false);
    if (m_DiscoveryThread.joinable()) m_DiscoveryThread.join();

    std::cout << "[Sync] Servidor detenido.\n";
}

// ── ServerThreadFunc ──────────────────────────────────────────────────────────
void SyncServer::ServerThreadFunc(int port, std::promise<bool> startedPromise) {
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(8); };

    auto checkToken = [this](const httplib::Request& req, httplib::Response& res) -> bool {
        std::string expected;
        { std::lock_guard<std::mutex> lk(m_ConfigMutex); expected = m_PairingToken; }
        std::string got = req.has_header("X-Sync-Token") ? req.get_header_value("X-Sync-Token") : "";
        if (expected.empty() || got != expected) {
            res.status = 401;
            res.set_content(R"({"error":"invalid_token"})", "application/json");
            return false;
        }
        return true;
    };

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[Sync] " << req.method << " " << req.path
                  << " -> " << res.status << " (" << req.remote_addr << ")\n";
    });

    // ── GET /sync/hello ───────────────────────────────────────────────────────
    svr.Get("/sync/hello", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        char hostname[256] = {};
        gethostname(hostname, sizeof(hostname));
        json j;
        j["ok"]   = true;
        j["app"]  = "ProyecThor";
        j["name"] = std::string(hostname);
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── GET /sync/manifest ────────────────────────────────────────────────────
    svr.Get("/sync/manifest", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;

        std::string root;
        { std::lock_guard<std::mutex> lk(m_ConfigMutex); root = m_RootDir; }

        json arr = json::array();
        std::error_code ec;
        if (fs::exists(root, ec)) {
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
            fs::recursive_directory_iterator end;
            for (; !ec && it != end; it.increment(ec)) {
                std::error_code fec;
                if (!it->is_regular_file(fec) || fec) continue;

                std::string filename = it->path().filename().string();
                if (IsExcludedFile(filename)) continue;

                fs::path relPath = fs::relative(it->path(), root, fec);
                if (fec) continue;

                uintmax_t size = it->file_size(fec);
                if (fec) continue;

                auto ftime = it->last_write_time(fec);
                if (fec) continue;

                json e;
                e["path"]  = relPath.generic_string();
                e["size"]  = static_cast<uint64_t>(size);
                e["mtime"] = FileTimeToEpochMs(ftime);
                arr.push_back(std::move(e));
            }
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /sync/file?path=... ───────────────────────────────────────────────
    svr.Get("/sync/file", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("path")) { res.status = 400; return; }

        std::string root;
        { std::lock_guard<std::mutex> lk(m_ConfigMutex); root = m_RootDir; }

        std::string full;
        if (!ResolveSafePath(root, req.get_param_value("path"), full)) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        std::error_code ec;
        if (!fs::exists(full, ec) || !fs::is_regular_file(full, ec)) {
            res.status = 404;
            res.set_content(R"({"error":"not_found"})", "application/json");
            return;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_file_content(full, "application/octet-stream");
    });

    // ── PUT /sync/file?path=...  (body = bytes crudos, streamed) ─────────────
    svr.Put("/sync/file", [this, &checkToken](const httplib::Request& req, httplib::Response& res,
                                                const httplib::ContentReader& contentReader) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("path")) {
            res.status = 400;
            res.set_content(R"({"error":"missing_path"})", "application/json");
            return;
        }

        std::string root;
        { std::lock_guard<std::mutex> lk(m_ConfigMutex); root = m_RootDir; }

        std::string full;
        if (!ResolveSafePath(root, req.get_param_value("path"), full)) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        std::error_code ec;
        fs::create_directories(fs::path(full).parent_path(), ec);

        std::string tmpPath = full + ".part";
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            res.status = 500;
            res.set_content(R"({"error":"cannot_open"})", "application/json");
            return;
        }

        bool ok = contentReader([&](const char* data, size_t len) {
            out.write(data, static_cast<std::streamsize>(len));
            return static_cast<bool>(out);
        });
        out.close();

        if (!ok) {
            std::error_code rmEc;
            fs::remove(tmpPath, rmEc);
            res.status = 500;
            res.set_content(R"({"error":"write_failed"})", "application/json");
            return;
        }

        fs::remove(full, ec); // reemplazo -- rename falla si el destino ya existe en algunos SO
        fs::rename(tmpPath, full, ec);
        if (ec) {
            res.status = 500;
            res.set_content(R"({"error":"rename_failed"})", "application/json");
            return;
        }

        if (req.has_header("X-Mtime")) {
            try {
                int64_t ms = std::stoll(req.get_header_value("X-Mtime"));
                std::error_code mtEc;
                fs::last_write_time(full, EpochMsToFileTime(ms), mtEc);
            } catch (...) { /* header invalido: se deja el mtime "ahora" */ }
        }

        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── REMOTE: helpers locales (capturan `this` para el estado de nav) ──────
    auto ensureBibleCached = [this](const std::string& filename) -> bool {
        std::lock_guard<std::mutex> lk(m_RemoteMutex);
        if (m_CachedBibleFile == filename && !m_CachedBible.books.empty()) return true;
        std::string path = ProyecThor::BiblesPath() + filename;
        ProyecThor::UI::BibleData bible;
        if (!ProyecThor::UI::XmlIO::LoadBible(path, bible)) return false;
        m_CachedBible     = std::move(bible);
        m_CachedBibleFile = filename;
        return true;
    };

    auto findBook = [](const ProyecThor::UI::BibleData& bible, int number) -> const ProyecThor::UI::BookData* {
        for (const auto& b : bible.books) if (b.canonicalNumber == number) return &b;
        return nullptr;
    };

    // ── GET /remote/status ────────────────────────────────────────────────────
    svr.Get("/remote/status", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;

        auto& core  = Core::PresentationCore::Get();
        auto  state = core.GetState();

        json j;
        j["ok"]           = true;
        j["isProjecting"] = state.isProjecting;
        j["showText"]     = state.showText;
        j["currentText"]  = state.currentText;
        j["bgType"]       = state.bgType == PresentationState::BackgroundType::Video   ? "video"
                          : state.bgType == PresentationState::BackgroundType::Audio   ? "audio"
                                                                                        : "solid";
        j["bgPath"]       = fs::path(state.bgPath).filename().string();

        {
            std::lock_guard<std::mutex> lk(m_RemoteMutex);
            if (m_RemoteMode == RemoteMode::Song) {
                j["song"] = { {"file", m_CurrentSongFile}, {"slideIndex", m_CurrentSlideIndex},
                              {"slideCount", (int)m_CurrentSlides.size()} };
            } else if (m_RemoteMode == RemoteMode::Bible) {
                j["bible"] = { {"file", m_CachedBibleFile}, {"book", m_CurrentBibleBook},
                               {"chapter", m_CurrentBibleChapter}, {"verseIndex", m_CurrentVerseIndex} };
            }
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/network-status ────────────────────────────────────────────
    // Le informa al celular si la Transmision en Red y/o el Chat de equipo
    // (Ajustes > Red, ver NetworkStreamServer.h) estan activos AHORA MISMO y
    // en que puerto -- asi el celular se conecta directo a esas paginas (que
    // ya sirve ese servidor, sin token, pensadas para cualquier navegador de
    // la LAN) sin que el operador tenga que tipear una IP/puerto a mano.
    // Streaming y Chat comparten el mismo NetworkStreamServer/puerto pero
    // pueden estar activos independientemente uno del otro.
    svr.Get("/remote/network-status", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;

        auto& core = Core::PresentationCore::Get();
        auto* net  = core.GetNetworkServer();
        bool  up   = net && net->IsRunning();

        json j;
        j["streaming"] = {
            {"active", up && core.IsStreamingNet()},
            {"port",   up ? net->GetPort() : 0},
            {"url",    up ? net->GetBaseURL() : ""},
        };
        j["chat"] = {
            {"active", up && core.IsChatRunning()},
            {"port",   up ? net->GetPort() : 0},
            {"url",    up ? (net->GetBaseURL() + "/chat") : ""},
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/songs ──────────────────────────────────────────────────────
    svr.Get("/remote/songs", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        json arr = json::array();
        for (const auto& s : ListRemoteSongs()) arr.push_back({ {"file", s.file}, {"title", s.title} });
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/songs/slides?file=... ──────────────────────────────────────
    svr.Get("/remote/songs/slides", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file")) { res.status = 400; return; }
        auto slides = ProyecThor::Library::LoadSongVerses(req.get_param_value("file"));
        json j; j["slides"] = slides;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/styles ──────────────────────────────────────────────────────
    // Catalogo completo de estilos guardados (Diseño > Estilos en el PC) en
    // formato JSON explicito -- ver StyleToJson/TextEffectsToJson arriba. Pensado
    // para que la app movil pueda dibujar un preview fiel de cada estilo (o de
    // la cancion seleccionada, ver /remote/songs/style) sin duplicar a mano la
    // logica de renderizado del PC.
    svr.Get("/remote/styles", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        auto& core = Core::PresentationCore::Get();
        json arr = json::array();
        for (const auto& name : core.GetSavedStyleNames()) {
            Core::SavedStyle s;
            if (core.GetSavedStyle(name, s)) arr.push_back(StyleToJson(name, s));
        }
        json j; j["styles"] = arr;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/styles/select?name=... ───────────────────────────────────
    // Aplica un estilo guardado directo a lo que esta en vivo AHORA (misma
    // accion que elegirlo desde el combo de estilos en la PC) -- independiente
    // de que cancion/biblia este seleccionada. Ver
    // Core::PresentationCore::ApplyStyleByName.
    svr.Post("/remote/styles/select", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("name")) { res.status = 400; return; }
        Core::PresentationCore::Get().ApplyStyleByName(req.get_param_value("name"));
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /remote/songs/style?file=... ──────────────────────────────────────
    // Estilo asignado a UNA cancion puntual (tarjeta "Ajustes" del grid de
    // estrofas en SongView, ver Library::GetSongStyle/GetSongBackground) --
    // "styleName" vacio significa que la cancion no tiene un estilo propio
    // guardado y el PC usaria el default de categoria (Ajustes > Biblioteca)
    // en su lugar. "style" solo viene si styleName no esta vacio Y ese
    // nombre todavia existe en el catalogo (pudo haberse borrado).
    svr.Get("/remote/songs/style", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file")) { res.status = 400; return; }
        std::string file = req.get_param_value("file");

        json j;
        std::string styleName = ProyecThor::Library::GetSongStyle(file);
        j["styleName"] = styleName;

        if (!styleName.empty()) {
            Core::SavedStyle s;
            if (Core::PresentationCore::Get().GetSavedStyle(styleName, s))
                j["style"] = StyleToJson(styleName, s);
        }

        auto bg = ProyecThor::Library::GetSongBackground(file);
        if (!bg.path.empty())
            j["background"] = { {"filename", fs::path(bg.path).filename().string()}, {"isVideo", bg.isVideo} };

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/songs/style?file=...&styleName=... ──────────────────────
    // Asigna el preset de estilo de UNA cancion desde el celular (misma accion
    // que elegirlo en la tarjeta "Ajustes" del grid de estrofas en SongView) --
    // styleName vacio limpia el preset (ver Library::SetSongStyle).
    svr.Post("/remote/songs/style", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file")) { res.status = 400; return; }
        std::string file      = req.get_param_value("file");
        std::string styleName = req.has_param("styleName") ? req.get_param_value("styleName") : "";

        ProyecThor::Library::SetSongStyle(file, styleName);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── POST /remote/songs/background?file=...&folder=...&filename=...&isVideo=0|1
    // Asigna el fondo preset de UNA cancion. filename vacio limpia el preset
    // (ver Library::SetSongBackground) -- mismo criterio de folder/filename
    // que /remote/backgrounds/select para no permitir path traversal.
    svr.Post("/remote/songs/background", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file")) { res.status = 400; return; }
        std::string file     = req.get_param_value("file");
        std::string filename = req.has_param("filename") ? req.get_param_value("filename") : "";

        if (filename.empty()) {
            ProyecThor::Library::SetSongBackground(file, "", false);
            res.set_content(R"({"ok":true})", "application/json");
            return;
        }

        std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
        if ((folder != "backgrounds" && folder != "images") ||
            filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        bool isVideo = req.has_param("isVideo") && req.get_param_value("isVideo") == "1";
        std::string fullPath = ProyecThor::GetAssetsPath() + "/" + folder + "/" + filename;
        ProyecThor::Library::SetSongBackground(file, fullPath, isVideo);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── POST /remote/songs/select?file=...&index=N ────────────────────────────
    svr.Post("/remote/songs/select", [this, &checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file")) { res.status = 400; return; }

        std::string file = req.get_param_value("file");
        auto slides = ProyecThor::Library::LoadSongVerses(file);
        if (slides.empty()) {
            res.status = 404;
            res.set_content(R"({"error":"song_empty_or_missing"})", "application/json");
            return;
        }

        int idx = 0;
        if (req.has_param("index")) try { idx = std::stoi(req.get_param_value("index")); } catch (...) {}
        idx = std::clamp(idx, 0, (int)slides.size() - 1);

        Core::LibrarySelection sel;
        sel.title = ProyecThor::Library::GetSongDisplayName(file);
        sel.type  = Core::ItemType::Song;
        sel.contentData = slides;

        auto& core = Core::PresentationCore::Get();
        core.SetSelection(sel);

        // Mismo criterio que LibrarySongs::ApplyDefaultStyleIfSet (PC): el
        // estilo/fondo propio de la cancion tiene prioridad; sin nada
        // guardado, cae al default de categoria -- sin esto, seleccionar una
        // cancion desde el celular ignoraba su estilo/fondo preferido y
        // dejaba lo que ya estuviera puesto en el PC.
        {
            std::string styleName = ProyecThor::Library::GetSongStyle(file);
            if (styleName.empty()) styleName = core.GetCategoryDefaultStyle(Core::ItemType::Song);
            if (!styleName.empty()) core.ApplyStyleByName(styleName);

            auto bg = ProyecThor::Library::GetSongBackground(file);
            if (!bg.path.empty()) core.SetBackgroundMedia(bg.path, bg.isVideo, false);
        }

        core.SetLayer2_Text(slides[idx]);
        core.SetNextText(idx + 1 < (int)slides.size() ? slides[idx + 1] : "");
        core.SetProjecting(true);

        {
            std::lock_guard<std::mutex> lk(m_RemoteMutex);
            m_RemoteMode        = RemoteMode::Song;
            m_CurrentSongFile   = file;
            m_CurrentSlides     = slides;
            m_CurrentSlideIndex = idx;
        }

        json j; j["ok"] = true; j["slideIndex"] = idx; j["slideCount"] = (int)slides.size();
        res.set_content(j.dump(), "application/json");
    });

    // ── POST /remote/next / /remote/prev ───────────────────────────────────────
    auto stepSlideOrVerse = [this](int delta, httplib::Response& res) {
        auto& core = Core::PresentationCore::Get();
        std::lock_guard<std::mutex> lk(m_RemoteMutex);

        if (m_RemoteMode == RemoteMode::Song && !m_CurrentSlides.empty()) {
            int idx = std::clamp(m_CurrentSlideIndex + delta, 0, (int)m_CurrentSlides.size() - 1);
            m_CurrentSlideIndex = idx;
            core.SetLayer2_Text(m_CurrentSlides[idx]);
            core.SetNextText(idx + 1 < (int)m_CurrentSlides.size() ? m_CurrentSlides[idx + 1] : "");
            json j; j["ok"] = true; j["slideIndex"] = idx; j["slideCount"] = (int)m_CurrentSlides.size();
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (m_RemoteMode == RemoteMode::Bible) {
            for (const auto& b : m_CachedBible.books) {
                if (b.canonicalNumber != m_CurrentBibleBook) continue;
                for (const auto& c : b.chapters) {
                    if (c.number != m_CurrentBibleChapter) continue;
                    if (c.verses.empty()) break;
                    int idx = std::clamp(m_CurrentVerseIndex + delta, 0, (int)c.verses.size() - 1);
                    m_CurrentVerseIndex = idx;
                    std::string ref  = BuildVerseRef(b, c, c.verses[idx]);
                    std::string next = idx + 1 < (int)c.verses.size()
                        ? BuildProjectedVerseText(b, c, c.verses[idx + 1], m_CachedBible.name) : "";
                    core.SetLayer2_Text(c.verses[idx].text);
                    core.SetCurrentRef(ref);
                    core.SetNextText(next);
                    json j; j["ok"] = true; j["verseIndex"] = idx; j["verseCount"] = (int)c.verses.size();
                    res.set_content(j.dump(), "application/json");
                    return;
                }
            }
        }

        res.status = 409;
        res.set_content(R"({"error":"nothing_selected"})", "application/json");
    };

    svr.Post("/remote/next", [this, &checkToken, &stepSlideOrVerse](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        stepSlideOrVerse(1, res);
    });
    svr.Post("/remote/prev", [this, &checkToken, &stepSlideOrVerse](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        stepSlideOrVerse(-1, res);
    });

    // ── POST /remote/blank | /remote/clear-all | /remote/live?on=0|1 ──────────
    svr.Post("/remote/blank", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        Core::PresentationCore::Get().ClearLayer2();
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Post("/remote/clear-all", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        auto& core = Core::PresentationCore::Get();
        core.ClearLayer2();
        core.StopBackgroundMedia();
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Post("/remote/live", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        bool on = req.has_param("on") && req.get_param_value("on") == "1";
        Core::PresentationCore::Get().SetProjecting(on);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── POST /remote/clock-message?text=... ─────────────────────────────────
    // "Mensaje al publico" desde Reloj y Contadores: agrega `text` a la
    // lista de titulos de OClock (m_Titles), igual que si el operador
    // hubiese escrito el mensaje y apretado "Agregar" en el panel de
    // escritorio -- pero activandolo de inmediato, ya que el sentido de
    // "enviar" desde el celular es que se vea en el momento (a diferencia
    // de "Agregar" en escritorio, que no cambia la seleccion activa). Solo
    // se transmite de verdad si Reloj y Contadores esta en modo Pantalla/
    // Solo LAN/Ambos en ese momento (mismo comportamiento que si se
    // hubiese escrito a mano). Encolado thread-safe (ver
    // PresentationCore::PushRemoteClockTitle) porque este handler corre en
    // el hilo httplib, no en el hilo de UI -- OClock::Update() lo drena una
    // vez por frame.
    svr.Post("/remote/clock-message", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::string text = req.has_param("text") ? req.get_param_value("text") : "";
        if (text.empty()) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"Mensaje vacio"})", "application/json");
            return;
        }
        Core::PresentationCore::Get().PushRemoteClockTitle(text);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /remote/bibles ─────────────────────────────────────────────────────
    svr.Get("/remote/bibles", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        json arr = ListRemoteBibleFiles();
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/bibles/books?file=... ─────────────────────────────────────
    svr.Get("/remote/bibles/books", [this, &checkToken, &ensureBibleCached](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file") || !ensureBibleCached(req.get_param_value("file"))) {
            res.status = 404;
            res.set_content(R"({"error":"bible_not_found"})", "application/json");
            return;
        }
        std::lock_guard<std::mutex> lk(m_RemoteMutex);
        json arr = json::array();
        for (const auto& b : m_CachedBible.books)
            arr.push_back({ {"number", b.canonicalNumber}, {"name", b.name}, {"chapterCount", (int)b.chapters.size()} });
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/bibles/chapters?file=...&book=N ───────────────────────────
    svr.Get("/remote/bibles/chapters", [this, &checkToken, &ensureBibleCached, &findBook](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file") || !req.has_param("book") || !ensureBibleCached(req.get_param_value("file"))) {
            res.status = 400; return;
        }
        int bookNum = std::atoi(req.get_param_value("book").c_str());
        std::lock_guard<std::mutex> lk(m_RemoteMutex);
        const auto* book = findBook(m_CachedBible, bookNum);
        if (!book) { res.status = 404; return; }
        json arr = json::array();
        for (const auto& c : book->chapters) arr.push_back({ {"number", c.number}, {"verseCount", (int)c.verses.size()} });
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/bibles/verses?file=...&book=N&chapter=M ───────────────────
    svr.Get("/remote/bibles/verses", [this, &checkToken, &ensureBibleCached, &findBook](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file") || !req.has_param("book") || !req.has_param("chapter") ||
            !ensureBibleCached(req.get_param_value("file"))) {
            res.status = 400; return;
        }
        int bookNum = std::atoi(req.get_param_value("book").c_str());
        int chapNum = std::atoi(req.get_param_value("chapter").c_str());
        std::lock_guard<std::mutex> lk(m_RemoteMutex);
        const auto* book = findBook(m_CachedBible, bookNum);
        if (!book) { res.status = 404; return; }
        const ProyecThor::UI::ChapterData* chap = nullptr;
        for (const auto& c : book->chapters) if (c.number == chapNum) { chap = &c; break; }
        if (!chap) { res.status = 404; return; }
        json arr = json::array();
        for (const auto& v : chap->verses) arr.push_back({ {"number", v.number}, {"text", v.text} });
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── GET /remote/bibles/search?file=...&q=... ──────────────────────────────
    // Buscador combinado para el celular: si "q" parsea como una referencia
    // ("Juan 3:16", "salmo 23", "1 corintios 13:4" -- ver
    // Search::ParseSmartQuery, mismo motor que la busqueda rapida de la PC)
    // devuelve el salto directo en "reference" (book/chapter/verseIndex, listos
    // para pasarle tal cual a POST /remote/bibles/select). Ademas siempre hace
    // una busqueda de texto simple (sin acentos, insensible a mayusculas) sobre
    // el contenido de los versos, en "results" (tope kMaxResults). Todo se
    // resuelve ACA -- el celular nunca descarga la biblia entera para buscar.
    svr.Get("/remote/bibles/search", [this, &checkToken, &ensureBibleCached](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file") || !req.has_param("q") || !ensureBibleCached(req.get_param_value("file"))) {
            res.status = 400; return;
        }
        std::string q = req.get_param_value("q");
        std::lock_guard<std::mutex> lk(m_RemoteMutex);

        json j;
        j["reference"] = nullptr;

        int outBook = -1, outChap = -1, outVerse = -1;
        if (ProyecThor::UI::Search::ParseSmartQuery(m_CachedBible, q, outBook, outChap, outVerse)) {
            const auto& book = m_CachedBible.books[outBook];
            int chapNum = outChap >= 0 ? book.chapters[outChap].number : -1;
            j["reference"] = {
                {"book", book.canonicalNumber},
                {"bookName", book.name},
                {"chapter", chapNum},
                {"verseIndex", outVerse >= 0 ? outVerse : 0},
            };
        }

        json results = json::array();
        std::string needle = ProyecThor::UI::TextUtils::StripAccents(ProyecThor::UI::TextUtils::ToLowerUTF8(q));
        static constexpr size_t kMaxResults = 150;
        if (needle.size() >= 2) {
            for (const auto& book : m_CachedBible.books) {
                bool full = false;
                for (const auto& chap : book.chapters) {
                    for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
                        const auto& verse = chap.verses[vi];
                        std::string haystack =
                            ProyecThor::UI::TextUtils::StripAccents(ProyecThor::UI::TextUtils::ToLowerUTF8(verse.text));
                        if (haystack.find(needle) == std::string::npos) continue;
                        results.push_back({
                            {"book", book.canonicalNumber},
                            {"bookName", book.name},
                            {"chapter", chap.number},
                            {"verseIndex", vi},
                            {"verseNumber", verse.number},
                            {"text", verse.text},
                        });
                        if (results.size() >= kMaxResults) { full = true; break; }
                    }
                    if (full) break;
                }
                if (full) break;
            }
        }
        j["results"] = results;

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/bibles/select?file=...&book=N&chapter=M&index=I ─────────
    svr.Post("/remote/bibles/select",
        [this, &checkToken, &ensureBibleCached, &findBook](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("file") || !req.has_param("book") || !req.has_param("chapter") ||
            !ensureBibleCached(req.get_param_value("file"))) {
            res.status = 400; return;
        }
        int bookNum = std::atoi(req.get_param_value("book").c_str());
        int chapNum = std::atoi(req.get_param_value("chapter").c_str());
        int idx     = req.has_param("index") ? std::atoi(req.get_param_value("index").c_str()) : 0;

        std::lock_guard<std::mutex> lk(m_RemoteMutex);
        const auto* book = findBook(m_CachedBible, bookNum);
        if (!book) { res.status = 404; return; }
        const ProyecThor::UI::ChapterData* chap = nullptr;
        for (const auto& c : book->chapters) if (c.number == chapNum) { chap = &c; break; }
        if (!chap || chap->verses.empty()) { res.status = 404; return; }

        idx = std::clamp(idx, 0, (int)chap->verses.size() - 1);
        std::string ref  = BuildVerseRef(*book, *chap, chap->verses[idx]);
        std::string next = idx + 1 < (int)chap->verses.size()
            ? BuildProjectedVerseText(*book, *chap, chap->verses[idx + 1], m_CachedBible.name) : "";

        auto& core = Core::PresentationCore::Get();
        core.SetLayer2_Text(chap->verses[idx].text);
        core.SetCurrentRef(ref);
        core.SetNextText(next);
        core.SetProjecting(true);

        m_RemoteMode          = RemoteMode::Bible;
        m_CurrentBibleBook    = bookNum;
        m_CurrentBibleChapter = chapNum;
        m_CurrentVerseIndex   = idx;

        json j; j["ok"] = true; j["verseIndex"] = idx; j["verseCount"] = (int)chap->verses.size();
        res.set_content(j.dump(), "application/json");
    });

    // ── GET /remote/backgrounds ────────────────────────────────────────────────
    svr.Get("/remote/backgrounds", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        json arr = json::array();
        for (const auto& m : ListRemoteBackgrounds())
            arr.push_back({ {"folder", m.folder}, {"filename", m.filename}, {"isVideo", m.isVideo} });
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/backgrounds/select?folder=...&filename=... ──────────────
    svr.Post("/remote/backgrounds/select", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("folder") || !req.has_param("filename")) { res.status = 400; return; }

        std::string folder = req.get_param_value("folder");
        std::string filename = req.get_param_value("filename");
        if ((folder != "backgrounds" && folder != "images") ||
            filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        std::string fullPath = ProyecThor::GetAssetsPath() + "/" + folder + "/" + filename;
        std::string ext = fs::path(filename).extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        Core::PresentationCore::Get().SetBackgroundMedia(fullPath, IsVideoExt(ext), false);
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Post("/remote/backgrounds/clear", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        Core::PresentationCore::Get().StopBackgroundMedia();
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /remote/multimedia?type=all|video|audio|image ────────────────────
    // Biblioteca de Multimedia de la PC (Proyeccion > Video/Audio/Imagen, ver
    // LibraryMultimedia.cpp) -- distinto de Fondos (backgrounds/images): esta
    // seccion incluye ademas la carpeta de Audio.
    svr.Get("/remote/multimedia", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        std::string type = req.has_param("type") ? req.get_param_value("type") : "all";
        json arr = json::array();
        for (const auto& m : ListRemoteMultimedia()) {
            if (type != "all" && type != m.type) continue;
            arr.push_back({ {"filename", m.filename}, {"type", m.type} });
        }
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/multimedia/select?filename=...&type=video|image|audio ───
    // Video/Imagen: se mandan como fondo del proyector (mismo camino que
    // "Enviar al monitor" en el menu contextual de LibraryMultimedia.cpp,
    // CON audio permitido a diferencia de Fondos). Audio: se reproduce y se
    // manda en vivo via AudioPanel::PlayFileLive (mismo resultado que elegir
    // la pista y apretar "En vivo" a mano).
    svr.Post("/remote/multimedia/select", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("filename") || !req.has_param("type")) { res.status = 400; return; }

        std::string filename = req.get_param_value("filename");
        std::string type     = req.get_param_value("type");
        if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        if (type == "video" || type == "image") {
            std::string folder   = (type == "video") ? "videos" : "images";
            std::string fullPath = ProyecThor::GetAssetsPath() + "/" + folder + "/" + filename;
            Core::PresentationCore::Get().SetBackgroundMedia(fullPath, type == "video", /*allowAudio=*/true);
            Core::PresentationCore::Get().SetProjecting(true);
            res.set_content(R"({"ok":true})", "application/json");
            return;
        }

        if (type == "audio") {
            auto* audioPanel = Core::PresentationCore::Get().GetAudioPanelRef();
            if (!audioPanel || !audioPanel->PlayFileLive(filename)) {
                res.status = 404;
                res.set_content(R"({"error":"audio_not_found"})", "application/json");
                return;
            }
            res.set_content(R"({"ok":true})", "application/json");
            return;
        }

        res.status = 400;
        res.set_content(R"({"error":"invalid_type"})", "application/json");
    });

    svr.Post("/remote/multimedia/clear", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        Core::PresentationCore::Get().StopBackgroundMedia();
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /remote/overlays ──────────────────────────────────────────────────
    // Galeria de Overlays (ver OverlayLibraryTab::ReloadList). La miniatura
    // de cada uno se pide aparte con el endpoint generico ya existente
    // GET /sync/file?path=assets/overlays/<name>.png -- no hace falta nada
    // nuevo para eso.
    svr.Get("/remote/overlays", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        json arr = json::array();
        for (const auto& o : ListRemoteOverlays())
            arr.push_back({ {"name", o.name}, {"canvasW", o.canvasW}, {"canvasH", o.canvasH}, {"hasClock", o.hasClock} });
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(arr.dump(), "application/json; charset=utf-8");
    });

    // ── POST /remote/overlays/select?name=... ────────────────────────────────
    // Misma secuencia exacta que OverlayLibraryTab::RenderCard/RenderRow al
    // hacer click sobre un overlay de la galeria.
    svr.Post("/remote/overlays/select", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("name")) { res.status = 400; return; }

        std::string name = req.get_param_value("name");
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        fs::path dir = ProyecThor::OverlaysPath();
        fs::path pngPath = dir / (name + ".png");
        if (!fs::exists(pngPath)) {
            res.status = 404;
            res.set_content(R"({"error":"not_found"})", "application/json");
            return;
        }

        ProyecThor::UI::OverlayDoc doc;
        ProyecThor::UI::LoadOverlayRecipe(dir, name, doc);

        Core::PresentationCore::Get().SetOverlayMedia(pngPath.string());
        if (const auto* cl = ProyecThor::UI::FindClockLayer(doc))
            Core::PresentationCore::Get().SetOverlayClockLayer(true, *cl, doc.canvasW, doc.canvasH);
        else
            Core::PresentationCore::Get().SetOverlayClockLayer(false, ProyecThor::UI::OverlayLayer{}, doc.canvasW, doc.canvasH);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── POST /remote/overlays/clear ───────────────────────────────────────────
    svr.Post("/remote/overlays/clear", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        Core::PresentationCore::Get().ClearOverlay();
        Core::PresentationCore::Get().SetOverlayClockLayer(false, ProyecThor::UI::OverlayLayer{}, 1920, 1080);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── POST /remote/overlays/publish?name=... ───────────────────────────────
    // Se llama DESPUES de que el celular ya subio "<name>.overlay" y sus
    // imagenes nuevas via el endpoint generico ya existente PUT /sync/file
    // (a "assets/overlays/<name>.overlay" y "assets/overlays/images/<file>").
    // El celular no puede conocer de antemano la ruta absoluta real de la PC
    // (depende del usuario de Windows de esta maquina), asi que marca sus
    // imagenes nuevas con el sentinel "PHONE_ASSET:<filename>" en vez de una
    // ruta -- esta llamada las reescribe (reescritura ESTRUCTURAL via
    // OverlayRecipeIO, no substitucion de texto) a la ruta absoluta real
    // antes de que el overlay quede disponible para activarse. Una imagePath
    // que ya venia de un overlay existente en la PC (sin el sentinel) se deja
    // intacta.
    svr.Post("/remote/overlays/publish", [&checkToken](const httplib::Request& req, httplib::Response& res) {
        if (!checkToken(req, res)) return;
        if (!req.has_param("name")) { res.status = 400; return; }

        std::string name = req.get_param_value("name");
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_path"})", "application/json");
            return;
        }

        fs::path dir = ProyecThor::OverlaysPath();
        ProyecThor::UI::OverlayDoc doc;
        if (!ProyecThor::UI::LoadOverlayRecipe(dir, name, doc)) {
            res.status = 404;
            res.set_content(R"({"error":"not_found"})", "application/json");
            return;
        }

        static const std::string kSentinel = "PHONE_ASSET:";
        std::string imagesDir = (dir / "images").string();
        for (auto& layer : doc.layers) {
            if (layer.imagePath.rfind(kSentinel, 0) != 0) continue;
            layer.imagePath = imagesDir + "/" + layer.imagePath.substr(kSentinel.size());
        }

        ProyecThor::UI::SaveOverlayRecipe(dir, name, doc);
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content(R"({"error":"not_found"})", "application/json");
    });

    // ── Watcher: detiene svr cuando m_Running pasa a false ────────────────────
    std::thread watcher([this, &svr]() {
        while (m_Running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        svr.stop();
    });

    int boundPort = svr.bind_to_port("0.0.0.0", port);
    if (boundPort <= 0) {
        startedPromise.set_value(false);
        m_Running.store(false);
        if (watcher.joinable()) watcher.join();
        return;
    }

    startedPromise.set_value(true);
    svr.listen_after_bind();

    if (watcher.joinable()) watcher.join();
}

// ── DiscoveryThreadFunc ────────────────────────────────────────────────────────
// Escucha UDP en kDiscoveryPort: si llega el datagrama magico esperado,
// responde (unicast, al remitente) con un JSON {name, port} para que la app
// movil encuentre este PC sin que el usuario tenga que tipear la IP.
void SyncServer::DiscoveryThreadFunc() {
    static const char kMagic[] = "PROYECTHOR_SYNC_DISCOVER_V1";

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;
#endif

    SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(kDiscoveryPort));

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return;
    }

    char buffer[512];
    while (m_Running.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval tv; tv.tv_sec = 0; tv.tv_usec = 200 * 1000;
        int selResult = select(static_cast<int>(sock) + 1, &readSet, nullptr, nullptr, &tv);
        if (selResult <= 0) continue;

        sockaddr_in from = {};
        socklen_t   fromLen = sizeof(from);
        int received = recvfrom(sock, buffer, static_cast<int>(sizeof(buffer) - 1), 0,
                                 reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (received <= 0) continue;
        buffer[received] = '\0';

        if (std::strncmp(buffer, kMagic, sizeof(kMagic) - 1) != 0) continue;

        char hostname[256] = {};
        gethostname(hostname, sizeof(hostname));

        json reply;
        reply["app"]  = "ProyecThor";
        reply["name"] = std::string(hostname);
        reply["port"] = m_Port;
        std::string replyStr = reply.dump();

        sendto(sock, replyStr.data(), static_cast<int>(replyStr.size()), 0,
               reinterpret_cast<sockaddr*>(&from), fromLen);
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
}

// ── DetectLocalIP ─────────────────────────────────────────────────────────────
std::string SyncServer::DetectLocalIP() {
#ifdef _WIN32
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    addrinfo hints = {}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) == 0 && result) {
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, ip, sizeof(ip));
        freeaddrinfo(result);
        std::string out(ip);
        if (!out.empty() && out != "127.0.0.1") return out;
    }
#else
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
            std::string out(ip);
            if (out != "127.0.0.1" && out.substr(0,3) != "169") { freeifaddrs(ifaddr); return out; }
        }
        freeifaddrs(ifaddr);
    }
#endif
    return "127.0.0.1";
}

} // namespace ProyecThor::Core
