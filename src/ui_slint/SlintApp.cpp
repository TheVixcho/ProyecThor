#include "SlintApp.h"
#include "AppPaths.h"
#include "BibleXmlIO.h"
#include "OSCSender.h"
#include "Version.h"
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"
#include "ui/framework/PatchNotesData.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <psapi.h>
#pragma comment(lib, "comdlg32.lib")
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::SlintUI {

namespace {

// Total de RAM física del sistema en GB (redondeado hacia abajo). 0 si no se
// puede determinar. Se usa para auto-detectar hardware modesto: un Pentium
// Gold reporta 4 hilos (hardware_concurrency no lo marca como gama baja), pero
// suele venir con 8 GB o menos, así que la RAM es el mejor indicador.
unsigned long long SystemRamGb() {
#ifdef _WIN32
    MEMORYSTATUSEX ms = {};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return ms.ullTotalPhys / (1024ULL * 1024 * 1024);
    return 0;
#elif defined(__APPLE__)
    int64_t mem = 0;
    size_t len = sizeof(mem);
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    if (sysctl(mib, 2, &mem, &len, nullptr, 0) == 0 && mem > 0)
        return static_cast<unsigned long long>(mem) / (1024ULL * 1024 * 1024);
    return 0;
#else
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    unsigned long long kb = 0;
    while (meminfo >> key) {
        if (key == "MemTotal:") { meminfo >> kb; break; }
        meminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return kb / (1024ULL * 1024);
#endif
}

// Heurística de gama baja: 2 o menos núcleos lógicos, o 8 GB de RAM o menos.
// Cubre el caso Pentium Gold (4 hilos pero 8 GB) que el chequeo de solo
// núcleos dejaba pasar.
bool IsLowEndHardware() {
    if (std::thread::hardware_concurrency() <= 2)
        return true;
    const unsigned long long ramGb = SystemRamGb();
    return ramGb != 0 && ramGb <= 8;
}

std::string PathToUtf8(const fs::path& p) {
#ifdef _WIN32
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), len, nullptr, nullptr);
    return out;
#else
    return PathToUtf8(p);
#endif
}

fs::path Utf8ToFsPath(const std::string& u8) {
    if (u8.empty()) return {};
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), nullptr, 0);
    if (len <= 0) return fs::path(u8);
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), w.data(), len);
    return fs::path(w);
#else
    return fs::path(u8);
#endif
}

bool HasExtension(const fs::path& p, const std::vector<std::string>& exts) {
    std::string ext = PathToUtf8(p.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

std::string RunCommandCapture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    // Usar CreateProcess con CREATE_NO_WINDOW en vez de _popen para que NO
    // aparezca una ventana de consola/PowerShell parpadeando cada vez que se
    // consulta la GPU, se sondea ffmpeg o se abre el diálogo de importar.
    // _popen siempre arrastra una consola visible en una app GUI sin consola.
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return out;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput  = writePipe;
    si.hStdError   = writePipe;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    // cmd.exe /c para resolver programas en PATH y comandos con tuberías/redirecciones.
    std::string full = "cmd.exe /c " + cmd;
    std::string mutableCmd = full;

    BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        return out;
    }

    char buf[512];
    DWORD read = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0)
        out.append(buf, read);

    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        out += buf;
    }
    pclose(pipe);
#endif
    return out;
}

std::string FindFirstExisting(const std::vector<std::string>& candidates) {
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) return c;
    }
    return "";
}

std::string SanitizeFilename(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            out += '_';
        } else {
            out += c;
        }
    }
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ')  out.pop_back();
    return out.empty() ? "Nueva Cancion" : out;
}

} // namespace

SlintApp::SlintApp()
    : m_Splash(SplashWindow::create())
    , m_Main(MainWindow::create())
    , m_Projector(ProjectorWindow::create())
    , m_Stage(StageWindow::create())
    , m_LibraryModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_SlidesModel(std::make_shared<slint::VectorModel<SlideItem>>())
    , m_SlideRowsModel(std::make_shared<slint::VectorModel<SlideGridRow>>())
    , m_MusicTracksModel(std::make_shared<slint::VectorModel<MusicTrackItem>>())
    , m_MusicGridRowsModel(std::make_shared<slint::VectorModel<MusicGridRow>>())
    , m_MusicScanFoldersModel(std::make_shared<slint::VectorModel<slint::SharedString>>())
    , m_HubSongsModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_HubMediaModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_HubMusicModel(std::make_shared<slint::VectorModel<MusicTrackItem>>())
    , m_HubDocsModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_DocPagesModel(std::make_shared<slint::VectorModel<DocPageItem>>())
    , m_QueueModel(std::make_shared<slint::VectorModel<QueueItem>>())
    , m_PadsModel(std::make_shared<slint::VectorModel<PadItem>>())
    , m_CaptureModel(std::make_shared<slint::VectorModel<CaptureDeviceItem>>())
    , m_SmartTvModel(std::make_shared<slint::VectorModel<SmartTvItem>>())
    , m_SavedStyleCardsModel(std::make_shared<slint::VectorModel<StyleCardItem>>())
    , m_BgDockModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_BgFoldersModel(std::make_shared<slint::VectorModel<BgFolderItem>>())
    , m_OverlayLayersModel(std::make_shared<slint::VectorModel<LibraryItem>>())
    , m_VideoQueueModel(std::make_shared<slint::VectorModel<VideoQueueItem>>())
    , m_BibleBooksModel(std::make_shared<slint::VectorModel<BibleBookItem>>())
    , m_BibleChaptersModel(std::make_shared<slint::VectorModel<int>>())
    , m_BibleHistoryModel(std::make_shared<slint::VectorModel<SlideItem>>())
    , m_BibleFavoritesModel(std::make_shared<slint::VectorModel<SlideItem>>())
    , m_BibleVersionNamesModel(std::make_shared<slint::VectorModel<slint::SharedString>>())
    , m_SongPlaylistNamesModel(std::make_shared<slint::VectorModel<slint::SharedString>>())
    , m_SystemFontsModel(std::make_shared<slint::VectorModel<slint::SharedString>>())
    , m_OverlayQueueModel(std::make_shared<slint::VectorModel<QueueItem>>())
    , m_ActiveLayersModel(std::make_shared<slint::VectorModel<ActiveLayerItem>>())
    , m_ScreenMatrixModel(std::make_shared<slint::VectorModel<ScreenMatrixItem>>())
    , m_MusicLiveWaveModel(std::make_shared<slint::VectorModel<float>>())
    , m_MusicTrackWaveModel(std::make_shared<slint::VectorModel<float>>())
    , m_AppAliveFlag(std::make_shared<std::atomic<bool>>(true))
    , m_MusicPlayer(true)
    , m_Player(true)
    , m_HoverPlayer(true)
{
    for (int i = 0; i < 8; ++i) {
        m_Pads[i].name = "Pad " + std::to_string(i + 1);
    }
    for (int i = 0; i < 12; ++i) {
        m_LiveWaveSmoothed[i] = 0.08f;
        m_MusicLiveWaveModel->push_back(0.08f);
    }
    m_MusicTrackEnvelope.assign(56, 0.14f);
    for (int i = 0; i < 56; ++i) {
        m_MusicTrackWaveModel->push_back(0.14f);
    }

    // Configure lightweight hover preview player (320x180, silent) & independent Music player
    m_HoverPlayer.SetRenderSize(320, 180);
    m_MusicPlayer.SetAudioOnlyMode(true);
    m_MusicPlayer.SetVolume(85);

    m_Splash->set_app_version(slint::SharedString(PROYECTHOR_VERSION_STRING));
    m_Main->set_app_version(slint::SharedString(PROYECTHOR_VERSION_STRING));
    m_Main->set_library_items(m_LibraryModel);
    m_Main->set_slides(m_SlidesModel);
    m_Main->set_slide_rows(m_SlideRowsModel);
    m_Main->set_music_tracks(m_MusicTracksModel);
    m_Main->set_music_grid_rows(m_MusicGridRowsModel);
    m_Main->set_music_scan_folders(m_MusicScanFoldersModel);
    m_Main->set_music_live_wave(m_MusicLiveWaveModel);
    m_Main->set_music_track_waveform(m_MusicTrackWaveModel);
    m_Main->set_hub_recent_songs(m_HubSongsModel);
    m_Main->set_hub_recent_media(m_HubMediaModel);
    m_Main->set_hub_recent_music(m_HubMusicModel);
    m_Main->set_hub_recent_docs(m_HubDocsModel);
    m_Main->set_doc_pages(m_DocPagesModel);
    m_Main->set_queue_items(m_QueueModel);
    m_Main->set_overlay_queue_items(m_OverlayQueueModel);
    m_Main->set_active_layers(m_ActiveLayersModel);
    m_Main->set_screen_matrix_items(m_ScreenMatrixModel);
    m_Main->set_song_playlist_names(m_SongPlaylistNamesModel);
    m_Main->set_system_font_names(m_SystemFontsModel);
    m_Main->set_pads(m_PadsModel);
    m_Main->set_capture_devices(m_CaptureModel);
    m_Main->set_smart_tv_devices(m_SmartTvModel);
    m_Main->set_saved_style_cards(m_SavedStyleCardsModel);

    // Pre-fill local LAN IP address for IP-based streaming
    auto localIps = ProyecThor::Core::SmartTvCaster::GetLocalIPv4Addresses();
    if (!localIps.empty()) {
        m_Main->set_lan_host_ip(slint::SharedString(localIps[0]));
    }
    m_Main->set_bg_dock_items(m_BgDockModel);
    m_Main->set_bg_folders(m_BgFoldersModel);
    m_Main->set_overlay_layer_items(m_OverlayLayersModel);
    m_Main->set_video_queue_items(m_VideoQueueModel);
    m_Main->set_bible_books(m_BibleBooksModel);
    m_Main->set_bible_chapter_numbers(m_BibleChaptersModel);
    m_Main->set_bible_history(m_BibleHistoryModel);
    m_Main->set_bible_favorites(m_BibleFavoritesModel);
    m_Main->set_bible_version_names(m_BibleVersionNamesModel);

    InitAssetsAndSplash();

    m_Main->window().on_close_requested([this]() {
        m_Projector->hide();
        m_Stage->hide();
        slint::quit_event_loop();
        return slint::CloseRequestResponse::HideWindow;
    });

    m_Projector->window().on_close_requested([this]() {
        m_ProjectorVisible = false;
        m_Main->set_audience_live(false);
        UpdatePlayerRenderSize();
        return slint::CloseRequestResponse::HideWindow;
    });
    m_Stage->window().on_close_requested([this]() {
        m_StageVisible = false;
        m_Main->set_stage_live(false);
        return slint::CloseRequestResponse::HideWindow;
    });

    InitMultiScreens();
    RefreshMonitorList();
    BindCallbacks();
    ScanSystemFonts();
    LoadPersistedState();
    // Revalida la pantalla de proyección cargada contra los monitores reales
    // (si el índice guardado no es válido, vuelve a la secundaria por defecto).
    if (m_ProjectionMonitorIdx < 0 ||
        m_ProjectionMonitorIdx >= static_cast<int>(m_Monitors.size())) {
        m_ProjectionMonitorIdx = DefaultProjectionMonitorIndex(m_Monitors);
    }
    m_Main->set_projection_monitor_idx(m_ProjectionMonitorIdx);
    ApplyGpuAndUpscalerConfig();
    UpdatePlayerRenderSize();
    m_Player.SetScaleMode(m_BgScaleMode);

    // Event-driven video frame delivery directly from SlintVideoPlayer's async render worker
    // (eliminates 33ms timer bottleneck that capped 60/120 FPS video at ~24 FPS!)
    auto aliveFlag = m_AppAliveFlag;
    m_Player.SetFrameReadyCallback([this, aliveFlag]() {
        slint::invoke_from_event_loop([this, aliveFlag]() {
            if (!aliveFlag || !aliveFlag->load(std::memory_order_acquire)) return;
            DeliverVideoFrameToOutputs();
        });
    });

    DetectGpuCapabilitiesAsync();
    LoadThemeFilesFromDisk();
    LoadSongPlaylists();
    ApplyThemePreset(m_ActiveThemeIdx);
    RefreshPadsModel();
    RefreshSavedStylesModel();
    RefreshMusicScanFoldersModel();
    RefreshBgDock();
    LoadCustomOverlaysFromDisk();
    RefreshOverlayLayers();
    LoadChangelogText();
    CheckFirewallStatusAsync();
    SyncProjectorStyle();
    SyncTilesState();
    ApplyShaders();
    SyncClockOverlay();
    SyncScreenOutputs();
    AppendDebugLog("ProyecThor Slint v" + std::string(PROYECTHOR_VERSION_STRING) + " inicializado.");

    // Configure Mobile Server defaults
    m_MobileServer.SetRootDir(ProyecThor::GetAppDataRoot());
    if (m_MobileServer.GetPairingPin().empty()) {
        std::string pin = GenerateRandomPin6();
        m_MobileServer.SetPairingPin(pin);
        m_Main->set_mobile_pin(slint::SharedString(pin));
    }

    auto weakMain = slint::ComponentWeakHandle<MainWindow>(m_Main);
    m_MobileServer.SetCallbacks({
        [this]() {
            slint::invoke_from_event_loop([this]() {
                int count = static_cast<int>(m_SlidesModel->row_count());
                if (count <= 0) return;
                int cur = m_Main->get_active_slide_index();
                int next = (cur < 0) ? 0 : std::min(cur + 1, count - 1);
                m_Main->set_active_slide_index(next);
                ProjectSlide(next);
            });
        },
        [this]() {
            slint::invoke_from_event_loop([this]() {
                int count = static_cast<int>(m_SlidesModel->row_count());
                if (count <= 0) return;
                int cur = m_Main->get_active_slide_index();
                int prev = std::max(cur - 1, 0);
                m_Main->set_active_slide_index(prev);
                ProjectSlide(prev);
            });
        },
        [this]() {
            slint::invoke_from_event_loop([this]() { ClearText(); });
        },
        [this]() {
            slint::invoke_from_event_loop([this]() { ClearAll(); });
        },
        [this]() {
            slint::invoke_from_event_loop([this]() { ToggleAudienceWindow(); });
        },
        [this]() -> std::string {
            std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
            nlohmann::json j = {
                {"text", m_SnapText},
                {"ref", m_SnapRef},
                {"projecting", m_SnapProjecting}
            };
            return j.dump();
        }
    });

    // Configure default ImGui StreamingPanel LAN config (1280x720, Normal MJPEG 30fps, 80% JPEG)
    m_LanConfig.sendBackground  = true;
    m_LanConfig.transparentMode = false;
    m_LanConfig.sendText        = true;
    m_LanConfig.sendOverlay     = true;
    m_LanConfig.videoMode       = ProyecThor::Core::StreamConfig::VideoMode::HighQuality;
    m_LanConfig.frameWidth      = 1280;
    m_LanConfig.frameHeight     = 720;
    m_LanConfig.jpegQuality     = 80;
    m_LanConfig.targetFPS       = 30;

    // Configure Independent Lightweight Chat LAN Server (Puerto 8082 — SIN Video)
    m_ChatServer.SetChatStore(&m_ChatStore);
    (void)weakMain;

    // Start tick timer at configured target FPS (30 / 60 / 120 FPS)
    if (!m_TickTimer) {
        int tickMs = (m_TargetFps >= 120) ? 8 : (m_TargetFps >= 60 ? 16 : 33);
        m_TickTimer = std::make_unique<slint::Timer>(
            std::chrono::milliseconds(tickMs),
            [this]() { OnTick(); }
        );
    }
}

SlintApp::~SlintApp() {
    if (m_AppAliveFlag) {
        m_AppAliveFlag->store(false, std::memory_order_release);
    }
    m_Player.SetFrameReadyCallback(nullptr);
    SavePersistedState();
    if (m_LanServer) {
        m_LanServer->Stop();
        m_LanServer.reset();
    }
    m_ChatServer.Stop();
    m_MobileServer.Stop();
    m_SplashTimer.reset();
    m_TickTimer.reset();
    m_HoverPlayer.Stop();
    m_MusicPlayer.Stop();
    m_Player.Stop();
}

void SlintApp::InitAssetsAndSplash() {
    std::string logoPath = FindFirstExisting({
        "proyecthor.png",
        "assets/proyecthor.png",
        "../assets/proyecthor.png",
        "packaging/proyecthor.png"
    });
    if (!logoPath.empty()) {
        auto logo = slint::Image::load_from_path(slint::SharedString(logoPath));
        m_Splash->set_logo_image(logo);
        m_Splash->set_has_logo(true);
        m_Main->set_logo_image(logo);
        m_Main->set_has_logo(true);
    }

    // ImGui SplashScreen parity: rotate across the 4 panoramic artworks persisted in splash_state.txt
    struct SplashArt { const char* file; const char* credit; };
    static const SplashArt kSplashArt[] = {
        {"splash_bg1.jpg", "Foto por Fabiola Fernandez"},
        {"splash_bg5.jpg", "Arte por TheVixcho"},
        {"splash_bg2.jpg", "Arte por TheVixcho"},
        {"splash_bg7.jpg", "Arte por TheVixcho"}
    };
    int artIdx = 0;
    fs::path stateFile = fs::path(ProyecThor::GetAppDataRoot()) / "splash_state.txt";
    {
        std::ifstream in(stateFile);
        if (in.is_open()) {
            int last = 0;
            if (in >> last) artIdx = (last + 1) % 4;
        }
        std::error_code ec;
        fs::create_directories(stateFile.parent_path(), ec);
        std::ofstream out(stateFile, std::ios::trunc);
        if (out.is_open()) out << artIdx;
    }

    std::string chosenBg;
    std::string chosenCredit = kSplashArt[artIdx].credit;
    for (int offset = 0; offset < 4; ++offset) {
        const auto& art = kSplashArt[(artIdx + offset) % 4];
        std::string f = art.file;
        chosenBg = FindFirstExisting({
            f,
            "assets/" + f,
            "../assets/" + f,
            PathToUtf8(fs::path(ProyecThor::GetAssetsPath()) / f),
            "bg_splash3.jpg",
            "assets/bg_splash3.jpg"
        });
        if (!chosenBg.empty()) {
            chosenCredit = art.credit;
            break;
        }
    }

    m_Splash->set_credit_text(slint::SharedString(chosenCredit));
    if (!chosenBg.empty()) {
        auto bg = slint::Image::load_from_path(slint::SharedString(chosenBg));
        m_Splash->set_bg_image(bg);
        m_Splash->set_has_bg(true);
    }
}

int SlintApp::Run() {
    // ImGui-style progressive splash screen with status + resource detail
    m_SplashStep = 0;
    m_Splash->set_progress(0.18f);
    m_Splash->set_status_text("Inicializando motor libmpv (GPU)...");
    m_Splash->set_detail_text("Cargando decodificador de video y renderizador Slint");
    m_Splash->show();

    m_SplashTimer = std::make_unique<slint::Timer>(
        std::chrono::milliseconds(260),
        [this]() {
            ++m_SplashStep;
            if (m_SplashStep == 1) {
                m_Splash->set_progress(0.48f);
                m_Splash->set_status_text("Cargando biblioteca de canciones, música, multimedia y Biblias...");
                m_Splash->set_detail_text("Escaneando carpetas locales de ProyecThor");
                RefreshLibraryHub();
            } else if (m_SplashStep == 2) {
                m_Splash->set_progress(0.82f);
                m_Splash->set_status_text("Preparando estilos, transiciones y fondos...");
                m_Splash->set_detail_text("Sincronizando motor de proyección y vista previa");
            } else if (m_SplashStep == 3) {
                m_Splash->set_progress(1.0f);
                m_Splash->set_status_text("Listo para proyectar");
                m_Splash->set_detail_text("ProyecThor Open Source • Slint + libmpv");
            } else {
                FinishSplash();
            }
        }
    );

    slint::run_event_loop();
    return 0;
}

void SlintApp::FinishSplash() {
    m_SplashTimer.reset();
    m_Main->show();
    m_Splash->hide();
}

void SlintApp::BindCallbacks() {
    m_Main->on_tab_changed([this](int tab) {
        StopHoverVideoPreview();
        m_Main->set_bible_version_menu_open(false);
        m_Main->set_scale_menu_open(false);
        m_Main->set_library_menu_open(false);
        m_Main->set_design_menu_open(false);
        m_Main->set_net_menu_open(false);
        m_ActiveTab = tab;
        m_SearchQuery.clear();
        m_Main->set_search_query("");
        m_Main->set_song_editor_open(false);
        m_OverlayEditorOpen = false;
        m_Main->set_overlay_editor_open(false);
        if (tab == 13) {
            RefreshLibraryHub();
        } else if (tab == 0 || tab == 1 || tab == 2 || tab == 10) {
            LoadTabData(tab);
        } else if (tab == 5) {
            CheckFirewallStatusAsync();
        } else if (tab == 8) {
            RefreshCaptureDevices();
        } else if (tab == 9) {
            if (!m_MusicScanned) ScanMusicTracks();
        } else if (tab == 12 || tab == 7) {
            RefreshDebugInfo();
        } else if (tab == 14) {
            RefreshScreenMatrixModel();
        }
    });

    m_Main->on_bg_subtab_changed([this](int sub) {
        StopHoverVideoPreview();
        m_BgSubtab = sub;
        if (m_ActiveTab == 3) {
            LoadTabData(3);
        }
    });

    m_Main->on_search_changed([this](slint::SharedString q) {
        m_SearchQuery = std::string(q);
        RefreshLibraryModel();
    });

    m_Main->on_library_item_selected([this](int idx) {
        SelectLibraryItem(idx);
    });

    m_Main->on_slide_clicked([this](int idx) {
        ProjectSlide(idx);
    });
    m_Main->on_slide_columns_changed([this](int) {
        RebuildSlideGridRows();
        SavePersistedState();
    });

    m_Main->on_next_slide([this]() {
        int count = static_cast<int>(m_SlidesModel->row_count());
        if (count <= 0) return;
        int cur = m_Main->get_active_slide_index();
        int next = (cur < 0) ? 0 : std::min(cur + 1, count - 1);
        m_Main->set_active_slide_index(next);
        ProjectSlide(next);
    });

    m_Main->on_prev_slide([this]() {
        int count = static_cast<int>(m_SlidesModel->row_count());
        if (count <= 0) return;
        int cur = m_Main->get_active_slide_index();
        int prev = std::max(cur - 1, 0);
        m_Main->set_active_slide_index(prev);
        ProjectSlide(prev);
    });

    // Song Editor (Título + Autor + Estrofas)
    m_Main->on_new_song_clicked([this]() { OpenNewSongEditor(); });
    m_Main->on_edit_song_clicked([this]() { OpenEditSongEditor(); });
    m_Main->on_save_song_clicked([this](slint::SharedString t, slint::SharedString a, slint::SharedString b) {
        SaveEditedSong(std::string(t), std::string(a), std::string(b));
    });
    m_Main->on_cancel_song_editor([this]() {
        m_Main->set_song_editor_open(false);
    });

    // Service Queue (Letras, Videos, Overlays) & Song Playlists
    m_Main->on_add_current_to_queue([this]() { AddCurrentToQueue(); });
    m_Main->on_queue_item_clicked([this](int idx) { ActivateQueueItem(idx); });
    m_Main->on_clear_queue([this]() {
        m_Queue.clear();
        RefreshQueueModel();
    });
    m_Main->on_add_overlay_to_queue([this]() { AddSelectedOverlayToQueue(); });
    m_Main->on_overlay_queue_clicked([this](int idx) { ToggleOverlayQueueItem(idx); });
    m_Main->on_clear_overlay_queue([this]() {
        m_OverlayQueue.clear();
        RefreshOverlayQueueModel();
    });
    m_Main->on_create_song_playlist([this](slint::SharedString name) {
        CreateSongPlaylist(std::string(name));
    });
    m_Main->on_select_song_playlist([this](int idx) { SelectSongPlaylist(idx); });
    m_Main->on_toggle_playlist_filter([this]() { TogglePlaylistFilter(); });
    m_Main->on_add_song_to_playlist([this]() { AddSelectedSongToPlaylist(); });
    m_Main->on_send_playlist_to_queue([this]() { SendActivePlaylistToQueue(); });
    m_Main->on_delete_song_playlist([this](int idx) { DeleteSongPlaylist(idx); });

    // Bible Fullscreen
    m_Main->on_cycle_bible_version([this]() {
        if (m_BiblePaths.empty()) return;
        m_CurrentBibleVersionIdx = (m_CurrentBibleVersionIdx + 1) % static_cast<int>(m_BiblePaths.size());
        LoadSelectedBible(m_CurrentBibleVersionIdx);
    });
    m_Main->on_bible_version_selected([this](int idx) {
        LoadSelectedBible(idx);
    });
    m_Main->on_import_bible_clicked([this]() { ImportFileAsync(false); });
    m_Main->on_bible_search_submitted([this](slint::SharedString q) { SearchBible(std::string(q)); });
    m_Main->on_bible_search_edited([this](slint::SharedString q) { OnBibleSearchEdited(std::string(q)); });
    m_Main->on_bible_filter_changed([this](int) {
        RefreshBibleBooks(std::string(m_Main->get_bible_search_query()));
    });
    m_Main->on_bible_book_selected([this](int idx) { SelectBibleBook(idx); });
    m_Main->on_bible_chapter_changed([this](int chap) {
        if (m_CurrentBibleBookIdx >= 0) {
            LoadBibleChapter(m_CurrentBibleBookIdx, chap);
        }
    });
    m_Main->on_bible_favorite_current([this]() { AddCurrentVerseToFavorites(); });
    m_Main->on_bible_history_clicked([this](int idx) {
        if (auto item = m_BibleHistoryModel->row_data(idx)) {
            m_RawLiveText = std::string(item->body);
            m_RawLiveRef  = std::string(item->ref_text);
            m_LiveIsBible = true;
            for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
                if (!IsScreenTargeted(i)) continue;
                SetScreenLiveText(m_Screens[i], m_RawLiveText, m_RawLiveRef, true);
            }
            SyncScreenOutputs();
            TriggerSlideTransition();
        }
    });
    m_Main->on_bible_favorite_clicked([this](int idx) {
        if (auto item = m_BibleFavoritesModel->row_data(idx)) {
            m_RawLiveText = std::string(item->body);
            m_RawLiveRef  = std::string(item->ref_text);
            m_LiveIsBible = true;
            for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
                if (!IsScreenTargeted(i)) continue;
                SetScreenLiveText(m_Screens[i], m_RawLiveText, m_RawLiveRef, true);
            }
            SyncScreenOutputs();
            TriggerSlideTransition();
        }
    });

    // Media Preview, Hover Video Preview & Video Queue
    m_Main->on_library_item_hovered([this](int filteredIdx) {
        if (filteredIdx < 0 || filteredIdx >= static_cast<int>(m_FilteredIndices.size())) {
            StopHoverVideoPreview();
            return;
        }
        const auto& entry = m_AllEntries[m_FilteredIndices[filteredIdx]];
        if ((m_ActiveTab == 2 || (m_ActiveTab == 3 && m_BgSubtab == 0)) && entry.isVideo) {
            StartHoverVideoPreview(entry.path);
        } else {
            StopHoverVideoPreview();
        }
    });
    m_Main->on_dock_bg_hovered([this](int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_BgDockEntries.size())) {
            StopHoverVideoPreview();
            return;
        }
        const auto& entry = m_BgDockEntries[idx];
        if (entry.isVideo) {
            StartHoverVideoPreview(entry.path);
        } else {
            StopHoverVideoPreview();
        }
    });
    m_Main->on_project_selected_media([this]() { ProjectSelectedMedia(); });
    m_Main->on_add_selected_to_video_queue([this]() { AddSelectedToVideoQueue(); });
    m_Main->on_toggle_video_queue_play([this]() { ToggleVideoQueuePlay(); });
    m_Main->on_prev_video_queue([this]() { PrevVideoQueueItem(); });
    m_Main->on_next_video_queue([this]() { NextVideoQueueItem(); });
    m_Main->on_play_video_queue_item([this](int idx) { PlayVideoQueueIndex(idx); });
    m_Main->on_move_video_queue_item([this](int from, int to) { MoveVideoQueueItem(from, to); });
    m_Main->on_remove_video_queue_item([this](int idx) { RemoveVideoQueueItem(idx); });
    m_Main->on_clear_video_queue([this]() { ClearVideoQueue(); });

    // Live Output, Freeze, Scaling/FSR & Manual Scan
    m_Main->on_toggle_audience([this]() { ToggleAudienceWindow(); });
    m_Main->on_close_audience_if_open([this]() {
        if (m_ProjectorVisible) ToggleAudienceWindow();
    });
    m_Projector->on_close_requested([this]() {
        if (m_ProjectorVisible) ToggleAudienceWindow();
    });
    m_Projector->on_next_slide_requested([this]() {
        if (m_ActiveTab == 10 && !m_DocPagePaths.empty()) {
            int next = std::min(static_cast<int>(m_DocPagePaths.size()) - 1, m_ActiveDocPageIdx + 1);
            m_Main->set_active_doc_page_idx(next);
            ProjectDocPage(next);
            return;
        }
        int count = static_cast<int>(m_SlidesModel->row_count());
        if (count <= 0) return;
        int cur = m_Main->get_active_slide_index();
        int next = (cur < 0) ? 0 : std::min(cur + 1, count - 1);
        m_Main->set_active_slide_index(next);
        ProjectSlide(next);
    });
    m_Projector->on_prev_slide_requested([this]() {
        if (m_ActiveTab == 10 && !m_DocPagePaths.empty()) {
            int prev = std::max(0, m_ActiveDocPageIdx - 1);
            m_Main->set_active_doc_page_idx(prev);
            ProjectDocPage(prev);
            return;
        }
        int count = static_cast<int>(m_SlidesModel->row_count());
        if (count <= 0) return;
        int cur = m_Main->get_active_slide_index();
        int prev = std::max(cur - 1, 0);
        m_Main->set_active_slide_index(prev);
        ProjectSlide(prev);
    });
    m_Main->on_toggle_stage([this]() { ToggleStageWindow(); });
    m_Main->on_toggle_freeze([this]() { ToggleFreeze(); });
    m_Main->on_scale_mode_changed([this](int mode) { ApplyScalingMode(mode); });
    m_Main->on_output_res_changed([this](int preset) { ApplyOutputResolution(preset); });
    m_Main->on_target_fps_changed([this](int fps) { ApplyTargetFps(fps); });
    m_Main->on_scan_libraries([this]() { ScanAllLibrariesManual(); });
    m_Main->on_bg_folder_entered([this](slint::SharedString rel) {
        StopHoverVideoPreview();
        m_BgCurrentRelDir = std::string(rel);
        RefreshBgDock();
    });
    m_Main->on_bg_folder_up([this]() {
        StopHoverVideoPreview();
        if (!m_BgCurrentRelDir.empty()) {
            fs::path p = fs::path(m_BgCurrentRelDir).parent_path();
            m_BgCurrentRelDir = (p == "." || p.empty()) ? "" : p.generic_string();
        }
        RefreshBgDock();
    });
    m_Main->on_bg_folder_created([this](slint::SharedString name) {
        CreateBgSubfolder(std::string(name));
    });
    m_Main->on_clear_text([this]() { ClearText(); });
    m_Main->on_clear_background([this]() { ClearBackground(); });
    m_Main->on_clear_overlay([this]() { ClearOverlay(); });
    m_Main->on_clear_all([this]() { ClearAll(); });

    // Music Player (Tab 9 - independent audio)
    m_Main->on_music_track_clicked([this](int idx) { PlayMusicTrack(idx); });
    m_Main->on_music_toggle_pause([this]() {
        if (!m_MusicPlayer.IsPlaying() && !m_MusicEntries.empty()) {
            PlayMusicTrack(m_CurrentMusicIdx >= 0 ? m_CurrentMusicIdx : 0);
            return;
        }
        bool next = !m_MusicPlayer.IsPaused();
        m_MusicPlayer.SetPause(next);
        m_Main->set_music_paused(next);
    });
    m_Main->on_music_stop([this]() {
        m_MusicPlayer.Stop();
        m_CurrentMusicIdx = -1;
        m_Main->set_music_playing(false);
        m_Main->set_music_paused(false);
        m_Main->set_music_position(0.0f);
        m_Main->set_music_time_label("00:00 / 00:00");
        m_Main->set_music_now_playing("Sin pista en reproducción");
        m_Main->set_music_now_subtitle("Selecciona una pista para reproducir");
        m_Main->set_has_music_now_cover(false);
        RefreshMusicModel();
        RefreshActiveLayersModel();
    });
    m_Main->on_music_prev([this]() {
        if (m_MusicEntries.empty()) return;
        int n = static_cast<int>(m_MusicEntries.size());
        int target = (m_CurrentMusicIdx <= 0) ? (n - 1) : (m_CurrentMusicIdx - 1);
        PlayMusicTrack(target);
    });
    m_Main->on_music_next([this]() {
        if (m_MusicEntries.empty()) return;
        int n = static_cast<int>(m_MusicEntries.size());
        int target = (m_CurrentMusicIdx + 1) % n;
        PlayMusicTrack(target);
    });
    m_Main->on_music_toggle_loop([this]() {
        bool next = !m_Main->get_music_loop();
        m_Main->set_music_loop(next);
        if (m_CurrentMusicIdx >= 0 && m_CurrentMusicIdx < static_cast<int>(m_MusicEntries.size()) && m_MusicPlayer.IsPlaying()) {
            m_MusicPlayer.Play(m_MusicEntries[m_CurrentMusicIdx].path, next, false);
        }
    });
    m_Main->on_music_volume_changed([this](float v) { m_MusicPlayer.SetVolume(static_cast<int>(v)); });
    m_Main->on_music_seek_changed([this](float p) {
        m_MusicPlayer.SetPosition(p);
        m_LastMusicPos = p;
        m_LastMusicSec = -1;
    });
    m_Main->on_import_music_clicked([this]() { ImportFileAsync(false, false, 9); });
    m_Main->on_browse_music_folder([this]() { BrowseMusicFolderAsync(); });
    m_Main->on_add_music_folder([this](slint::SharedString p) { AddMusicScanFolder(std::string(p)); });
    m_Main->on_remove_music_folder([this](int idx) { RemoveMusicScanFolder(idx); });
    m_Main->on_music_eq_changed([this]() {
        ApplyMusicEqualizer(true);
        SavePersistedState();
    });
    m_Main->on_music_eq_preset_selected([this](int idx) {
        ApplyMusicEqPreset(idx);
        SavePersistedState();
    });
    m_Main->on_music_dynamic_eq_toggled([this](bool enabled) {
        if (!enabled) {
            for (int b = 0; b < 5; ++b) { m_DynEqOffsets[b] = 0.0f; m_DynEqApplied[b] = 0.0f; }
            m_Main->set_music_dynamic_eq_status("Inactiva — activa para nivelación y balance espectral adaptativo");
        } else {
            m_Main->set_music_dynamic_eq_status("⚡ EQ Dinámica Activa • Analizando señal y balance espectral en vivo...");
        }
        ApplyMusicEqualizer(true);
        SavePersistedState();
    });

    // Library Hub (Tab 13) quick jump actions
    m_Main->on_hub_open_song([this](slint::SharedString path) {
        std::string target(path);
        m_ActiveTab = 0;
        m_Main->set_active_tab(0);
        LoadTabData(0);
        for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
            if (m_AllEntries[m_FilteredIndices[i]].path == target) {
                m_Main->set_selected_library_index(i);
                SelectLibraryItem(i);
                break;
            }
        }
    });
    m_Main->on_hub_open_media([this](slint::SharedString path) {
        std::string target(path);
        m_ActiveTab = 2;
        m_Main->set_active_tab(2);
        LoadTabData(2);
        for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
            if (m_AllEntries[m_FilteredIndices[i]].path == target) {
                m_Main->set_selected_library_index(i);
                SelectLibraryItem(i);
                break;
            }
        }
    });
    m_Main->on_hub_play_music([this](int idx) {
        if (!m_MusicScanned) ScanMusicTracks();
        PlayMusicTrack(idx);
    });
    m_Main->on_hub_open_doc([this](slint::SharedString path) {
        std::string target(path);
        m_ActiveTab = 10;
        m_Main->set_active_tab(10);
        LoadTabData(10);
        for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
            if (m_AllEntries[m_FilteredIndices[i]].path == target) {
                m_Main->set_selected_library_index(i);
                SelectLibraryItem(i);
                break;
            }
        }
    });

    // Documents (Tab 10)
    m_Main->on_doc_page_clicked([this](int idx) { ProjectDocPage(idx); });
    m_Main->on_doc_prev_page([this]() {
        if (m_DocPagePaths.empty()) return;
        int prev = std::max(0, m_ActiveDocPageIdx - 1);
        m_Main->set_active_doc_page_idx(prev);
        ProjectDocPage(prev);
    });
    m_Main->on_doc_next_page([this]() {
        if (m_DocPagePaths.empty()) return;
        int next = std::min(static_cast<int>(m_DocPagePaths.size()) - 1, m_ActiveDocPageIdx + 1);
        m_Main->set_active_doc_page_idx(next);
        ProjectDocPage(next);
    });
    m_Main->on_import_document_clicked([this]() { ImportFileAsync(false, false, 10); });

    // QR desde texto — previsualización en vivo y guardado como PNG en Documentos.
    m_Main->on_qr_text_changed([this](slint::SharedString txt) {
        std::string s(txt);
        if (s.empty()) {
            m_Main->set_has_qr_preview(false);
            m_Main->set_qr_status("");
            return;
        }
        m_Main->set_qr_preview_image(GenerateQrImage(s, 6, 2));
        m_Main->set_has_qr_preview(true);
        m_Main->set_qr_status("");
    });
    m_Main->on_qr_save_clicked([this](slint::SharedString textS, slint::SharedString nameS) {
        std::string text(textS);
        if (text.empty()) return;

        // Nombre de archivo seguro (sin separadores ni caracteres problemáticos).
        std::string stem(nameS);
        std::string safe;
        for (char c : stem) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 0x80) { safe.push_back(c); continue; }
            if (std::isalnum(uc) || c == ' ' || c == '-' || c == '_') safe.push_back(c);
            else safe.push_back('_');
        }
        while (!safe.empty() && safe.front() == ' ') safe.erase(safe.begin());
        while (!safe.empty() && safe.back()  == ' ') safe.pop_back();
        if (safe.empty()) safe = "qr";

        fs::path dir  = ProyecThor::DocumentsPath();
        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::path out = dir / (safe + ".png");
        // Evita pisar un archivo existente: añade sufijo incremental.
        int n = 1;
        while (fs::exists(out, ec)) {
            out = dir / (safe + "_" + std::to_string(++n) + ".png");
        }

        if (SaveQrPng(text, PathToUtf8(out))) {
            m_Main->set_qr_status(slint::SharedString(
                "QR guardado como \"" + PathToUtf8(out.stem()) + "\" en Documentos."));
            // Refresca la lista de documentos para que aparezca de inmediato.
            m_CachedTabEntries.erase(10);
            m_ActiveTab = 10;
            m_Main->set_active_tab(10);
            LoadTabData(10);
            m_Main->set_qr_modal_open(false);
        } else {
            m_Main->set_qr_status("No se pudo generar el QR (¿texto demasiado largo?).");
        }
    });

    // Debug & Firewall
    m_Main->on_debug_clear_log([this]() {
        m_DebugLogLines.clear();
        m_Main->set_debug_log("");
    });
    m_Main->on_debug_refresh([this]() { RefreshDebugInfo(); });
    m_Main->on_open_firewall_ports([this]() { OpenFirewallPortsAsync(); });

    // libmpv media transport
    m_Main->on_toggle_pause([this]() {
        bool next = !m_Player.IsPaused();
        m_Player.SetPause(next);
        m_Main->set_media_paused(next);
    });
    m_Main->on_stop_media([this]() { ClearBackground(); });
    m_Main->on_toggle_mute([this]() {
        bool next = !m_Player.IsMuted();
        m_Player.SetMute(next);
        m_Main->set_media_muted(next);
    });
    m_Main->on_toggle_loop([this]() {
        bool next = !m_Main->get_media_loop();
        m_Main->set_media_loop(next);
        if (m_HasVideoBg && !m_CurrentBgPath.empty()) {
            m_Player.Play(m_CurrentBgPath, next, m_Player.IsMuted());
        }
    });
    m_Main->on_volume_changed([this](float v) {
        m_Player.SetVolume(static_cast<int>(v));
    });
    m_Main->on_seek_changed([this](float pos) {
        m_Player.SetPosition(pos);
        m_LastMediaPos = pos;
        m_LastMediaSec = -1;
    });

    // Design, Full Style Editor, Shaders, Transitions, Tiles & Alerts
    m_Main->on_style_changed([this]() {
        SyncProjectorStyle();
        if (!m_Main->get_style_editor_open()) SavePersistedState();
    });
    m_Main->on_font_preset_selected([this](int targetBox, int presetIdx) {
        std::string resolved = ResolveFontPreset(presetIdx);
        if (targetBox == 1) {
            m_Main->set_index_font_family_idx(presetIdx);
            m_Main->set_index_font_family_name(slint::SharedString(resolved));
        } else {
            m_Main->set_font_family_idx(presetIdx);
            m_Main->set_font_family_name(slint::SharedString(resolved));
        }
        SyncProjectorStyle();
        if (!m_Main->get_style_editor_open()) SavePersistedState();
    });
    m_Main->on_system_font_selected([this](int targetBox, slint::SharedString name) {
        std::string fName(name);
        if (targetBox == 1) {
            m_Main->set_index_font_family_idx(fName.empty() ? 0 : -1);
            m_Main->set_index_font_family_name(name);
        } else {
            m_Main->set_font_family_idx(fName.empty() ? 0 : -1);
            m_Main->set_font_family_name(name);
        }
        SyncProjectorStyle();
        if (!m_Main->get_style_editor_open()) SavePersistedState();
    });
    m_Main->on_font_search_changed([this](slint::SharedString q) {
        RefreshFontPickerModel(std::string(q));
    });
    m_Main->on_open_new_style_editor([this]() { OpenNewStyleEditor(); });
    m_Main->on_open_edit_style_editor([this](int idx) { OpenEditStyleEditor(idx); });
    m_Main->on_save_style_editor([this](slint::SharedString name) { SaveStyleEditor(std::string(name)); });
    m_Main->on_cancel_style_editor([this]() { CancelStyleEditor(); });
    m_Main->on_duplicate_custom_style([this](int idx) { DuplicateCustomStyle(idx); });
    m_Main->on_load_custom_style([this](int idx) { LoadCustomStyle(idx); });
    m_Main->on_delete_custom_style([this](int idx) { DeleteCustomStyle(idx); });
    m_Main->on_select_style_target([this](int mode) { SelectStyleTarget(mode); });
    m_Main->on_apply_style_to_screen([this](int styleIdx, int screenIdx) { ApplyCustomStyleToScreen(styleIdx, screenIdx); });
    m_Main->on_set_screen_own_style([this](int screenIdx, bool enabled) { SetScreenOwnStyle(screenIdx, enabled); });
    m_Main->on_text_color_rgb_changed([this](float r, float g, float b) {
        SetTextColorRGB(static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)));
    });
    m_Main->on_text_color_selected([this](int idx) { SelectTextColor(idx); });
    m_Main->on_index_color_rgb_changed([this](float r, float g, float b) {
        SetIndexColorRGB(static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                         static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                         static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)));
    });
    m_Main->on_index_color_selected([this](int idx) { SelectIndexColor(idx); });
    m_Main->on_shaders_changed([this]() { ApplyShaders(); });
    m_Main->on_reset_shaders([this]() { ResetShaders(); });
    m_Main->on_eq_changed([this]() { ApplyEqualizer(); });
    m_Main->on_apply_eq_preset([this](int idx) { ApplyEqPreset(idx); });
    m_Main->on_transition_changed([this]() {
        SyncProjectorStyle();
        SavePersistedState();
    });
    m_Main->on_select_transition_preset([this](int idx) { SelectTransitionPreset(idx); });
    m_Main->on_replay_transition_preview([this]() { ReplayTransitionPreview(); });
    m_Main->on_test_transition([this]() { TriggerSlideTransition(); });
    m_Main->on_tiles_changed([this]() {
        SyncTilesState();
        SavePersistedState();
    });

    m_Main->on_send_public_alert([this](slint::SharedString msg) {
        std::string sMsg(msg);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].alertText = sMsg;
        }
        m_HasActivePublicAlert = false;
        for (const auto& sc : m_Screens) {
            if (!sc.alertText.empty()) { m_HasActivePublicAlert = true; break; }
        }
        m_AlertTickerPos  = 0.0f;
        m_AlertPulsePhase = 0.0f;
        SyncAlertConfig();
        SyncScreenOutputs();
        if (!m_AudienceFrozen && !m_Screens[0].alertText.empty()) {
            if (m_Main->get_alert_anim_mode() == 2) {
                m_Projector->set_alert_visible(false);
                slint::ComponentWeakHandle<ProjectorWindow> weakProj(m_Projector);
                slint::Timer::single_shot(std::chrono::milliseconds(25), [weakProj]() {
                    if (auto locked = weakProj.lock()) {
                        (*locked)->set_alert_visible(true);
                    }
                });
            } else {
                m_Projector->set_alert_visible(true);
            }
        }
    });
    m_Main->on_send_stage_alert([this](slint::SharedString msg) {
        m_Main->set_active_stage_alert(msg);
        if (m_Screens.size() > 2) m_Screens[2].alertText = std::string(msg);
        m_Stage->set_stage_alert(msg);
        SyncScreenOutputs();
    });
    m_Main->on_alert_config_changed([this]() { SyncAlertConfig(); });
    m_Main->on_clear_alerts([this]() {
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].alertText.clear();
        }
        m_HasActivePublicAlert = false;
        for (const auto& sc : m_Screens) {
            if (!sc.alertText.empty()) { m_HasActivePublicAlert = true; break; }
        }
        if (IsScreenTargeted(2)) {
            m_Main->set_active_stage_alert("");
            m_Stage->set_stage_alert("");
        }
        SyncScreenOutputs();
    });

    // Custom Overlay & Time/Clock Editor (per-screen & multi-screen routing)
    m_Main->on_open_new_overlay_editor([this](bool withClock) {
        OpenNewOverlayEditor(withClock);
    });
    m_Main->on_open_edit_overlay_editor([this](int idx) {
        OpenEditOverlayEditor(idx);
    });
    m_Main->on_open_edit_active_overlay([this]() {
        OpenEditActiveScreenOverlay();
    });
    m_Main->on_save_overlay_editor([this](bool alsoApply) {
        SaveOverlayEditor(alsoApply);
    });
    m_Main->on_apply_overlay_editor([this](int explicitTarget) {
        SyncOverlayEditorStateFromUi();
        ApplyOverlayConfigToTargets(m_EditingOverlay, explicitTarget);
    });
    m_Main->on_cancel_overlay_editor([this]() {
        CancelOverlayEditor();
    });
    m_Main->on_ov_editor_set_minutes([this](int mins) {
        OverlayEditorSetMinutes(mins);
    });
    m_Main->on_ov_editor_set_manual_time([this](slint::SharedString mStr, slint::SharedString sStr) {
        OverlayEditorSetManualTime(std::string(mStr), std::string(sStr));
    });
    m_Main->on_ov_editor_timer_toggle([this]() {
        OverlayEditorTimerToggle();
    });
    m_Main->on_ov_editor_timer_reset([this]() {
        OverlayEditorTimerReset();
    });
    m_Main->on_ov_editor_select_text_color([this](int r, int g, int b) {
        SetOverlayEditorTextColorRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    });
    m_Main->on_ov_editor_select_bg_color([this](int r, int g, int b) {
        SetOverlayEditorBgColorRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    });
    m_Main->on_ov_editor_select_border_color([this](int r, int g, int b) {
        SetOverlayEditorBorderColorRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    });
    m_Main->on_ov_editor_props_changed([this]() {
        SyncOverlayEditorStateFromUi();
    });

    // Photoshop-Style Real Active Layers & Multi-Screen / Multi-Room Matrix
    m_Main->on_remove_active_layer([this](slint::SharedString id) {
        RemoveActiveLayer(std::string(id));
    });
    m_Main->on_select_output_target([this](int mode) {
        m_OutputTargetMode = std::max(0, mode);
        m_Main->set_output_target_mode(m_OutputTargetMode);
        if (m_OutputTargetMode >= 1 && (m_OutputTargetMode - 1) < static_cast<int>(m_Screens.size())) {
            m_InspectedScreenIdx = m_OutputTargetMode - 1;
            m_Main->set_inspected_screen_idx(m_InspectedScreenIdx);
        }
        SyncInspectedScreenToPreview();
        RefreshActiveLayersModel();
        RefreshScreenMatrixModel();
    });
    m_Main->on_inspect_screen([this](int scrIdx) {
        if (scrIdx >= 0 && scrIdx < static_cast<int>(m_Screens.size())) {
            m_InspectedScreenIdx = scrIdx;
            m_Main->set_inspected_screen_idx(m_InspectedScreenIdx);
            SyncInspectedScreenToPreview();
            RefreshActiveLayersModel();
            RefreshScreenMatrixModel();
        }
    });
    m_Main->on_screen_toggle_layer([this](int scrIdx, slint::SharedString layerId) {
        ToggleLayerOnScreen(scrIdx, std::string(layerId));
    });
    m_Main->on_screen_send_current([this](int scrIdx) {
        SendCurrentSelectionToScreen(scrIdx);
    });
    m_Main->on_screen_clone_public([this](int scrIdx) {
        ClonePublicToScreen(scrIdx);
    });
    m_Main->on_screen_clear_all([this](int scrIdx) {
        ClearAllOnScreen(scrIdx);
    });
    m_Main->on_create_extra_screen([this](slint::SharedString name) {
        CreateExtraScreen(std::string(name));
    });
    m_Main->on_delete_extra_screen([this](int scrIdx) {
        DeleteExtraScreen(scrIdx);
    });

    // Pads
    m_Main->on_pad_triggered([this](int idx) { TriggerPad(idx); });
    m_Main->on_pad_saved([this](int idx) { SaveToPad(idx); });

    // Capture Devices
    m_Main->on_refresh_capture_devices([this]() { RefreshCaptureDevices(); });
    m_Main->on_start_capture_device([this](int idx) { StartCaptureDevice(idx); });
    m_Main->on_stop_capture_or_url([this]() {
        m_Main->set_selected_capture_idx(-1);
        ClearBackground();
        m_Main->set_capture_status("Captura detenida.");
    });

    // Network Hub (Servicios independientes — LAN portado de ImGui StreamingPanel)
    m_Main->on_toggle_lan_stream([this]() { ToggleLanStream(); });
    m_Main->on_lan_config_changed([this]() { ApplyLanConfig(); });
    m_Main->on_copy_lan_url([this]() {
        if (IsLanRunning()) {
            CopyToClipboard(m_LanServer->GetBaseURL());
            m_Main->set_lan_copy_feedback("¡Copiado!");
            slint::ComponentWeakHandle<MainWindow> weakM(m_Main);
            slint::Timer::single_shot(std::chrono::milliseconds(1600), [weakM]() {
                if (auto locked = weakM.lock()) {
                    (*locked)->set_lan_copy_feedback("Copiar");
                }
            });
        }
    });
    m_Main->on_scan_smart_tvs([this]() { ScanSmartTvsAsync(); });
    m_Main->on_cast_to_smart_tv([this](int idx) { CastToSmartTv(idx); });
    m_Main->on_cast_to_smart_tv_ip([this](slint::SharedString ip) { CastToSmartTvIp(std::string(ip)); });
    m_Main->on_stop_smart_tv_cast([this](int idx) { StopSmartTvCast(idx); });
    m_Main->on_toggle_chat_server([this]() { ToggleChatServer(); });
    m_Main->on_toggle_mobile_server([this]() { ToggleMobileServer(); });
    m_Main->on_regenerate_mobile_pin([this]() { RegenerateMobilePin(); });
    m_Main->on_send_chat_message([this](slint::SharedString msg) {
        m_ChatStore.PostChatMessage("PC Operador", std::string(msg));
        RefreshChatLog();
    });
    m_Main->on_send_osc([this](slint::SharedString h, slint::SharedString p,
                               slint::SharedString a, slint::SharedString args) {
        SendOscCommand(std::string(h), std::string(p), std::string(a), std::string(args));
    });

    // Themes, Import & Settings
    m_Main->on_theme_selected([this](int idx) {
        ApplyThemePreset(idx);
        SavePersistedState();
    });
    m_Main->on_import_file_clicked([this]() { ImportFileAsync(false, false); });
    m_Main->on_import_background_clicked([this]() { ImportFileAsync(true, false); });
    m_Main->on_import_overlay_clicked([this]() { ImportFileAsync(false, true); });
    m_Main->on_dock_bg_selected([this](int idx) { SelectDockBackground(idx); });
    m_Main->on_overlay_layer_selected([this](int idx) { SelectOverlayLayer(idx); });
    m_Main->on_overlay_layer_add_to_queue([this](int idx) { AddOverlayLayerToQueue(idx); });
    m_Main->on_dock_solid_color_selected([this](int idx) { SelectSolidBgColor(idx); });
    m_Main->on_open_legacy_imgui([this]() { LaunchLegacyImGui(); });
    m_Main->on_fsr_changed([this](bool enabled) {
        m_UpscalerMode = enabled ? (m_UpscalerMode > 0 ? m_UpscalerMode : 2) : 0;
        m_Main->set_upscaler_mode(m_UpscalerMode);
        ApplyGpuAndUpscalerConfig();
        SavePersistedState();
    });
    m_Main->on_gpu_config_changed([this]() {
        ApplyGpuAndUpscalerConfig();
        SavePersistedState();
    });
    m_Main->on_apply_low_spec_preset([this]() {
        ApplyLowSpecPreset();
    });
    m_Main->on_purge_ram_cache([this]() {
        PurgeMemoryCaches();
    });
    m_Main->on_performance_mode_changed([this](bool perf) {
        m_HoverPlayer.SetRenderSize(perf ? 256 : 320, perf ? 144 : 180);
        UpdatePlayerRenderSize();
        ApplyTargetFps(m_TargetFps);
        if (perf) PurgeMemoryCaches();
        SavePersistedState();
    });
    // Vista previa del operador: apagar / limitar FPS / priorizar salida.
    // No toca al reproductor principal ni a la salida pública; solo cambia
    // cuánto se entrega a la vista previa (ver DeliverVideoFrameToOutputs).
    m_Main->on_preview_config_changed([this]() {
        m_LastPreviewDeliverMs = 0; // re-arma el gate para que surta efecto ya
        SavePersistedState();
    });
    // Idioma de la interfaz: aplica la traducción empaquetada en vivo y la
    // persiste. slint::select_bundled_translation re-traduce todas las
    // cadenas @tr(...) visibles sin reiniciar la app.
    m_Main->on_language_changed([this](slint::SharedString code) {
        ApplyUiLanguage(std::string(code));
        SavePersistedState();
    });
    m_Main->on_fullscreen_changed([this](bool fs) {
        if (m_ProjectorVisible) {
            m_Projector->window().set_fullscreen(fs);
        }
        SavePersistedState();
    });
    m_Main->on_projection_monitor_changed([this](int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_Monitors.size())) return;
        m_ProjectionMonitorIdx = idx;
        // Si la proyección ya está abierta, muévela al monitor elegido ahora.
        if (m_ProjectorVisible) {
            const bool wantFs = m_Main->get_fullscreen_output();
            if (wantFs) m_Projector->window().set_fullscreen(false);
            PositionProjectorOnSelectedMonitor();
            if (wantFs) {
                slint::ComponentWeakHandle<ProjectorWindow> weakProj(m_Projector);
                slint::Timer::single_shot(std::chrono::milliseconds(60), [weakProj]() {
                    if (auto locked = weakProj.lock()) {
                        (*locked)->window().set_fullscreen(true);
                        (*locked)->invoke_focus_window();
                    }
                });
            }
        }
        SavePersistedState();
    });
    m_Main->on_visual_effects_changed([this]() {
        m_EnableGlassTranslucency = m_Main->get_enable_glass_translucency();
        m_EnableGlassSheen        = m_Main->get_enable_glass_sheen();
        m_EnableAmbientBg         = m_Main->get_enable_ambient_bg();
        ApplyThemePreset(m_ActiveThemeIdx);
        SavePersistedState();
    });
    m_Main->on_check_github_updates([this]() { CheckGitHubUpdatesAsync(); });
    auto openExternalUrl = [this](const std::string& url) {
        AppendDebugLog("Abriendo enlace externo en el navegador: " + url);
        std::thread([url]() {
#ifdef _WIN32
            ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
            std::string cmd = "open '" + url + "' >/dev/null 2>&1 &";
            int rc = std::system(cmd.c_str());
            (void)rc;
#else
            std::string cmd = "xdg-open '" + url + "' >/dev/null 2>&1 &";
            int rc = std::system(cmd.c_str());
            (void)rc;
#endif
        }).detach();
    };
    m_Main->on_open_kofi_donations([openExternalUrl]() {
        openExternalUrl("https://ko-fi.com/vixcho");
    });
    m_Main->on_open_website([openExternalUrl]() {
        openExternalUrl("https://thevixcho.github.io/ProyecThor/");
    });
    m_Main->on_open_discord([openExternalUrl]() {
        openExternalUrl("https://discord.gg/QJ6wGHQjs6");
    });
    m_Main->on_open_instagram([openExternalUrl]() {
        openExternalUrl("https://www.instagram.com/theproyecthor/");
    });
    m_Main->on_open_whatsapp([openExternalUrl]() {
        openExternalUrl("https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19");
    });
    auto updateHelpQr = [this](int idx) {
        struct HelpQrEntry {
            const char* label;
            const char* url;
        };
        static constexpr HelpQrEntry kHelpQrEntries[] = {
            {"Sitio Web Oficial", "https://thevixcho.github.io/ProyecThor/"},
            {"Discord (Soporte y Comunidad)", "https://discord.gg/QJ6wGHQjs6"},
            {"Instagram (@theproyecthor)", "https://www.instagram.com/theproyecthor/"},
            {"Canal de WhatsApp", "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"},
        };
        const int safeIdx = (idx >= 0 && idx < 4) ? idx : 1;
        const auto& entry = kHelpQrEntries[safeIdx];
        m_Main->set_help_qr_idx(safeIdx);
        m_Main->set_help_qr_label(slint::SharedString(entry.label));
        m_Main->set_help_qr_url(slint::SharedString(entry.url));
        m_Main->set_help_qr_image(GenerateQrImage(entry.url, 5, 2));
    };
    updateHelpQr(1);
    m_Main->on_select_help_qr([updateHelpQr](int idx) {
        updateHelpQr(idx);
    });
    m_Main->on_tutorial_preference_changed([this](bool showOnStart) {
        m_Main->set_show_tutorial_on_start(showOnStart);
        m_Main->set_tutorial_dont_show_again(!showOnStart);
        SavePersistedState();
    });
    m_Main->on_item_action_play([this](int kind, slint::SharedString path) {
        HandleItemPlay(kind, std::string(path));
    });
    m_Main->on_item_action_queue([this](int kind, slint::SharedString path) {
        HandleItemQueue(kind, std::string(path));
    });
    m_Main->on_item_action_rename([this](int kind, slint::SharedString path, slint::SharedString newName) {
        HandleItemRename(kind, std::string(path), std::string(newName));
    });
    m_Main->on_item_action_delete([this](int kind, slint::SharedString path) {
        HandleItemDelete(kind, std::string(path));
    });
}

void SlintApp::LoadTabData(int tab) {
    m_AllEntries.clear();
    m_SlidesModel->clear();
    m_SlideRowsModel->clear();
    m_Main->set_selected_library_index(-1);
    m_Main->set_selected_library_path("");
    m_Main->set_active_slide_index(-1);
    m_Main->set_selected_song_author("");
    m_Main->set_chapter_count(1);

    if (tab == 1) {
        m_Main->set_selected_item_title("Biblia — Selecciona un libro");
        if (m_BiblePaths.empty()) {
            ScanBibles();
        }
        RefreshLibraryModel();
        return;
    }

    auto cacheIt = m_CachedTabEntries.find(tab);
    if (cacheIt != m_CachedTabEntries.end()) {
        m_AllEntries = cacheIt->second;
    } else {
        if (tab == 0) {
            ScanSongs();
        } else if (tab == 2) {
            ScanMedia(false);
        } else if (tab == 10) {
            ScanDocuments();
        }
        m_CachedTabEntries[tab] = m_AllEntries;
    }

    if (tab == 0) {
        m_Main->set_selected_item_title("Canciones — Selecciona una canción");
    } else if (tab == 2) {
        m_Main->set_selected_item_title("Multimedia — Videos e Imágenes");
    } else if (tab == 10) {
        m_Main->set_doc_title("Documentos — Selecciona un PDF o imagen");
    }

    RefreshLibraryModel();
}

void SlintApp::RefreshLibraryModel() {
    m_LibraryModel->clear();
    m_FilteredIndices.clear();
    m_SearchMatchedStanzas.clear();

    const std::vector<std::string>* activePlPaths = nullptr;
    if (m_ActiveTab == 0 && m_PlaylistFilterActive &&
        m_ActiveSongPlaylistIdx >= 0 &&
        m_ActiveSongPlaylistIdx < static_cast<int>(m_SongPlaylists.size())) {
        activePlPaths = &m_SongPlaylists[m_ActiveSongPlaylistIdx].songPaths;
    }

    std::string needle = ToLowerUtf8(m_SearchQuery);
    for (int i = 0; i < static_cast<int>(m_AllEntries.size()); ++i) {
        const auto& e = m_AllEntries[i];
        if (activePlPaths) {
            if (std::find(activePlPaths->begin(), activePlPaths->end(), e.path) == activePlPaths->end()) {
                continue;
            }
        }
        int matchedStanza = -1;
        if (!needle.empty()) {
            bool metaMatch = (ToLowerUtf8(e.title).find(needle) != std::string::npos ||
                              ToLowerUtf8(e.subtitle).find(needle) != std::string::npos);
            if (m_ActiveTab == 0) {
                auto stanzas = ParseSongStanzas(e.path);
                for (size_t si = 0; si < stanzas.size(); ++si) {
                    if (ToLowerUtf8(stanzas[si]).find(needle) != std::string::npos) {
                        matchedStanza = static_cast<int>(si);
                        break;
                    }
                }
                if (matchedStanza >= 0) {
                    m_SearchMatchedStanzas[e.path] = matchedStanza;
                }
            }
            if (!metaMatch && matchedStanza < 0) {
                continue;
            }
        }
        m_FilteredIndices.push_back(i);
        LibraryItem item;
        item.title    = slint::SharedString(e.title);
        std::string sub = e.subtitle;
        if (matchedStanza >= 0) {
            sub = sub.empty()
                ? ("★ Estrofa " + std::to_string(matchedStanza + 1))
                : (sub + " • ★ Estrofa " + std::to_string(matchedStanza + 1));
        }
        item.subtitle = slint::SharedString(sub);
        item.path     = slint::SharedString(e.path);
        item.has_thumbnail = false;
        if ((m_ActiveTab == 2 || m_ActiveTab == 3) && (e.isVideo || e.isImage || e.isOverlay)) {
            slint::Image thumb;
            if (LoadOrCreateThumbnail(e.path, e.isVideo, thumb)) {
                item.thumbnail     = thumb;
                item.has_thumbnail = true;
            }
        }
        m_LibraryModel->push_back(item);
    }
}

void SlintApp::SelectLibraryItem(int filteredIdx) {
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(m_FilteredIndices.size())) return;
    int realIdx = m_FilteredIndices[filteredIdx];
    const auto& entry = m_AllEntries[realIdx];
    m_Main->set_selected_library_path(slint::SharedString(entry.path));

    if (m_ActiveTab == 0) {
        m_Main->set_song_editor_open(false);
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_song_author(slint::SharedString(entry.author));
        auto stanzas = ParseSongStanzas(entry.path);
        int matchedStanza = -1;
        if (auto it = m_SearchMatchedStanzas.find(entry.path); it != m_SearchMatchedStanzas.end()) {
            matchedStanza = it->second;
        } else if (!m_SearchQuery.empty()) {
            std::string needle = ToLowerUtf8(m_SearchQuery);
            for (size_t si = 0; si < stanzas.size(); ++si) {
                if (ToLowerUtf8(stanzas[si]).find(needle) != std::string::npos) {
                    matchedStanza = static_cast<int>(si);
                    break;
                }
            }
        }
        m_SlidesModel->clear();
        for (size_t i = 0; i < stanzas.size(); ++i) {
            SlideItem s;
            std::string badge = "Estrofa " + std::to_string(i + 1);
            if (static_cast<int>(i) == matchedStanza) {
                badge += " • ★ Coincide";
            }
            s.badge    = slint::SharedString(badge);
            s.body     = slint::SharedString(stanzas[i]);
            s.ref_text = slint::SharedString(entry.title);
            m_SlidesModel->push_back(s);
        }
        m_Main->set_active_slide_index(
            (matchedStanza >= 0 && matchedStanza < static_cast<int>(stanzas.size())) ? matchedStanza : -1);
        RebuildSlideGridRows();
    } else if (m_ActiveTab == 10) {
        OpenDocumentAsync(entry.path);
    } else if (m_ActiveTab == 2) {
        // Multimedia: Load into Preview ONLY (does NOT interrupt Live output until user clicks "Enviar a Pantalla" or "+ Cola Videos")
        m_SelectedPreviewEntry    = entry;
        m_HasSelectedPreviewEntry = true;
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_media_info(slint::SharedString(entry.subtitle + " • " + PathToUtf8(fs::path(entry.path).filename())));

        slint::Image thumb;
        if ((entry.isVideo || entry.isImage) && LoadOrCreateThumbnail(entry.path, entry.isVideo, thumb)) {
            m_Main->set_selected_media_preview(thumb);
            m_Main->set_has_selected_media_preview(true);
        } else {
            m_Main->set_has_selected_media_preview(false);
        }
        m_SlidesModel->clear();
        m_Main->set_active_slide_index(-1);
    } else if (m_ActiveTab == 3) {
        // Fondos / Overlays: Preview + immediate live apply
        m_SelectedPreviewEntry    = entry;
        m_HasSelectedPreviewEntry = true;
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_media_info(slint::SharedString(entry.subtitle + " • " + PathToUtf8(fs::path(entry.path).filename())));

        slint::Image thumb;
        if (LoadOrCreateThumbnail(entry.path, entry.isVideo, thumb)) {
            m_Main->set_selected_media_preview(thumb);
            m_Main->set_has_selected_media_preview(true);
        } else {
            m_Main->set_has_selected_media_preview(false);
        }
        ProjectSelectedMedia();
        m_SlidesModel->clear();
        m_Main->set_active_slide_index(-1);
    }
}

void SlintApp::ProjectSlide(int index) {
    if (index < 0 || index >= static_cast<int>(m_SlidesModel->row_count())) return;
    if (m_ActiveTab >= 2) return;

    auto slide = m_SlidesModel->row_data(index);
    if (!slide.has_value()) return;

    m_LiveIsBible = (m_ActiveTab == 1);
    m_RawLiveText = std::string(slide->body);
    m_RawLiveRef  = std::string(slide->ref_text);

    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        SetScreenLiveText(m_Screens[i], m_RawLiveText, m_RawLiveRef, m_LiveIsBible);
    }

    if (m_ActiveTab == 1) {
        PushBibleHistory(std::string(slide->badge), std::string(slide->body), std::string(slide->ref_text));
    }
    SyncStageNextSlide();
    SyncTilesState();
    SyncScreenOutputs();
    if (IsScreenTargeted(0)) {
        TriggerSlideTransition();
    }
}

void SlintApp::SyncStageNextSlide() {
    int cur = m_Main->get_active_slide_index();
    int count = static_cast<int>(m_SlidesModel->row_count());
    if (cur >= 0 && cur + 1 < count) {
        auto next = m_SlidesModel->row_data(cur + 1);
        if (next.has_value()) {
            m_Stage->set_next_text(next->body);
            return;
        }
    }
    m_Stage->set_next_text("");
}

void SlintApp::ClearText() {
    if (IsScreenTargeted(0)) {
        m_RawLiveText.clear();
        m_RawLiveRef.clear();
        m_LiveIsBible = false;
        m_Main->set_active_slide_index(-1);
    }
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].rawText.clear();
        m_Screens[i].rawRef.clear();
        m_Screens[i].displayText.clear();
        m_Screens[i].displayRef.clear();
        m_Screens[i].isBible = false;
    }
    if (IsScreenTargeted(2)) {
        m_Stage->set_next_text("");
    }
    SyncScreenOutputs();
}

void SlintApp::ClearBackground() {
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].bgName       = "Sin fondo";
        m_Screens[i].bgPath.clear();
        m_Screens[i].hasBgImage   = false;
        m_Screens[i].bgIsStatic   = false;
        m_Screens[i].bgIsVideo    = false;
        m_Screens[i].hasBgThumb   = false;
        m_Screens[i].bgSolidColor = slint::Color::from_rgb_uint8(3, 3, 4);
        m_Screens[i].bgSolidRgb[0] = 0.0f;
        m_Screens[i].bgSolidRgb[1] = 0.0f;
        m_Screens[i].bgSolidRgb[2] = 0.0f;
    }

    bool anyVideoStillUsed = false;
    for (const auto& sc : m_Screens) {
        if (sc.bgIsVideo) { anyVideoStillUsed = true; break; }
    }
    if (!anyVideoStillUsed) {
        m_Player.Stop();
        m_CurrentBgPath.clear();
        m_HasStaticImage = false;
        m_HasVideoBg = false;
        m_Main->set_media_position(0.0f);
        m_Main->set_media_time_label("00:00 / 00:00");
    }
    if (IsScreenTargeted(1)) {
        ClearLanFrame();
    }
    SyncScreenOutputs();
}

void SlintApp::ClearOverlay() {
    if (IsScreenTargeted(0)) {
        m_ActiveOverlayPath.clear();
    }
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].overlayName = "Sin overlay";
        m_Screens[i].overlayPath.clear();
        m_Screens[i].hasOverlayImage = false;
        m_Screens[i].hasOverlayThumb = false;
        m_Screens[i].hasCustomOverlay = false;
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::ClearAll() {
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].hasCustomOverlay = false;
        m_Screens[i].alertText.clear();
    }
    ClearText();
    ClearBackground();
    ClearOverlay();
}

void SlintApp::ScanSystemFonts() {
    m_AllSystemFonts.clear();
    m_SystemFontFiles.clear();
#ifdef _WIN32
    m_DefaultSansFont  = "Arial";
    m_DefaultSerifFont = "Georgia";
    m_DefaultMonoFont  = "Consolas";
    m_AllSystemFonts = {
        "Arial", "Bahnschrift", "Calibri", "Cambria", "Candara", "Comic Sans MS",
        "Consolas", "Constantia", "Corbel", "Courier New", "Ebrima",
        "Franklin Gothic Medium", "Gabriola", "Georgia", "Impact", "Lucida Console",
        "Montserrat", "Open Sans", "Roboto", "Segoe UI", "Tahoma",
        "Times New Roman", "Trebuchet MS", "Verdana"
    };
#elif defined(__APPLE__)
    m_DefaultSansFont  = "Helvetica Neue";
    m_DefaultSerifFont = "Georgia";
    m_DefaultMonoFont  = "Menlo";
    m_AllSystemFonts = {
        "Arial", "Avenir", "Avenir Next", "Courier New", "Futura", "Georgia",
        "Gill Sans", "Helvetica", "Helvetica Neue", "Impact", "Menlo",
        "Monaco", "Optima", "Palatino", "SF Pro", "Times New Roman",
        "Trebuchet MS", "Verdana"
    };
    const char* homeEnv = std::getenv("HOME");
    std::vector<fs::path> macFontDirs = {
        "/System/Library/Fonts",
        "/System/Library/Fonts/Supplemental",
        "/Library/Fonts"
    };
    if (homeEnv) macFontDirs.push_back(fs::path(homeEnv) / "Library/Fonts");
    for (const auto& dir : macFontDirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".ttf", ".otf" })) continue;
            std::string stem = PathToUtf8(entry.path().stem());
            if (stem.empty() || stem[0] == '.') continue;
            if (m_SystemFontFiles.find(stem) == m_SystemFontFiles.end()) {
                m_SystemFontFiles[stem] = PathToUtf8(entry.path());
            }
            if (std::find(m_AllSystemFonts.begin(), m_AllSystemFonts.end(), stem) == m_AllSystemFonts.end()) {
                m_AllSystemFonts.push_back(stem);
            }
        }
    }
    std::sort(m_AllSystemFonts.begin(), m_AllSystemFonts.end());
#else
    auto trimLine = [](std::string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
        return s;
    };
    m_DefaultSansFont  = trimLine(RunCommandCapture("fc-match -f '%{family[0]}' sans 2>/dev/null"));
    m_DefaultSerifFont = trimLine(RunCommandCapture("fc-match -f '%{family[0]}' serif 2>/dev/null"));
    m_DefaultMonoFont  = trimLine(RunCommandCapture("fc-match -f '%{family[0]}' monospace 2>/dev/null"));
    if (m_DefaultSansFont.empty())  m_DefaultSansFont  = "Noto Sans";
    if (m_DefaultSerifFont.empty()) m_DefaultSerifFont = "Noto Serif";
    if (m_DefaultMonoFont.empty())  m_DefaultMonoFont  = "Noto Sans Mono";

    std::string raw = RunCommandCapture("fc-list : family style file 2>/dev/null");
    std::istringstream iss(raw);
    std::string line;
    std::vector<std::string> unique;
    std::unordered_map<std::string, int> fontScore;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto firstColon = line.find(':');
        if (firstColon == std::string::npos) continue;
        std::string filePath = trimLine(line.substr(0, firstColon));
        std::string rest     = line.substr(firstColon + 1);
        auto stylePos        = rest.find(":style=");
        std::string famPart  = (stylePos != std::string::npos) ? rest.substr(0, stylePos) : rest;
        std::string stylePart = (stylePos != std::string::npos) ? rest.substr(stylePos + 7) : "";

        // fc-list can output comma-separated aliases per line; take primary family
        auto comma = famPart.find(',');
        if (comma != std::string::npos) famPart = famPart.substr(0, comma);
        std::string fam = trimLine(famPart);
        if (fam.empty()) continue;
        std::string low = ToLowerUtf8(fam);
        if (low.find(".pcf") != std::string::npos || low.find("cursor") != std::string::npos ||
            low.find("icomoon") != std::string::npos || low.find("fontawesome") != std::string::npos ||
            low.find("emoji") != std::string::npos || low.find("dingbat") != std::string::npos) {
            continue;
        }
        unique.push_back(fam);

        // Cache best .ttf / .otf file per family for LAN @font-face serving
        std::string lowFile = ToLowerUtf8(filePath);
        if (lowFile.size() >= 4 &&
            (lowFile.rfind(".ttf") == lowFile.size() - 4 || lowFile.rfind(".otf") == lowFile.size() - 4)) {
            std::string lowStyle = ToLowerUtf8(stylePart);
            int score = 1;
            if (lowStyle.find("regular") != std::string::npos) score = 5;
            else if (lowStyle.find("medium") != std::string::npos) score = 4;
            else if (lowStyle.find("bold") != std::string::npos) score = 3;
            if (lowStyle.find("italic") != std::string::npos || lowStyle.find("oblique") != std::string::npos) score -= 2;
            auto it = fontScore.find(fam);
            if (it == fontScore.end() || score > it->second) {
                fontScore[fam] = score;
                m_SystemFontFiles[fam] = filePath;
            }
        }
    }
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    m_AllSystemFonts = std::move(unique);
#endif
    RefreshFontPickerModel("");
}

void SlintApp::RefreshFontPickerModel(const std::string& filter) {
    m_SystemFontsModel->clear();
    std::string needle = ToLowerUtf8(filter);
    for (const auto& fam : m_AllSystemFonts) {
        if (!needle.empty() && ToLowerUtf8(fam).find(needle) == std::string::npos) continue;
        m_SystemFontsModel->push_back(slint::SharedString(fam));
    }
}

std::string SlintApp::ResolveFontPreset(int presetIdx) const {
    switch (presetIdx) {
        case 1: return m_DefaultSansFont;
        case 2: return m_DefaultSerifFont;
        case 3: return m_DefaultMonoFont;
        default: return "";
    }
}

std::string SlintApp::ResolveStyleFontName(const SavedTextStyle& st, bool indexBox) const {
    if (indexBox) {
        if (!st.indexFontName.empty()) return st.indexFontName;
        return ResolveFontPreset(st.indexFontFamIdx);
    }
    if (!st.fontName.empty()) return st.fontName;
    return ResolveFontPreset(st.fontFamIdx);
}

std::string SlintApp::ResolveFontFilePath(const std::string& family) const {
    if (family.empty() || family == "Predeterminada") return "";
    auto it = m_SystemFontFiles.find(family);
    if (it != m_SystemFontFiles.end()) return it->second;
    return "";
}

void SlintApp::SetScreenLiveText(ScreenOutputState& sc, const std::string& rawText, const std::string& rawRef, bool isBible) {
    sc.isBible     = isBible;
    sc.rawText     = rawText;
    sc.rawRef      = rawRef;
    sc.displayText = sc.textStyle.uppercase ? ToUpperUtf8(rawText) : rawText;
    sc.displayRef  = (isBible && sc.textStyle.indexUppercase) ? ToUpperUtf8(rawRef) : rawRef;
}

void SlintApp::ApplyStyleToProjector(const SavedTextStyle& st) {
    static const int kFontWeights[] = { 400, 700, 900 };
    int wtIdx    = std::clamp(st.fontWtIdx, 0, 2);
    int idxWtIdx = std::clamp(st.indexFontWtIdx, 0, 2);
    std::string mainFont = ResolveStyleFontName(st, false);
    std::string idxFont  = ResolveStyleFontName(st, true);

    m_Projector->set_text_size(st.textSize);
    m_Projector->set_text_fit_mode(st.textFitMode);
    m_Projector->set_text_align(st.textAlign);
    m_Projector->set_text_valign(st.textValign);
    m_Projector->set_text_shadow(st.shadow);
    m_Projector->set_text_outline(st.outline);
    m_Projector->set_text_glow(st.glow);
    m_Projector->set_text_neon(st.neon);
    m_Projector->set_text_3d(st.text3d);
    m_Projector->set_text_underline(st.underline);
    m_Projector->set_text_box_bg(st.boxBg);
    m_Projector->set_text_color(slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB));
    m_Projector->set_font_family_name(slint::SharedString(mainFont));
    m_Projector->set_font_weight_val(kFontWeights[wtIdx]);
    m_Projector->set_text_margin_h(st.marginH);
    m_Projector->set_lyrics_pos_x(st.lyricsPosX);
    m_Projector->set_lyrics_pos_y(st.lyricsPosY);
    m_Projector->set_lyrics_width(st.lyricsWidth);
    m_Projector->set_lyrics_height(st.lyricsHeight);

    m_Projector->set_index_box_enabled(st.indexEnabled);
    m_Projector->set_index_pos_x(st.indexPosX);
    m_Projector->set_index_pos_y(st.indexPosY);
    m_Projector->set_index_width(st.indexWidth);
    m_Projector->set_index_height(st.indexHeight);
    m_Projector->set_index_size(st.indexSize);
    m_Projector->set_index_align(st.indexAlign);
    m_Projector->set_index_color(slint::Color::from_rgb_uint8(st.indexColorR, st.indexColorG, st.indexColorB));
    m_Projector->set_index_font_family_name(slint::SharedString(idxFont));
    m_Projector->set_index_font_weight_val(kFontWeights[idxWtIdx]);
    m_Projector->set_index_shadow(st.indexShadow);
    m_Projector->set_index_outline(st.indexOutline);
    m_Projector->set_index_glow(st.indexGlow);
    m_Projector->set_index_box_bg(st.indexBoxBg);

    m_Projector->set_transition_type(m_Main->get_transition_type());
    m_Projector->set_transition_dur(static_cast<int64_t>(m_Main->get_transition_duration_ms()));
}

void SlintApp::SyncProjectorStyle() {
    std::string mainFont(m_Main->get_font_family_name());
    if (mainFont.empty() && m_Main->get_font_family_idx() > 0) {
        mainFont = ResolveFontPreset(m_Main->get_font_family_idx());
        m_Main->set_font_family_name(slint::SharedString(mainFont));
    }
    std::string idxFont(m_Main->get_index_font_family_name());
    if (idxFont.empty() && m_Main->get_index_font_family_idx() > 0) {
        idxFont = ResolveFontPreset(m_Main->get_index_font_family_idx());
        m_Main->set_index_font_family_name(slint::SharedString(idxFont));
    }

    if (m_Main->get_style_editor_open()) {
        // While editing in Style Editor, live-preview onto Projector only if targeting All or Público
        if (!m_AudienceFrozen && (m_StyleTargetMode == 0 || m_StyleTargetMode == 1)) {
            ApplyStyleToProjector(CaptureCurrentStyleFromUI(""));
        }
        return;
    }

    // Normal mode: every screen uses its own independent SavedTextStyle
    for (auto& sc : m_Screens) {
        if (!sc.rawText.empty()) {
            sc.displayText = sc.textStyle.uppercase ? ToUpperUtf8(sc.rawText) : sc.rawText;
        }
        if (!sc.rawRef.empty()) {
            sc.displayRef = (sc.isBible && sc.textStyle.indexUppercase) ? ToUpperUtf8(sc.rawRef) : sc.rawRef;
        }
    }
    SyncScreenOutputs();
}

SlintApp::SavedTextStyle SlintApp::CaptureCurrentStyleFromUI(const std::string& name) const {
    SavedTextStyle st;
    st.name            = name;
    st.textSize        = m_Main->get_text_size();
    st.marginH         = m_Main->get_text_margin_h();
    st.lyricsPosX      = m_Main->get_lyrics_pos_x();
    st.lyricsPosY      = m_Main->get_lyrics_pos_y();
    st.lyricsWidth     = m_Main->get_lyrics_width();
    st.lyricsHeight    = m_Main->get_lyrics_height();
    st.indexEnabled    = m_Main->get_index_box_enabled();
    st.indexPosX       = m_Main->get_index_pos_x();
    st.indexPosY       = m_Main->get_index_pos_y();
    st.indexWidth      = m_Main->get_index_width();
    st.indexHeight     = m_Main->get_index_height();
    st.indexSize       = m_Main->get_index_size();
    st.indexAlign      = m_Main->get_index_align();
    st.indexFontFamIdx = m_Main->get_index_font_family_idx();
    st.indexFontName   = std::string(m_Main->get_index_font_family_name());
    st.indexFontWtIdx  = m_Main->get_index_font_weight_idx();
    st.indexColorR     = static_cast<uint8_t>(std::clamp(m_Main->get_index_color_r(), 0.0f, 255.0f));
    st.indexColorG     = static_cast<uint8_t>(std::clamp(m_Main->get_index_color_g(), 0.0f, 255.0f));
    st.indexColorB     = static_cast<uint8_t>(std::clamp(m_Main->get_index_color_b(), 0.0f, 255.0f));
    st.indexShadow     = m_Main->get_index_shadow();
    st.indexOutline    = m_Main->get_index_outline();
    st.indexGlow       = m_Main->get_index_glow();
    st.indexBoxBg      = m_Main->get_index_box_bg();
    st.indexUppercase  = m_Main->get_index_uppercase();
    st.textAlign       = m_Main->get_text_align();
    st.textValign      = m_Main->get_text_valign();
    st.colorR          = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_r(), 0.0f, 255.0f));
    st.colorG          = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_g(), 0.0f, 255.0f));
    st.colorB          = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_b(), 0.0f, 255.0f));
    st.fontFamIdx      = m_Main->get_font_family_idx();
    st.fontName        = std::string(m_Main->get_font_family_name());
    st.fontWtIdx       = m_Main->get_font_weight_idx();
    st.shadow          = m_Main->get_text_shadow();
    st.outline         = m_Main->get_text_outline();
    st.glow            = m_Main->get_text_glow();
    st.neon            = m_Main->get_text_neon();
    st.text3d          = m_Main->get_text_3d();
    st.underline       = m_Main->get_text_underline();
    st.uppercase       = m_Main->get_text_uppercase();
    st.boxBg           = m_Main->get_text_box_bg();
    st.textFitMode     = m_Main->get_text_fit_mode();
    return st;
}

void SlintApp::ApplyStyleStructToUI(const SavedTextStyle& st) {
    m_Main->set_text_size(st.textSize);
    m_Main->set_text_margin_h(st.marginH);
    m_Main->set_lyrics_pos_x(st.lyricsPosX);
    m_Main->set_lyrics_pos_y(st.lyricsPosY);
    m_Main->set_lyrics_width(st.lyricsWidth);
    m_Main->set_lyrics_height(st.lyricsHeight);
    m_Main->set_index_box_enabled(st.indexEnabled);
    m_Main->set_index_pos_x(st.indexPosX);
    m_Main->set_index_pos_y(st.indexPosY);
    m_Main->set_index_width(st.indexWidth);
    m_Main->set_index_height(st.indexHeight);
    m_Main->set_index_size(st.indexSize);
    m_Main->set_index_align(st.indexAlign);
    m_Main->set_index_font_family_idx(st.indexFontFamIdx);
    m_Main->set_index_font_family_name(slint::SharedString(
        !st.indexFontName.empty() ? st.indexFontName : ResolveFontPreset(st.indexFontFamIdx)));
    m_Main->set_index_font_weight_idx(st.indexFontWtIdx);
    m_Main->set_index_shadow(st.indexShadow);
    m_Main->set_index_outline(st.indexOutline);
    m_Main->set_index_glow(st.indexGlow);
    m_Main->set_index_box_bg(st.indexBoxBg);
    m_Main->set_index_uppercase(st.indexUppercase);
    m_Main->set_text_align(st.textAlign);
    m_Main->set_text_valign(st.textValign);
    m_Main->set_font_family_idx(st.fontFamIdx);
    m_Main->set_font_family_name(slint::SharedString(
        !st.fontName.empty() ? st.fontName : ResolveFontPreset(st.fontFamIdx)));
    m_Main->set_font_weight_idx(st.fontWtIdx);
    m_Main->set_text_shadow(st.shadow);
    m_Main->set_text_outline(st.outline);
    m_Main->set_text_glow(st.glow);
    m_Main->set_text_neon(st.neon);
    m_Main->set_text_3d(st.text3d);
    m_Main->set_text_underline(st.underline);
    m_Main->set_text_uppercase(st.uppercase);
    m_Main->set_text_box_bg(st.boxBg);
    m_Main->set_text_fit_mode(st.textFitMode);
    SetIndexColorRGB(st.indexColorR, st.indexColorG, st.indexColorB);
    SetTextColorRGB(st.colorR, st.colorG, st.colorB);
}

std::string SlintApp::GetStyleTargetShortLabel(int targetMode) const {
    if (targetMode <= 0) return "Todas las pantallas";
    int scrIdx = targetMode - 1;
    if (scrIdx == 0) return "Público";
    if (scrIdx == 1) return "LAN (8080)";
    if (scrIdx == 2) return "Stage";
    if (scrIdx >= 0 && scrIdx < static_cast<int>(m_Screens.size())) return m_Screens[scrIdx].name;
    return "Todas las pantallas";
}

void SlintApp::SelectStyleTarget(int targetMode) {
    m_StyleTargetMode = std::clamp(targetMode, 0, static_cast<int>(m_Screens.size()));
    m_Main->set_style_target_mode(m_StyleTargetMode);
    m_Main->set_style_target_label(slint::SharedString(GetStyleTargetShortLabel(m_StyleTargetMode)));
    if (m_StyleTargetMode >= 1) {
        int scrIdx = m_StyleTargetMode - 1;
        if (scrIdx < static_cast<int>(m_Screens.size())) {
            m_Main->set_active_saved_style_idx(m_Screens[scrIdx].assignedStyleIdx);
            m_InspectedScreenIdx = scrIdx;
            m_Main->set_inspected_screen_idx(scrIdx);
            SyncInspectedScreenToPreview();
            RefreshActiveLayersModel();
            RefreshScreenMatrixModel();
        }
    } else if (!m_Screens.empty()) {
        int firstIdx = m_Screens[0].assignedStyleIdx;
        bool allLinkedSame = true;
        for (int i = 1; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!m_Screens[i].useOwnStyle && m_Screens[i].assignedStyleIdx != firstIdx) {
                allLinkedSame = false;
                break;
            }
        }
        m_Main->set_active_saved_style_idx(allLinkedSame ? firstIdx : -1);
    }
    RefreshSavedStylesModel();
}

void SlintApp::SetScreenOwnStyle(int screenIdx, bool enabled) {
    if (screenIdx <= 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    m_Screens[screenIdx].useOwnStyle = enabled;
    if (!enabled && !m_Screens.empty()) {
        ApplyStyleToScreen(screenIdx, m_Screens[0].textStyle, m_Screens[0].assignedStyleIdx);
    }
    SyncScreenOutputs();
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::ApplyStyleToScreen(int screenIdx, const SavedTextStyle& st, int styleIdx) {
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    auto& sc = m_Screens[screenIdx];
    sc.textStyle        = st;
    sc.assignedStyleIdx = styleIdx;
    if (!sc.rawText.empty()) {
        sc.displayText = sc.textStyle.uppercase ? ToUpperUtf8(sc.rawText) : sc.rawText;
    }
    if (!sc.rawRef.empty()) {
        sc.displayRef = (sc.isBible && sc.textStyle.indexUppercase) ? ToUpperUtf8(sc.rawRef) : sc.rawRef;
    }
}

void SlintApp::ApplyCustomStyleToScreen(int styleIdx, int screenIdx) {
    if (styleIdx < 0 || styleIdx >= static_cast<int>(m_SavedStyles.size())) return;
    const auto& st = m_SavedStyles[styleIdx];
    if (screenIdx <= 0) {
        // Público (0) o Todas (<0): actualiza Público y todas las pantallas enlazadas (!useOwnStyle),
        // protegiendo las pantallas con estilo propio independiente (ej. LAN con letras gigantes).
        if (!m_Screens.empty()) {
            ApplyStyleToScreen(0, st, styleIdx);
        }
        for (int i = 1; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!m_Screens[i].useOwnStyle) {
                ApplyStyleToScreen(i, st, styleIdx);
            }
        }
    } else if (screenIdx < static_cast<int>(m_Screens.size())) {
        // Pantalla secundaria específica (LAN, Stage, Sala Extra): activa estilo independiente
        // automáticamente y NO afecta a Público ni a otras pantallas.
        m_Screens[screenIdx].useOwnStyle = true;
        ApplyStyleToScreen(screenIdx, st, styleIdx);
    } else {
        return;
    }

    if (screenIdx <= 0) {
        ApplyStyleStructToUI(st);
        m_Main->set_active_saved_style_idx(styleIdx);
    } else if (m_StyleTargetMode >= 1 && (m_StyleTargetMode - 1) == screenIdx) {
        m_Main->set_active_saved_style_idx(styleIdx);
    }
    if (screenIdx >= 0 && screenIdx < static_cast<int>(m_Screens.size())) {
        m_InspectedScreenIdx = screenIdx;
        m_Main->set_inspected_screen_idx(screenIdx);
    }
    SyncScreenOutputs();
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::OpenNewStyleEditor() {
    m_StyleBeforeEdit = CaptureCurrentStyleFromUI("");
    m_EditingStyleIdx = -1;
    int seedIdx = (m_StyleTargetMode >= 1 && (m_StyleTargetMode - 1) < static_cast<int>(m_Screens.size()))
        ? (m_StyleTargetMode - 1)
        : 0;
    if (seedIdx >= 0 && seedIdx < static_cast<int>(m_Screens.size())) {
        ApplyStyleStructToUI(m_Screens[seedIdx].textStyle);
    }
    m_Main->set_editing_style_name("Mi Estilo " + slint::SharedString(std::to_string(m_SavedStyles.size() + 1)));
    m_Main->set_style_editor_box(0);
    m_Main->set_style_editor_open(true);
    m_OverlayEditorOpen = false;
    m_Main->set_overlay_editor_open(false);
}

void SlintApp::OpenEditStyleEditor(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    m_StyleBeforeEdit = CaptureCurrentStyleFromUI("");
    m_EditingStyleIdx = idx;
    ApplyStyleStructToUI(m_SavedStyles[idx]);
    m_Main->set_editing_style_name(slint::SharedString(m_SavedStyles[idx].name));
    m_Main->set_style_editor_box(0);
    m_Main->set_style_editor_open(true);
    m_OverlayEditorOpen = false;
    m_Main->set_overlay_editor_open(false);
}

void SlintApp::SaveStyleEditor(const std::string& rawName) {
    std::string cleanName = rawName;
    while (!cleanName.empty() && cleanName.front() == ' ') cleanName.erase(cleanName.begin());
    while (!cleanName.empty() && cleanName.back() == ' ')  cleanName.pop_back();
    if (cleanName.empty()) cleanName = "Estilo " + std::to_string(m_SavedStyles.size() + 1);

    SavedTextStyle st = CaptureCurrentStyleFromUI(cleanName);
    int savedIdx = -1;

    if (m_EditingStyleIdx >= 0 && m_EditingStyleIdx < static_cast<int>(m_SavedStyles.size())) {
        // Delete old .theme file if renamed
        if (m_SavedStyles[m_EditingStyleIdx].name != cleanName) {
            std::error_code ec;
            fs::remove(fs::path(ProyecThor::GetAppDataRoot()) / "themes" / (SanitizeFilename(m_SavedStyles[m_EditingStyleIdx].name) + ".theme"), ec);
        }
        m_SavedStyles[m_EditingStyleIdx] = st;
        savedIdx = m_EditingStyleIdx;
    } else {
        for (int i = 0; i < static_cast<int>(m_SavedStyles.size()); ++i) {
            if (m_SavedStyles[i].name == cleanName) {
                m_SavedStyles[i] = st;
                savedIdx = i;
                break;
            }
        }
        if (savedIdx < 0) {
            m_SavedStyles.push_back(st);
            savedIdx = static_cast<int>(m_SavedStyles.size()) - 1;
        }
    }

    SaveThemeFileToDisk(st);
    m_Main->set_style_editor_open(false);
    m_Main->set_active_saved_style_idx(savedIdx);

    // Apply saved style to targeted screen(s) AND refresh any screen already using this edited style
    if (m_StyleTargetMode <= 1) {
        if (!m_Screens.empty()) {
            ApplyStyleToScreen(0, st, savedIdx);
        }
        for (int i = 1; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!m_Screens[i].useOwnStyle || (m_EditingStyleIdx >= 0 && m_Screens[i].assignedStyleIdx == savedIdx)) {
                ApplyStyleToScreen(i, st, savedIdx);
            }
        }
    } else {
        int targetScr = m_StyleTargetMode - 1;
        if (targetScr >= 1 && targetScr < static_cast<int>(m_Screens.size())) {
            m_Screens[targetScr].useOwnStyle = true;
        }
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (i == targetScr || (m_EditingStyleIdx >= 0 && m_Screens[i].assignedStyleIdx == savedIdx)) {
                ApplyStyleToScreen(i, st, savedIdx);
            }
        }
    }

    SyncScreenOutputs();
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::CancelStyleEditor() {
    ApplyStyleStructToUI(m_StyleBeforeEdit);
    m_Main->set_style_editor_open(false);
    SyncScreenOutputs();
}

void SlintApp::DuplicateCustomStyle(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    SavedTextStyle copy = m_SavedStyles[idx];
    copy.name += " (Copia)";
    m_SavedStyles.push_back(copy);
    SaveThemeFileToDisk(copy);
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::LoadCustomStyle(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    m_Main->set_active_saved_style_idx(idx);
    int screenTarget = (m_StyleTargetMode <= 0) ? -1 : (m_StyleTargetMode - 1);
    ApplyCustomStyleToScreen(idx, screenTarget);
}

void SlintApp::DeleteCustomStyle(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    std::error_code ec;
    fs::remove(fs::path(ProyecThor::GetAppDataRoot()) / "themes" / (SanitizeFilename(m_SavedStyles[idx].name) + ".theme"), ec);
    m_SavedStyles.erase(m_SavedStyles.begin() + idx);
    if (m_Main->get_active_saved_style_idx() == idx) m_Main->set_active_saved_style_idx(-1);
    else if (m_Main->get_active_saved_style_idx() > idx) m_Main->set_active_saved_style_idx(m_Main->get_active_saved_style_idx() - 1);
    for (auto& sc : m_Screens) {
        if (sc.assignedStyleIdx == idx) sc.assignedStyleIdx = -1;
        else if (sc.assignedStyleIdx > idx) sc.assignedStyleIdx -= 1;
    }
    SyncLanStyleProperties();
    RefreshSavedStylesModel();
    RefreshScreenMatrixModel();
    SavePersistedState();
}

void SlintApp::RefreshSavedStylesModel() {
    m_SavedStyleCardsModel->clear();
    static const char* kFamNames[] = { "Default", "Sans", "Serif", "Mono" };
    static const int kFontWeights[] = { 400, 700, 900 };

    auto shortScreenName = [](int i, const ScreenOutputState& sc) -> std::string {
        if (i == 0) return "Público";
        if (i == 1) return "LAN";
        if (i == 2) return "Stage";
        return sc.name;
    };

    for (int sidx = 0; sidx < static_cast<int>(m_SavedStyles.size()); ++sidx) {
        const auto& st = m_SavedStyles[sidx];
        StyleCardItem card;
        card.name             = slint::SharedString(st.name);
        card.text_color       = slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB);
        card.box_bg           = st.boxBg;
        card.font_family_name = slint::SharedString(ResolveStyleFontName(st, false));
        card.font_weight_val  = kFontWeights[std::clamp(st.fontWtIdx, 0, 2)];

        std::string fontLabel = !st.fontName.empty()
            ? st.fontName
            : kFamNames[std::clamp(st.fontFamIdx, 0, 3)];
        std::string sum = std::to_string(static_cast<int>(st.textSize)) + "px • " + fontLabel;
        if (st.shadow)    sum += " • Sombra";
        if (st.outline)   sum += " • Contorno";
        if (st.glow)      sum += " • Glow";
        if (st.neon)      sum += " • Neón";
        if (st.text3d)    sum += " • 3D";
        if (st.boxBg)     sum += " • Caja";
        if (st.indexEnabled) sum += " • +Índice";
        card.summary = slint::SharedString(sum);

        auto targetsModel = std::make_shared<slint::VectorModel<StyleScreenTarget>>();
        std::string assignedLbl;
        bool activeOnTarget = false;
        bool allLinkedUsing = !m_Screens.empty();
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            bool isAssigned = (m_Screens[i].assignedStyleIdx == sidx);
            if ((i == 0 || !m_Screens[i].useOwnStyle) && !isAssigned) allLinkedUsing = false;
            if (isAssigned) {
                if (!assignedLbl.empty()) assignedLbl += ", ";
                assignedLbl += shortScreenName(i, m_Screens[i]);
            }
            if (m_StyleTargetMode == i + 1 && isAssigned) {
                activeOnTarget = true;
            }
            StyleScreenTarget tgt;
            tgt.screen_id = i;
            tgt.label     = slint::SharedString(shortScreenName(i, m_Screens[i]));
            tgt.active    = isAssigned;
            targetsModel->push_back(tgt);
        }
        if (m_StyleTargetMode <= 0 && allLinkedUsing) activeOnTarget = true;
        card.active_on_target       = activeOnTarget;
        card.assigned_screens_label = slint::SharedString(assignedLbl);
        card.screen_targets         = targetsModel;

        m_SavedStyleCardsModel->push_back(card);
    }
}

void SlintApp::SaveThemeFileToDisk(const SavedTextStyle& st) {
    fs::path dir = fs::path(ProyecThor::GetAppDataRoot()) / "themes";
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::ofstream f(dir / (SanitizeFilename(st.name) + ".theme"), std::ios::trunc);
    if (!f.is_open()) return;

    f << "lyricsPosX="  << st.lyricsPosX  << "\n";
    f << "lyricsPosY="  << st.lyricsPosY  << "\n";
    f << "lyricsSizeW=" << st.lyricsWidth << "\n";
    f << "lyricsSizeH=" << st.lyricsHeight << "\n";
    f << "lyricsSize="  << st.textSize    << "\n";
    f << "lyricsHAlign="<< st.textAlign   << "\n";
    f << "lyricsVAlign="<< st.textValign  << "\n";
    f << "lyricsColor=" << (st.colorR / 255.0f) << "," << (st.colorG / 255.0f) << "," << (st.colorB / 255.0f) << ",1\n";
    f << "indexEnabled="   << (st.indexEnabled ? 1 : 0) << "\n";
    f << "indexPosX="      << st.indexPosX   << "\n";
    f << "indexPosY="      << st.indexPosY   << "\n";
    f << "indexWidth="     << st.indexWidth  << "\n";
    f << "indexSizeW="     << st.indexWidth  << "\n";
    f << "indexSizeH="     << st.indexHeight << "\n";
    f << "indexSize="      << st.indexSize   << "\n";
    f << "indexAlign="     << st.indexAlign  << "\n";
    f << "indexFontFam="   << st.indexFontFamIdx << "\n";
    f << "indexFontWt="    << st.indexFontWtIdx  << "\n";
    f << "indexColor="     << (st.indexColorR / 255.0f) << "," << (st.indexColorG / 255.0f) << "," << (st.indexColorB / 255.0f) << ",1\n";
    f << "indexShadow="    << (st.indexShadow ? 1 : 0) << "\n";
    f << "indexOutline="   << (st.indexOutline ? 1 : 0) << "\n";
    f << "indexGlow="      << (st.indexGlow ? 1 : 0) << "\n";
    f << "indexBoxBg="     << (st.indexBoxBg ? 1 : 0) << "\n";
    f << "indexUpper="     << (st.indexUppercase ? 1 : 0) << "\n";
    f << "lyricsFont="  << st.fontName    << "\n";
    f << "indexFont="   << st.indexFontName << "\n";
    f << "slintFontFam="<< st.fontFamIdx  << "\n";
    f << "slintFontWt=" << st.fontWtIdx   << "\n";
    f << "slintShadow=" << (st.shadow ? 1 : 0) << "\n";
    f << "slintOutline="<< (st.outline ? 1 : 0) << "\n";
    f << "slintGlow="   << (st.glow ? 1 : 0) << "\n";
    f << "slintNeon="   << (st.neon ? 1 : 0) << "\n";
    f << "slint3D="     << (st.text3d ? 1 : 0) << "\n";
    f << "slintUnder="  << (st.underline ? 1 : 0) << "\n";
    f << "slintUpper="  << (st.uppercase ? 1 : 0) << "\n";
    f << "slintBoxBg="  << (st.boxBg ? 1 : 0) << "\n";
    f << "slintTextFit="<< st.textFitMode << "\n";
}

void SlintApp::LoadThemeFilesFromDisk() {
    fs::path dir = fs::path(ProyecThor::GetAppDataRoot()) / "themes";
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".theme") continue;
        std::string themeName = PathToUtf8(entry.path().stem());
        bool alreadyLoaded = false;
        for (const auto& s : m_SavedStyles) {
            if (s.name == themeName) { alreadyLoaded = true; break; }
        }
        if (alreadyLoaded) continue;

        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        SavedTextStyle st;
        st.name = themeName;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            try {
                if (k == "lyricsPosX")        st.lyricsPosX   = std::stof(v);
                else if (k == "lyricsPosY")   st.lyricsPosY   = std::stof(v);
                else if (k == "lyricsSizeW")  st.lyricsWidth  = std::stof(v);
                else if (k == "lyricsSizeH")  st.lyricsHeight = std::stof(v);
                else if (k == "lyricsSize" || k == "size") st.textSize = std::stof(v);
                else if (k == "lyricsHAlign") st.textAlign    = std::stoi(v);
                else if (k == "lyricsVAlign") st.textValign   = std::stoi(v);
                else if (k == "indexEnabled") st.indexEnabled    = (std::stoi(v) != 0);
                else if (k == "indexPosX")    st.indexPosX       = std::stof(v);
                else if (k == "indexPosY")    st.indexPosY       = std::stof(v);
                else if (k == "indexWidth" || k == "indexSizeW") st.indexWidth = std::stof(v);
                else if (k == "indexHeight" || k == "indexSizeH") st.indexHeight = std::stof(v);
                else if (k == "indexSize")    st.indexSize       = std::stof(v);
                else if (k == "indexAlign")   st.indexAlign      = std::stoi(v);
                else if (k == "indexFontFam") st.indexFontFamIdx = std::stoi(v);
                else if (k == "indexFontWt")  st.indexFontWtIdx  = std::stoi(v);
                else if (k == "indexShadow")  st.indexShadow     = (std::stoi(v) != 0);
                else if (k == "indexOutline") st.indexOutline    = (std::stoi(v) != 0);
                else if (k == "indexGlow")    st.indexGlow       = (std::stoi(v) != 0);
                else if (k == "indexBoxBg")   st.indexBoxBg      = (std::stoi(v) != 0);
                else if (k == "indexUpper")   st.indexUppercase  = (std::stoi(v) != 0);
                else if (k == "indexColor") {
                    std::istringstream ss(v);
                    std::string tok;
                    float rgb[3] = {0.98f, 0.75f, 0.14f};
                    for (int c = 0; c < 3 && std::getline(ss, tok, ','); ++c) rgb[c] = std::stof(tok);
                    st.indexColorR = static_cast<uint8_t>(std::clamp(rgb[0] * 255.0f, 0.0f, 255.0f));
                    st.indexColorG = static_cast<uint8_t>(std::clamp(rgb[1] * 255.0f, 0.0f, 255.0f));
                    st.indexColorB = static_cast<uint8_t>(std::clamp(rgb[2] * 255.0f, 0.0f, 255.0f));
                }
                else if (k == "lyricsFont" || k == "font") st.fontName = v;
                else if (k == "indexFont")    st.indexFontName = v;
                else if (k == "slintFontFam") st.fontFamIdx   = std::stoi(v);
                else if (k == "slintFontWt")  st.fontWtIdx    = std::stoi(v);
                else if (k == "slintShadow")  st.shadow       = (std::stoi(v) != 0);
                else if (k == "slintOutline") st.outline      = (std::stoi(v) != 0);
                else if (k == "slintGlow")    st.glow         = (std::stoi(v) != 0);
                else if (k == "slintNeon")    st.neon         = (std::stoi(v) != 0);
                else if (k == "slint3D")      st.text3d       = (std::stoi(v) != 0);
                else if (k == "slintUnder")   st.underline    = (std::stoi(v) != 0);
                else if (k == "slintUpper")   st.uppercase    = (std::stoi(v) != 0);
                else if (k == "slintBoxBg")   st.boxBg        = (std::stoi(v) != 0);
                else if (k == "slintTextFit") st.textFitMode  = std::clamp(std::stoi(v), 0, 2);
                else if (k == "lyricsColor" || k == "color") {
                    std::istringstream ss(v);
                    std::string tok;
                    float rgb[3] = {1, 1, 1};
                    for (int c = 0; c < 3 && std::getline(ss, tok, ','); ++c) rgb[c] = std::stof(tok);
                    st.colorR = static_cast<uint8_t>(std::clamp(rgb[0] * 255.0f, 0.0f, 255.0f));
                    st.colorG = static_cast<uint8_t>(std::clamp(rgb[1] * 255.0f, 0.0f, 255.0f));
                    st.colorB = static_cast<uint8_t>(std::clamp(rgb[2] * 255.0f, 0.0f, 255.0f));
                }
            } catch (...) {}
        }
        m_SavedStyles.push_back(st);
    }
}

void SlintApp::ApplyShaders() {
    int br     = static_cast<int>(m_Main->get_shader_brightness());
    int ct     = static_cast<int>(m_Main->get_shader_contrast());
    int sat    = static_cast<int>(m_Main->get_shader_saturation());
    int gm     = static_cast<int>(m_Main->get_shader_gamma());
    bool blur  = m_Main->get_shader_blur();
    int preset = m_Main->get_shader_preset();
    bool fsr   = m_Main->get_fsr_enabled();

    m_Player.SetVideoFilters(br, ct, sat, gm, blur, preset, fsr);
    if (!m_AudienceFrozen) {
        m_Projector->set_vignette_enabled(m_Main->get_shader_vignette());
        m_Projector->set_shader_brightness(static_cast<float>(br));
        m_Projector->set_shader_preset(preset);
        m_Projector->set_bg_is_static_image(m_HasStaticImage);
    }
    m_Main->set_bg_is_static_image(m_HasStaticImage);
}

void SlintApp::ResetShaders() {
    m_Main->set_shader_preset(0);
    m_Main->set_shader_brightness(0.0f);
    m_Main->set_shader_contrast(0.0f);
    m_Main->set_shader_saturation(0.0f);
    m_Main->set_shader_gamma(0.0f);
    m_Main->set_shader_blur(false);
    m_Main->set_shader_vignette(false);
    ApplyShaders();
}

void SlintApp::ApplyEqualizer() {
    m_Player.SetAudioEqualizer(
        m_Main->get_eq_enabled(),
        m_Main->get_eq_bass(),
        m_Main->get_eq_mid(),
        m_Main->get_eq_treble());
}

void SlintApp::ApplyEqPreset(int presetIdx) {
    m_Main->set_eq_enabled(true);
    if (presetIdx == 0) { // Plano
        m_Main->set_eq_bass(0.0f); m_Main->set_eq_mid(0.0f); m_Main->set_eq_treble(0.0f);
    } else if (presetIdx == 1) { // Voz / Predicación
        m_Main->set_eq_bass(-3.0f); m_Main->set_eq_mid(4.0f); m_Main->set_eq_treble(2.0f);
    } else if (presetIdx == 2) { // Alabanza
        m_Main->set_eq_bass(5.0f); m_Main->set_eq_mid(1.0f); m_Main->set_eq_treble(4.0f);
    } else if (presetIdx == 3) { // Acústico
        m_Main->set_eq_bass(2.0f); m_Main->set_eq_mid(2.0f); m_Main->set_eq_treble(3.0f);
    }
    ApplyEqualizer();
}

void SlintApp::TriggerSlideTransition() {
    if (m_AudienceFrozen) return;
    int type = m_Main->get_transition_type();
    if (type == 0) {
        m_Projector->set_transition_visible(true);
        return;
    }
    m_Projector->set_transition_dur(0);
    m_Projector->set_transition_visible(false);

    slint::ComponentWeakHandle<ProjectorWindow> weakProj(m_Projector);
    int64_t durMs = static_cast<int64_t>(m_Main->get_transition_duration_ms());
    slint::Timer::single_shot(std::chrono::milliseconds(25), [weakProj, durMs]() {
        if (auto locked = weakProj.lock()) {
            (*locked)->set_transition_dur(durMs);
            (*locked)->set_transition_visible(true);
        }
    });
}

void SlintApp::SelectTransitionPreset(int presetIdx) {
    struct PresetDef { int type; float durMs; };
    static const PresetDef kPresets[6] = {
        { 0, 100.0f }, // 0: Corte Directo
        { 1, 350.0f }, // 1: Fundido Suave (Default)
        { 1, 900.0f }, // 2: Fundido Cinematográfico
        { 2, 400.0f }, // 3: Deslizar Izquierda
        { 3, 420.0f }, // 4: Deslizar Arriba
        { 4, 400.0f }  // 5: Deslizar Derecha
    };
    int idx = std::clamp(presetIdx, 0, 5);
    m_Main->set_active_transition_preset(idx);
    m_Main->set_transition_type(kPresets[idx].type);
    m_Main->set_transition_duration_ms(kPresets[idx].durMs);
    SyncProjectorStyle();
    SavePersistedState();
    ReplayTransitionPreview();
}

void SlintApp::ReplayTransitionPreview() {
    int type = m_Main->get_transition_type();
    if (type == 0) {
        m_Main->set_trans_preview_dur(0);
        m_Main->set_trans_preview_visible(true);
        return;
    }
    m_Main->set_trans_preview_dur(0);
    m_Main->set_trans_preview_visible(false);

    slint::ComponentWeakHandle<MainWindow> weakMain(m_Main);
    int64_t durMs = static_cast<int64_t>(m_Main->get_transition_duration_ms());
    slint::Timer::single_shot(std::chrono::milliseconds(25), [weakMain, durMs]() {
        if (auto locked = weakMain.lock()) {
            (*locked)->set_trans_preview_dur(durMs);
            (*locked)->set_trans_preview_visible(true);
        }
    });
}

void SlintApp::SyncTilesState() {
    bool enabled = m_Main->get_tiles_enabled();
    m_Projector->set_tiles_enabled(enabled);
    m_Projector->set_tiles_layout(m_Main->get_tiles_layout());
    m_Projector->set_tiles_show_divider(m_Main->get_tiles_divider());

    if (!enabled) return;

    int cur   = m_Main->get_active_slide_index();
    int count = static_cast<int>(m_SlidesModel->row_count());
    int cont  = m_Main->get_tiles_continuity(); // 0=Next, 1=Prev
    int idx2  = (cont == 0) ? (cur + 1) : (cur - 1);

    if (cur >= 0 && idx2 >= 0 && idx2 < count) {
        if (auto s2 = m_SlidesModel->row_data(idx2)) {
            std::string body = std::string(s2->body);
            bool upper = !m_Screens.empty() ? m_Screens[0].textStyle.uppercase : m_Main->get_text_uppercase();
            if (upper) body = ToUpperUtf8(body);
            m_Projector->set_tile2_text(slint::SharedString(body));
            m_Projector->set_tile2_ref(s2->ref_text);
            return;
        }
    }
    m_Projector->set_tile2_text("");
    m_Projector->set_tile2_ref("");
}

std::string SlintApp::ToUpperUtf8(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

void SlintApp::SetTextColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    m_Main->set_text_color_r(static_cast<float>(r));
    m_Main->set_text_color_g(static_cast<float>(g));
    m_Main->set_text_color_b(static_cast<float>(b));
    m_Main->set_text_color(slint::Color::from_rgb_uint8(r, g, b));
    SyncProjectorStyle();
}

void SlintApp::SelectTextColor(int colorIdx) {
    m_Main->set_text_color_idx(colorIdx);
    switch (colorIdx) {
        case 1:  SetTextColorRGB(254, 243, 199); break; // Crema
        case 2:  SetTextColorRGB(212, 212, 216); break; // Gris claro
        case 3:  SetTextColorRGB(245, 196, 81);  break; // Dorado
        case 4:  SetTextColorRGB(250, 204, 21);  break; // Amarillo
        case 5:  SetTextColorRGB(56, 189, 248);  break; // Celeste
        case 6:  SetTextColorRGB(59, 130, 246);  break; // Azul
        case 7:  SetTextColorRGB(74, 222, 128);  break; // Verde
        case 8:  SetTextColorRGB(248, 113, 113); break; // Rojo coral
        case 9:  SetTextColorRGB(236, 72, 153);  break; // Rosa
        case 10: SetTextColorRGB(192, 132, 252); break; // Violeta
        case 11: SetTextColorRGB(251, 146, 60);  break; // Naranja
        case 12: SetTextColorRGB(45, 212, 191);  break; // Turquesa
        case 13: SetTextColorRGB(9, 9, 11);      break; // Negro
        default: SetTextColorRGB(255, 255, 255); break; // Blanco
    }
}

void SlintApp::SetIndexColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    m_Main->set_index_color_r(static_cast<float>(r));
    m_Main->set_index_color_g(static_cast<float>(g));
    m_Main->set_index_color_b(static_cast<float>(b));
    m_Main->set_index_color(slint::Color::from_rgb_uint8(r, g, b));
    SyncProjectorStyle();
}

void SlintApp::SelectIndexColor(int colorIdx) {
    switch (colorIdx) {
        case 1: SetIndexColorRGB(251, 191, 36);  break; // Dorado
        case 2: SetIndexColorRGB(56, 189, 248);  break; // Celeste
        case 3: SetIndexColorRGB(212, 212, 216); break; // Plata
        default: SetIndexColorRGB(255, 255, 255); break; // Blanco
    }
}

void SlintApp::SyncAlertConfig() {
    m_Projector->set_alert_anim_mode(m_Main->get_alert_anim_mode());
    m_Projector->set_alert_position(m_Main->get_alert_position());
    m_Projector->set_alert_theme(m_Main->get_alert_theme());
    m_Projector->set_alert_ticker_pos(m_AlertTickerPos);
    m_Projector->set_alert_pulse_alpha(1.0f);
    m_Main->set_alert_ticker_pos(m_AlertTickerPos);
    m_Main->set_alert_pulse_alpha(1.0f);
}

slint::Color SlintApp::TextColorFromIndex(int idx) {
    switch (idx) {
        case 1: return slint::Color::from_rgb_uint8(254, 243, 199); // Crema
        case 2: return slint::Color::from_rgb_uint8(212, 212, 216); // Plata
        case 3: return slint::Color::from_rgb_uint8(245, 196, 81);  // Dorado
        case 4: return slint::Color::from_rgb_uint8(250, 204, 21);  // Amarillo
        default: return slint::Color::from_rgb_uint8(255, 255, 255); // Blanco
    }
}

void SlintApp::ToggleAudienceWindow() {
    m_ProjectorVisible = !m_ProjectorVisible;
    m_Main->set_audience_live(m_ProjectorVisible);
    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapProjecting = m_ProjectorVisible;
        ++m_StreamVersion;
    }
    if (m_ProjectorVisible) {
        UpdatePlayerRenderSize();
        SyncProjectorStyle();
        m_Player.SetScaleMode(m_BgScaleMode);
        m_Projector->set_bg_scale_mode(m_BgScaleMode);
        m_Projector->show();
        const bool wantFs = m_Main->get_fullscreen_output();
        // Coloca la ventana en el monitor elegido (por defecto el secundario)
        // antes de ir a pantalla completa, para que no se abra en la pantalla
        // equivocada.
        PositionProjectorOnSelectedMonitor();
        m_Projector->window().set_fullscreen(wantFs);
        m_Projector->invoke_focus_window();
        // Wayland / Hyprland: re-apply fullscreen & focus once xdg_toplevel surface maps
        slint::ComponentWeakHandle<ProjectorWindow> weakProj(m_Projector);
        slint::Timer::single_shot(std::chrono::milliseconds(60), [this, weakProj, wantFs]() {
            if (auto locked = weakProj.lock()) {
                if (wantFs) {
                    PositionProjectorOnSelectedMonitor();
                    (*locked)->window().set_fullscreen(true);
                }
                (*locked)->invoke_focus_window();
            }
        });
    } else {
        m_Projector->window().set_fullscreen(false);
        m_Projector->hide();
        UpdatePlayerRenderSize();
    }
}

void SlintApp::GetOutputResolution(int& outW, int& outH) const {
    switch (m_OutputResPreset) {
        case 0:  outW = 1280; outH = 720;  break; // 720p (16:9)
        case 2:  outW = 1440; outH = 900;  break; // 16:10
        case 3:  outW = 1280; outH = 960;  break; // 4:3
        case 4:  outW = 3840; outH = 2160; break; // 2160p (4K UHD)
        case 1:
        default: outW = 1920; outH = 1080; break; // 1080p (16:9)
    }
}

bool SlintApp::RenderScaledStaticImage(const std::string& path, slint::Image& outImg) {
    if (path.empty()) return false;

    // Reuse cached decoded source pixels when re-scaling the same image (instant Cover/Fill/Contain switch!)
    if (m_StaticSrcPath != path || m_StaticSrcRgba.empty() || m_StaticSrcW <= 0 || m_StaticSrcH <= 0) {
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!data || w <= 0 || h <= 0) {
            if (data) stbi_image_free(data);
            outImg = slint::Image::load_from_path(slint::SharedString(path));
            return true;
        }
        m_StaticSrcPath = path;
        const bool perf = m_Main->get_performance_mode();
        const int maxCacheW = perf ? 1280 : 3840;
        const int maxCacheH = perf ? 720  : 2160;
        if (w > maxCacheW || h > maxCacheH) {
            // Downscale oversized source (e.g. 6000x4000 96MB photo -> 3.5MB in Low-Spec mode) to protect 4GB RAM!
            double sc = std::min(static_cast<double>(maxCacheW) / w, static_cast<double>(maxCacheH) / h);
            int cw = std::max(1, static_cast<int>(std::round(w * sc)));
            int ch = std::max(1, static_cast<int>(std::round(h * sc)));
            m_StaticSrcRgba.resize(static_cast<size_t>(cw) * static_cast<size_t>(ch) * 4);
            stbir_resize_uint8_srgb(data, w, h, w * 4, m_StaticSrcRgba.data(), cw, ch, cw * 4, STBIR_RGBA);
            stbi_image_free(data);
            m_StaticSrcW = cw;
            m_StaticSrcH = ch;
        } else {
            m_StaticSrcW = w;
            m_StaticSrcH = h;
            m_StaticSrcRgba.assign(data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
            stbi_image_free(data);
        }
    }

    int outW = 1920, outH = 1080;
    GetOutputResolution(outW, outH);

    slint::SharedPixelBuffer<slint::Rgba8Pixel> dstBuf(
        static_cast<uint32_t>(outW), static_cast<uint32_t>(outH));
    auto* dst = reinterpret_cast<uint8_t*>(dstBuf.begin());

    // Clear output frame to solid black (opaque alpha=255) for clean letterbox/pillarbox in Contain mode
    const size_t totalPx = static_cast<size_t>(outW) * static_cast<size_t>(outH);
    for (size_t i = 0; i < totalPx; ++i) {
        dst[i * 4 + 0] = 0;
        dst[i * 4 + 1] = 0;
        dst[i * 4 + 2] = 0;
        dst[i * 4 + 3] = 255;
    }

    const int srcW = m_StaticSrcW;
    const int srcH = m_StaticSrcH;
    const uint8_t* src = m_StaticSrcRgba.data();

    if (m_BgScaleMode == 1) {
        // 1 = Estirar (Fill — stretches entire source to exact outW x outH)
        stbir_resize_uint8_srgb(
            src, srcW, srcH, srcW * 4,
            dst, outW, outH, outW * 4,
            STBIR_RGBA);
    } else if (m_BgScaleMode == 2) {
        // 2 = Original (Contain — fits entire source inside outW x outH preserving exact aspect)
        double scale = std::min(static_cast<double>(outW) / srcW, static_cast<double>(outH) / srcH);
        int fitW = std::clamp(static_cast<int>(std::round(srcW * scale)), 1, outW);
        int fitH = std::clamp(static_cast<int>(std::round(srcH * scale)), 1, outH);
        int offX = (outW - fitW) / 2;
        int offY = (outH - fitH) / 2;
        uint8_t* dstSub = dst + (static_cast<size_t>(offY) * outW + offX) * 4;
        stbir_resize_uint8_srgb(
            src, srcW, srcH, srcW * 4,
            dstSub, fitW, fitH, outW * 4,
            STBIR_RGBA);
    } else {
        // 0 = Recortar / Llenar (Cover — center-crops source to match output aspect, fills 100%)
        double srcAr = static_cast<double>(srcW) / static_cast<double>(srcH);
        double dstAr = static_cast<double>(outW) / static_cast<double>(outH);
        int cropX = 0, cropY = 0, cropW = srcW, cropH = srcH;
        if (srcAr > dstAr) {
            cropW = std::clamp(static_cast<int>(std::round(srcH * dstAr)), 1, srcW);
            cropX = (srcW - cropW) / 2;
        } else if (srcAr < dstAr) {
            cropH = std::clamp(static_cast<int>(std::round(srcW / dstAr)), 1, srcH);
            cropY = (srcH - cropH) / 2;
        }
        const uint8_t* srcSub = src + (static_cast<size_t>(cropY) * srcW + cropX) * 4;
        stbir_resize_uint8_srgb(
            srcSub, cropW, cropH, srcW * 4,
            dst, outW, outH, outW * 4,
            STBIR_RGBA);
    }

    outImg = slint::Image(dstBuf);
    return true;
}

void SlintApp::ReapplyCurrentStaticImage() {
    if (!m_HasStaticImage || m_CurrentBgPath.empty()) return;
    slint::Image img;
    if (RenderScaledStaticImage(m_CurrentBgPath, img)) {
        m_Main->set_live_preview_image(img);
        m_Main->set_has_live_preview_image(true);
        if (!m_AudienceFrozen) {
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
        }
        if (IsLanRunning()) UpdateLanStaticFrame(img);
    }
}

void SlintApp::UpdatePlayerRenderSize() {
    const bool perf = m_Main->get_performance_mode();
    const bool lowCoreCpu = (std::thread::hardware_concurrency() <= 2);
    const bool lanNeedsVideo = IsLanRunning() && !m_LanConfig.transparentMode && m_LanConfig.sendBackground;
    int outW = 1920, outH = 1080;
    GetOutputResolution(outW, outH);

    if (m_ProjectorVisible) {
        int maxW = (perf || lowCoreCpu) ? (lowCoreCpu ? 854 : 960) : outW;
        int w = std::min(outW, maxW);
        int h = std::max(360, static_cast<int>(std::round(static_cast<double>(w) * outH / std::max(1, outW))));
        w &= ~1; h &= ~1;
        m_Player.SetRenderSize(w, h);
    } else if (lanNeedsVideo) {
        int w = std::clamp(m_LanConfig.frameWidth, 640, (perf || lowCoreCpu) ? 960 : 1280);
        int h = std::clamp(m_LanConfig.frameHeight, 360, (perf || lowCoreCpu) ? 540 : 720);
        m_Player.SetRenderSize(w, h);
    } else {
        int w = (perf || lowCoreCpu) ? 480 : 640;
        int h = std::max(270, static_cast<int>(std::round(static_cast<double>(w) * outH / std::max(1, outW))));
        w &= ~1; h &= ~1;
        m_Player.SetRenderSize(w, h);
    }
}

void SlintApp::ApplyOutputResolution(int preset) {
    m_OutputResPreset = std::clamp(preset, 0, 4);
    m_Main->set_output_res_preset(m_OutputResPreset);
    UpdatePlayerRenderSize();
    ReapplyCurrentStaticImage();
    SavePersistedState();
}

void SlintApp::ApplyUiLanguage(const std::string& code) {
    // Solo idiomas con traducción empaquetada. "es" es el idioma fuente
    // (los msgid de @tr están en español), así que seleccionar "es" vuelve
    // a las cadenas originales. Cualquier código desconocido cae a "es".
    std::string lang = (code == "en") ? "en" : "es";
    m_UiLanguage = lang;
    m_Main->set_ui_language(slint::SharedString(lang));
    // Para el idioma fuente pasamos "" (sin bundle) -> usa los msgid tal cual.
    slint::select_bundled_translation(lang == "es" ? "" : lang);
    AppendDebugLog("Idioma de interfaz: " + lang);
}

void SlintApp::ApplyTargetFps(int fps) {
    if (fps != 30 && fps != 60 && fps != 120) fps = 60;
    m_TargetFps = fps;
    m_Main->set_target_fps(m_TargetFps);
    m_Player.SetMaxFps(m_TargetFps);

    // Run UI tick timer at exact target FPS (30 FPS = 33ms, 60 FPS = 16ms, 120 FPS = 8ms)
    int tickMs = (m_TargetFps >= 120) ? 8 : (m_TargetFps >= 60 ? 16 : 33);
    m_TickTimer = std::make_unique<slint::Timer>(
        std::chrono::milliseconds(tickMs),
        [this]() { OnTick(); }
    );
    SavePersistedState();
}

void SlintApp::ApplyGpuAndUpscalerConfig() {
    m_GpuHwdecMode      = std::clamp(m_Main->get_gpu_hwdec_mode(), 0, 4);
    m_NvencEnabled      = m_Main->get_nvenc_enabled();
    m_UpscalerMode      = std::clamp(m_Main->get_upscaler_mode(), 0, 4);
    m_UpscalerSharpness = std::clamp(m_Main->get_upscaler_sharpness() / 100.0f, 0.0f, 1.0f);
    m_SmoothMotion60    = m_Main->get_smooth_motion_60();

    m_Main->set_fsr_enabled(m_UpscalerMode > 0);
    m_Player.SetGpuBackend(m_GpuHwdecMode);
    // Keep hover preview on lightweight CPU decode (256x144 / 320x180) so it NEVER
    // steals or thrashes NVIDIA NVDEC hardware decoder sessions from the main player!
    m_HoverPlayer.SetGpuBackend(4);
    m_HoverPlayer.SetMaxFps(20);
    m_Player.SetUpscaler(m_UpscalerMode, m_UpscalerSharpness, m_SmoothMotion60);
    m_Player.SetMaxFps(m_TargetFps);
    ApplyShaders();

    static const char* kHwLabels[] = { "Auto GPU", "NVIDIA NVDEC/CUDA", "Direct3D 11", "VAAPI/Vulkan", "Solo CPU" };
    static const char* kUpLabels[] = { "Bilineal Rápido", "AMD FSR 1.0", "FidelityFX CAS+", "NVIDIA NIS", "LanczosHQ 4K" };

#ifdef __APPLE__
    const std::string decodeLabel = (m_GpuHwdecMode == 4) ? "Solo CPU" : "VideoToolbox GPU";
    std::string status = "GPU: " + m_DetectedGpuName +
                         " • Render: Metal (Skia) • Decode: " + decodeLabel +
                         " • Reescalado: " + std::string(kUpLabels[m_UpscalerMode]);
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
    const std::string decodeLabel = (m_GpuHwdecMode == 4) ? "Solo CPU" : "Direct3D 11 (ARM64)";
    std::string status = "GPU: " + m_DetectedGpuName +
                         " • Render: Direct3D/Skia • Decode: " + decodeLabel +
                         " • Reescalado: " + std::string(kUpLabels[m_UpscalerMode]);
#else
    std::string status = "GPU: " + m_DetectedGpuName +
                         " • Decode: " + std::string(kHwLabels[m_GpuHwdecMode]) +
                         " • Reescalado: " + std::string(kUpLabels[m_UpscalerMode]);
    if (m_NvencEnabled) {
        status += m_NvencAvailable ? " • NVENC: ACTIVO" : " • NVENC: Fallback seguro (GPU sin NVENC)";
    }
#endif
    m_Main->set_gpu_status_label(slint::SharedString(status));
}

void SlintApp::ApplyLowSpecPreset() {
    const unsigned int cores = std::max(1u, std::thread::hardware_concurrency());
    m_Main->set_performance_mode(true);
    // Use hardware GPU decode (Auto -> uses GT 710 NVDEC/D3D11VA or Intel QuickSync so the 2-core CPU stays free!)
    m_GpuHwdecMode = 0;
    m_Main->set_gpu_hwdec_mode(0);
    // On <=2 cores use ultra-fast bilinear (0); otherwise FidelityFX CAS+ (2)
    m_UpscalerMode = (cores <= 2) ? 0 : 2;
    m_Main->set_upscaler_mode(m_UpscalerMode);
    m_SmoothMotion60 = false;
    m_Main->set_smooth_motion_60(false);
    m_HoverPlayer.SetRenderSize(256, 144);

    // Vista previa casi quieta + prioridad total a la salida pública: en un
    // Celeron/Pentium de 2 núcleos esto es lo que más libera al decodificador.
    m_Main->set_preview_enabled(true);
    m_Main->set_preview_max_fps(5);
    m_Main->set_prioritize_output(true);
    m_LastPreviewDeliverMs = 0;

    ApplyGpuAndUpscalerConfig();
    UpdatePlayerRenderSize();
    ApplyTargetFps(30);
    PurgeMemoryCaches();
    SavePersistedState();
    AppendDebugLog("Perfil PC Modesta activado (2 núcleos / GT 710 / 4GB RAM): GPU Decode + RAM optimizada.");
}

void SlintApp::PurgeMemoryCaches() {
    m_ThumbCache.clear();
    if (!m_HasStaticImage) {
        m_StaticSrcPath.clear();
        m_StaticSrcRgba.clear();
        m_StaticSrcRgba.shrink_to_fit();
        m_StaticSrcW = 0;
        m_StaticSrcH = 0;
    }
    AppendDebugLog("Caché de miniaturas y buffers en RAM liberados.");
    if (m_ActiveTab == 12) RefreshDebugInfo();
}

void SlintApp::DeliverVideoFrameToOutputs() {
    if (!m_HasVideoBg) return;
    slint::Image frame;
    if (!m_Player.PollFrame(frame)) return;

    ++m_PerfVideoFrames;
    const uint64_t seq = ++m_DeliveredVideoSeq;

    // 1. Audience ProjectorWindow ALWAYS receives 100% of frames (full 30 / 60 / 120 FPS!)
    if (!m_Screens.empty() && m_Screens[0].bgIsVideo) {
        m_Screens[0].bgImage    = frame;
        m_Screens[0].hasBgImage = true;
        if (!m_AudienceFrozen) {
            m_Projector->set_bg_image(frame);
            m_Projector->set_has_bg_image(true);
        }
    }

    // 2. Smart decimation for MainWindow's small preview monitor so MainWindow's eglSwapBuffers
    //    never serializes against ProjectorWindow VSync on Wayland/OpenGL (eliminates 20/30fps VSync cap!)
    const bool perf = m_Main->get_performance_mode();

    // ── Ahorro de energía: vista previa del operador ──────────────────────
    // La salida al público (bloque 1, arriba) ya recibió el 100% de los
    // cuadros SIEMPRE. Acá decidimos cuánto de eso llega a la vista previa
    // del operador, que es el costo evitable en PC de bajos recursos:
    //   preview-enabled=false -> no se entrega NADA a la vista previa.
    //   prioritize-output=true -> la vista previa se fuerza a ~1 FPS para
    //                             dejarle todo el equipo a la salida pública.
    //   preview-max-fps>0     -> tope duro de FPS por tiempo real (1/5/15/30).
    if (!m_Main->get_preview_enabled())
        return;

    int previewCapFps = m_Main->get_preview_max_fps();
    if (m_Main->get_prioritize_output()) {
        // "Modo salida": la referencia del operador basta a 1 FPS. Si el
        // usuario ya pidió un tope más bajo, se respeta el más restrictivo.
        previewCapFps = (previewCapFps > 0) ? std::min(previewCapFps, 1) : 1;
    }

    if (previewCapFps > 0) {
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const int64_t minIntervalMs = 1000 / static_cast<int64_t>(previewCapFps);
        // Margen de 2ms para no perder un cuadro justo en el borde del
        // intervalo por el jitter del scheduler.
        if ((nowMs - m_LastPreviewDeliverMs) < (minIntervalMs - 2)) {
            return;
        }
        m_LastPreviewDeliverMs = nowMs;
    }

    uint64_t previewDecim = 1;
    if (previewCapFps > 0) {
        // Ya pasamos el filtro por tiempo real de arriba: este cuadro debe
        // entregarse tal cual, sin volver a decimarlo por secuencia (si no,
        // el tope efectivo quedaría muy por debajo del pedido).
        previewDecim = 1;
    } else if (m_ProjectorVisible) {
        if (perf) {
            previewDecim = (m_TargetFps >= 120) ? 8 : (m_TargetFps >= 60 ? 4 : 2);
        } else {
            previewDecim = (m_TargetFps >= 120) ? 6 : (m_TargetFps >= 60 ? 3 : 2);
        }
    } else if (perf && m_TargetFps >= 60) {
        previewDecim = (m_TargetFps >= 120) ? 4 : 2;
    }

    if ((seq % previewDecim) == 0 || seq <= 2) {
        if (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size()) &&
            m_Screens[m_InspectedScreenIdx].bgIsVideo) {
            if (m_InspectedScreenIdx > 0) {
                m_Screens[m_InspectedScreenIdx].bgImage    = frame;
                m_Screens[m_InspectedScreenIdx].hasBgImage = true;
            }
            m_Main->set_live_preview_image(frame);
            m_Main->set_has_live_preview_image(true);
        }
        if ((m_ActiveTab == 2 || m_ActiveTab == 3) && !m_HoverVideoActive) {
            m_Main->set_selected_media_preview(frame);
            m_Main->set_has_selected_media_preview(true);
        }
    }
}

void SlintApp::DetectGpuCapabilitiesAsync() {
    auto aliveFlag = m_AppAliveFlag;
    std::thread([this, aliveFlag]() {
        std::string gpuName;
        bool hasNvenc = false;

#ifdef _WIN32
        gpuName = RunCommandCapture("nvidia-smi --query-gpu=name --format=csv,noheader 2>nul");
        if (gpuName.empty()) {
            gpuName = RunCommandCapture("powershell -NoProfile -Command \"(Get-CimInstance Win32_VideoController | Select-Object -First 1 -ExpandProperty Name)\" 2>nul");
        }
        std::string encs = RunCommandCapture("ffmpeg -hide_banner -encoders 2>nul");
#elif defined(__APPLE__)
        gpuName = RunCommandCapture("system_profiler SPDisplaysDataType 2>/dev/null | grep -iE 'Chipset Model|Modelo de chipset' | head -n 1 | sed 's/^.*: //'");
        if (gpuName.empty()) {
            gpuName = RunCommandCapture("sysctl -n machdep.cpu.brand_string 2>/dev/null");
        }
        std::string encs = RunCommandCapture("ffmpeg -hide_banner -encoders 2>/dev/null");
#else
        gpuName = RunCommandCapture("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
        if (gpuName.empty()) {
            gpuName = RunCommandCapture("lspci 2>/dev/null | grep -iE 'vga|3d|display' | head -n 1 | sed 's/^.*: //'");
        }
        std::string encs = RunCommandCapture("ffmpeg -hide_banner -encoders 2>/dev/null");
#endif
        while (!gpuName.empty() && (gpuName.back() == '\n' || gpuName.back() == '\r' || gpuName.back() == ' ')) {
            gpuName.pop_back();
        }
        auto nlPos = gpuName.find('\n');
        if (nlPos != std::string::npos) gpuName = gpuName.substr(0, nlPos);
        if (gpuName.empty()) gpuName = "GPU Estándar del Sistema";

        std::string lowGpu = ToLowerUtf8(gpuName);
        bool isNvidia = (lowGpu.find("nvidia") != std::string::npos || lowGpu.find("geforce") != std::string::npos ||
                         lowGpu.find("quadro") != std::string::npos || lowGpu.find("rtx") != std::string::npos ||
                         lowGpu.find("gtx") != std::string::npos);
        bool isGt710  = (lowGpu.find("gt 710") != std::string::npos || lowGpu.find("gt710") != std::string::npos);
        if (isNvidia && !isGt710 && encs.find("h264_nvenc") != std::string::npos) {
            hasNvenc = true;
        }

        slint::invoke_from_event_loop([this, aliveFlag, gpuName, hasNvenc]() {
            if (!aliveFlag || !aliveFlag->load(std::memory_order_acquire)) return;
            m_DetectedGpuName = gpuName;
            m_NvencAvailable  = hasNvenc;
            ApplyGpuAndUpscalerConfig();
        });
    }).detach();
}

void SlintApp::ToggleStageWindow() {
    m_StageVisible = !m_StageVisible;
    m_Main->set_stage_live(m_StageVisible);
    if (m_StageVisible) {
        m_Stage->show();
    } else {
        m_Stage->hide();
    }
}

void SlintApp::ToggleFreeze() {
    m_AudienceFrozen = !m_AudienceFrozen;
    m_Main->set_audience_frozen(m_AudienceFrozen);
    AppendDebugLog(m_AudienceFrozen ? "FREEZE ACTIVADO: ventana secundaria congelada." : "FREEZE DESACTIVADO: sincronizando salida.");
    if (!m_AudienceFrozen) {
        ResyncProjectorFromLive();
    }
}

void SlintApp::ResyncProjectorFromLive() {
    ApplyShaders();
    ApplyScalingMode(m_BgScaleMode);
    SyncAlertConfig();
    SyncTilesState();
    SyncScreenOutputs();
}

void SlintApp::ApplyScalingMode(int mode) {
    m_BgScaleMode = std::clamp(mode, 0, 2);
    m_Main->set_bg_scale_mode(m_BgScaleMode);
    m_Player.SetScaleMode(m_BgScaleMode);
    if (!m_AudienceFrozen) {
        m_Projector->set_bg_scale_mode(m_BgScaleMode);
    }
    ReapplyCurrentStaticImage();
    SavePersistedState();
}

void SlintApp::ScanAllLibrariesManual() {
    m_CachedTabEntries.clear();
    m_ThumbCache.clear();
    m_MusicScanned = false;
    RefreshBgDock();
    RefreshOverlayLayers();
    if (m_ActiveTab == 13) {
        RefreshLibraryHub();
    } else if (m_ActiveTab == 0 || m_ActiveTab == 1 || m_ActiveTab == 2 || m_ActiveTab == 10) {
        if (m_ActiveTab == 1) m_BiblePaths.clear();
        LoadTabData(m_ActiveTab);
    } else if (m_ActiveTab == 9) {
        ScanMusicTracks();
    }
    AppendDebugLog("Escaneo manual de bibliotecas completado.");
    if (m_ActiveTab == 12) RefreshDebugInfo();
}

void SlintApp::RebuildSlideGridRows() {
    m_SlideRowsModel->clear();
    int cols = std::clamp(m_Main->get_slide_columns(), 1, 4);
    int total = static_cast<int>(m_SlidesModel->row_count());
    for (int i = 0; i < total; i += cols) {
        auto cellsModel = std::make_shared<slint::VectorModel<SlideGridCell>>();
        for (int c = 0; c < cols; ++c) {
            SlideGridCell cell;
            int idx = i + c;
            if (idx < total) {
                if (auto s = m_SlidesModel->row_data(idx)) {
                    cell.index = idx;
                    cell.badge = s->badge;
                    cell.body  = s->body;
                    cell.valid = true;
                }
            } else {
                cell.index = -1;
                cell.valid = false;
            }
            cellsModel->push_back(cell);
        }
        SlideGridRow row;
        row.cells = cellsModel;
        m_SlideRowsModel->push_back(row);
    }
}

// ── Song Editor & Metadata (ImGui songs_meta/*.json compatible) ──────────────
void SlintApp::ReadSongMeta(const std::string& songPath, std::string& outTitle, std::string& outAuthor) {
    fs::path p(songPath);
    outTitle  = PathToUtf8(p.stem());
    outAuthor.clear();

    // 1. Check ImGui songs_meta/<filename>.json
    std::string fname = PathToUtf8(p.filename());
    for (char& ch : fname) { if (ch == ':') ch = '_'; }
    fs::path metaPath = fs::path(ProyecThor::GetAssetsPath()) / "songs_meta" / (fname + ".json");
    std::ifstream mf(metaPath);
    if (mf.is_open()) {
        try {
            nlohmann::json j;
            mf >> j;
            std::string mt = j.value("title", "");
            std::string ma = j.value("artistAuthor", "");
            if (ma.empty()) ma = j.value("author", "");
            if (ma.empty()) ma = j.value("artist", "");
            if (!mt.empty()) outTitle  = mt;
            if (!ma.empty()) outAuthor = ma;
        } catch (...) {}
    }

    // 2. Fallback: inline headers in first lines of .txt/.song (e.g. "Autor: ..." / "Titulo: ...")
    if (outAuthor.empty()) {
        std::ifstream sf(songPath);
        std::string line;
        for (int i = 0; i < 4 && std::getline(sf, line); ++i) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::string low = ToLowerUtf8(line);
            if (low.rfind("autor:", 0) == 0 || low.rfind("author:", 0) == 0 || low.rfind("artista:", 0) == 0) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    outAuthor = line.substr(colon + 1);
                    while (!outAuthor.empty() && outAuthor.front() == ' ') outAuthor.erase(outAuthor.begin());
                }
            } else if (low.rfind("titulo:", 0) == 0 || low.rfind("título:", 0) == 0 || low.rfind("title:", 0) == 0) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string t = line.substr(colon + 1);
                    while (!t.empty() && t.front() == ' ') t.erase(t.begin());
                    if (!t.empty()) outTitle = t;
                }
            }
        }
    }

    // 3. Fallback: infer author from "Title - Author" or "Title (Author)"
    if (outAuthor.empty()) {
        auto dashPos = outTitle.rfind(" - ");
        if (dashPos != std::string::npos && dashPos > 0 && dashPos + 3 < outTitle.size()) {
            outAuthor = outTitle.substr(dashPos + 3);
            outTitle  = outTitle.substr(0, dashPos);
            while (!outAuthor.empty() && outAuthor.front() == ' ') outAuthor.erase(outAuthor.begin());
            while (!outTitle.empty() && outTitle.back() == ' ') outTitle.pop_back();
        } else if (!outTitle.empty() && outTitle.back() == ')') {
            auto openParen = outTitle.rfind(" (");
            if (openParen != std::string::npos && openParen > 0 && openParen + 2 < outTitle.size() - 1) {
                outAuthor = outTitle.substr(openParen + 2, outTitle.size() - openParen - 3);
                outTitle  = outTitle.substr(0, openParen);
                while (!outTitle.empty() && outTitle.back() == ' ') outTitle.pop_back();
            }
        }
    }
}

void SlintApp::WriteSongMeta(const std::string& songPath, const std::string& title, const std::string& author) {
    fs::path p(songPath);
    std::string fname = PathToUtf8(p.filename());
    for (char& ch : fname) { if (ch == ':') ch = '_'; }
    fs::path metaDir = fs::path(ProyecThor::GetAssetsPath()) / "songs_meta";
    std::error_code ec;
    fs::create_directories(metaDir, ec);
    fs::path metaPath = metaDir / (fname + ".json");

    nlohmann::json j;
    {
        std::ifstream existing(metaPath);
        if (existing.is_open()) {
            try { existing >> j; } catch (...) {}
        }
    }
    j["version"]      = j.value("version", 1);
    j["title"]        = title;
    j["artistAuthor"] = author;
    std::ofstream out(metaPath, std::ios::trunc);
    if (out.is_open()) out << j.dump(2);
}

void SlintApp::OpenNewSongEditor() {
    m_EditingSongPath.clear();
    m_Main->set_editor_song_title("");
    m_Main->set_editor_song_author("");
    m_Main->set_editor_song_body("");
    m_Main->set_song_editor_open(true);
}

void SlintApp::OpenEditSongEditor() {
    int fIdx = m_Main->get_selected_library_index();
    if (fIdx < 0 || fIdx >= static_cast<int>(m_FilteredIndices.size())) return;
    const auto& entry = m_AllEntries[m_FilteredIndices[fIdx]];

    std::ifstream f(entry.path);
    std::ostringstream ss;
    if (f.is_open()) ss << f.rdbuf();

    m_EditingSongPath = entry.path;
    m_Main->set_editor_song_title(slint::SharedString(entry.title));
    m_Main->set_editor_song_author(slint::SharedString(entry.author));
    m_Main->set_editor_song_body(slint::SharedString(ss.str()));
    m_Main->set_song_editor_open(true);
}

void SlintApp::SaveEditedSong(const std::string& title, const std::string& author, const std::string& body) {
    std::string safeName = SanitizeFilename(title);
    fs::path dir = ProyecThor::SongsPath();
    std::error_code ec;
    fs::create_directories(dir, ec);

    fs::path targetPath = m_EditingSongPath.empty()
        ? (dir / (safeName + ".txt"))
        : fs::path(m_EditingSongPath);

    std::ofstream out(targetPath, std::ios::trunc);
    if (out.is_open()) {
        out << body;
        out.close();
    }
    WriteSongMeta(PathToUtf8(targetPath), title.empty() ? safeName : title, author);

    m_CachedTabEntries.erase(0);
    m_Main->set_song_editor_open(false);
    LoadTabData(0);

    // Select newly saved song
    for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
        if (m_AllEntries[m_FilteredIndices[i]].path == PathToUtf8(targetPath)) {
            m_Main->set_selected_library_index(i);
            SelectLibraryItem(i);
            break;
        }
    }
}

// ── Service Queue ────────────────────────────────────────────────────────────
void SlintApp::AddCurrentToQueue() {
    int fIdx = m_Main->get_selected_library_index();
    if (fIdx < 0 || fIdx >= static_cast<int>(m_FilteredIndices.size())) return;
    const auto& entry = m_AllEntries[m_FilteredIndices[fIdx]];

    ServiceQueueEntry q;
    q.tab  = m_ActiveTab;
    q.path = entry.path;
    if (m_ActiveTab == 0) {
        q.kind  = "CANCIÓN";
        q.title = entry.title;
    } else if (m_ActiveTab == 1) {
        q.kind         = "BIBLIA";
        q.title        = entry.title + " " + std::to_string(m_CurrentBibleChapter);
        q.bibleBookIdx = m_FilteredIndices[fIdx];
        q.bibleChapter = m_CurrentBibleChapter;
    } else if (m_ActiveTab == 2) {
        q.kind  = "MEDIA";
        q.title = entry.title;
    } else {
        q.kind  = "FONDO";
        q.title = entry.title;
    }
    m_Queue.push_back(std::move(q));
    m_Main->set_queue_category(0);
    RefreshQueueModel();
}

void SlintApp::ActivateQueueItem(int index) {
    if (index < 0 || index >= static_cast<int>(m_Queue.size())) return;
    const auto q = m_Queue[index];

    if (q.tab == 9) {
        if (!m_MusicScanned) ScanMusicTracks();
        for (int i = 0; i < static_cast<int>(m_MusicEntries.size()); ++i) {
            if (m_MusicEntries[i].path == q.path) {
                PlayMusicTrack(i);
                return;
            }
        }
        return;
    }

    m_ActiveTab = q.tab;
    m_Main->set_active_tab(q.tab);
    LoadTabData(q.tab);

    if (q.tab == 1 && q.bibleBookIdx >= 0) {
        m_Main->set_selected_library_index(q.bibleBookIdx);
        m_CurrentBibleBookIdx = q.bibleBookIdx;
        m_Main->set_selected_chapter(q.bibleChapter);
        LoadBibleChapter(q.bibleBookIdx, q.bibleChapter);
        return;
    }

    for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
        if (m_AllEntries[m_FilteredIndices[i]].path == q.path) {
            m_Main->set_selected_library_index(i);
            SelectLibraryItem(i);
            break;
        }
    }
}

void SlintApp::RefreshQueueModel() {
    m_QueueModel->clear();
    for (const auto& q : m_Queue) {
        QueueItem item;
        item.title = slint::SharedString(q.title);
        item.kind  = slint::SharedString(q.kind);
        m_QueueModel->push_back(item);
    }
}

// ── Song Playlists (persisted in assets/playlists/song_playlists.json) ───────
void SlintApp::LoadSongPlaylists() {
    m_SongPlaylists.clear();
    fs::path plFile = fs::path(ProyecThor::GetAssetsPath()) / "playlists" / "song_playlists.json";
    std::ifstream f(plFile);
    if (!f.is_open()) {
        RefreshSongPlaylistsModel();
        return;
    }
    try {
        nlohmann::json j;
        f >> j;
        if (j.is_array()) {
            for (const auto& jp : j) {
                SongPlaylist pl;
                pl.name = jp.value("name", "");
                if (jp.contains("songs") && jp["songs"].is_array()) {
                    for (const auto& sp : jp["songs"]) {
                        if (sp.is_string()) pl.songPaths.push_back(sp.get<std::string>());
                    }
                }
                if (!pl.name.empty()) m_SongPlaylists.push_back(std::move(pl));
            }
        }
    } catch (...) {}
    RefreshSongPlaylistsModel();
}

void SlintApp::SaveSongPlaylists() {
    fs::path dir = fs::path(ProyecThor::GetAssetsPath()) / "playlists";
    std::error_code ec;
    fs::create_directories(dir, ec);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& pl : m_SongPlaylists) {
        arr.push_back({ {"name", pl.name}, {"songs", pl.songPaths} });
    }
    std::ofstream out(dir / "song_playlists.json", std::ios::trunc);
    if (out.is_open()) out << arr.dump(2);
}

void SlintApp::CreateSongPlaylist(const std::string& rawName) {
    std::string clean = rawName;
    while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
    while (!clean.empty() && clean.back() == ' ')  clean.pop_back();
    if (clean.empty()) return;

    SongPlaylist pl;
    pl.name = clean;
    m_SongPlaylists.push_back(std::move(pl));
    m_ActiveSongPlaylistIdx = static_cast<int>(m_SongPlaylists.size()) - 1;
    m_PlaylistFilterActive  = false;
    m_Main->set_active_song_playlist_idx(m_ActiveSongPlaylistIdx);
    m_Main->set_playlist_filter_active(false);
    RefreshSongPlaylistsModel();
    SaveSongPlaylists();
}

void SlintApp::SelectSongPlaylist(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SongPlaylists.size())) {
        m_ActiveSongPlaylistIdx = -1;
        m_PlaylistFilterActive  = false;
        m_Main->set_active_song_playlist_idx(-1);
        m_Main->set_playlist_filter_active(false);
        m_Main->set_active_playlist_song_count(0);
        RefreshLibraryModel();
        return;
    }
    m_ActiveSongPlaylistIdx = idx;
    m_Main->set_active_song_playlist_idx(idx);
    m_Main->set_active_playlist_song_count(static_cast<int>(m_SongPlaylists[idx].songPaths.size()));
    if (m_PlaylistFilterActive) {
        RefreshLibraryModel();
    }
}

void SlintApp::TogglePlaylistFilter() {
    if (m_ActiveSongPlaylistIdx < 0 || m_ActiveSongPlaylistIdx >= static_cast<int>(m_SongPlaylists.size())) {
        m_PlaylistFilterActive = false;
        m_Main->set_playlist_filter_active(false);
        return;
    }
    m_PlaylistFilterActive = !m_PlaylistFilterActive;
    m_Main->set_playlist_filter_active(m_PlaylistFilterActive);
    RefreshLibraryModel();
}

void SlintApp::AddSelectedSongToPlaylist() {
    if (m_ActiveTab != 0) return;
    if (m_ActiveSongPlaylistIdx < 0 || m_ActiveSongPlaylistIdx >= static_cast<int>(m_SongPlaylists.size())) return;
    int fIdx = m_Main->get_selected_library_index();
    if (fIdx < 0 || fIdx >= static_cast<int>(m_FilteredIndices.size())) return;

    const auto& entry = m_AllEntries[m_FilteredIndices[fIdx]];
    auto& paths = m_SongPlaylists[m_ActiveSongPlaylistIdx].songPaths;
    if (std::find(paths.begin(), paths.end(), entry.path) == paths.end()) {
        paths.push_back(entry.path);
        m_Main->set_active_playlist_song_count(static_cast<int>(paths.size()));
        SaveSongPlaylists();
    }
}

void SlintApp::SendActivePlaylistToQueue() {
    if (m_ActiveSongPlaylistIdx < 0 || m_ActiveSongPlaylistIdx >= static_cast<int>(m_SongPlaylists.size())) return;
    const auto& pl = m_SongPlaylists[m_ActiveSongPlaylistIdx];
    for (const auto& sp : pl.songPaths) {
        ServiceQueueEntry q;
        q.tab   = 0;
        q.path  = sp;
        q.kind  = "PLAYLIST";
        q.title = PathToUtf8(fs::path(sp).stem());
        m_Queue.push_back(std::move(q));
    }
    m_Main->set_queue_category(0);
    RefreshQueueModel();
}

void SlintApp::DeleteSongPlaylist(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SongPlaylists.size())) return;
    m_SongPlaylists.erase(m_SongPlaylists.begin() + idx);
    m_ActiveSongPlaylistIdx = -1;
    m_PlaylistFilterActive  = false;
    m_Main->set_active_song_playlist_idx(-1);
    m_Main->set_playlist_filter_active(false);
    m_Main->set_active_playlist_song_count(0);
    RefreshSongPlaylistsModel();
    RefreshLibraryModel();
    SaveSongPlaylists();
}

void SlintApp::RefreshSongPlaylistsModel() {
    m_SongPlaylistNamesModel->clear();
    for (const auto& pl : m_SongPlaylists) {
        m_SongPlaylistNamesModel->push_back(slint::SharedString(pl.name));
    }
    if (m_ActiveSongPlaylistIdx >= 0 && m_ActiveSongPlaylistIdx < static_cast<int>(m_SongPlaylists.size())) {
        m_Main->set_active_playlist_song_count(static_cast<int>(m_SongPlaylists[m_ActiveSongPlaylistIdx].songPaths.size()));
    } else {
        m_Main->set_active_playlist_song_count(0);
    }
}

// ── Overlay Queue (Category 2 in Service Queue) ──────────────────────────────
void SlintApp::AddSelectedOverlayToQueue() {
    if (!m_HasSelectedPreviewEntry || !m_SelectedPreviewEntry.isOverlay) return;
    for (const auto& o : m_OverlayQueue) {
        if (o.path == m_SelectedPreviewEntry.path) return;
    }
    OverlayQueueEntry o;
    o.title = m_SelectedPreviewEntry.title;
    o.path  = m_SelectedPreviewEntry.path;
    m_OverlayQueue.push_back(std::move(o));
    m_Main->set_queue_category(2);
    RefreshOverlayQueueModel();
}

void SlintApp::ToggleOverlayQueueItem(int index) {
    if (index < 0 || index >= static_cast<int>(m_OverlayQueue.size())) return;
    const auto o = m_OverlayQueue[index];
    if (o.isCustom) {
        for (const auto& cov : m_CustomOverlays) {
            if (cov.path == o.path) {
                ApplyOverlayConfigToTargets(cov);
                return;
            }
        }
        return;
    }
    int refScr = (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size())) ? m_InspectedScreenIdx : 0;
    if (m_Screens[refScr].overlayPath == o.path && m_Screens[refScr].hasOverlayImage) {
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            RemoveLayerOnScreen(i, "overlay");
        }
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
        SyncScreenOutputs();
        return;
    }
    m_ActiveOverlayPath = o.path;
    auto img = slint::Image::load_from_path(slint::SharedString(o.path));
    slint::Image thumb;
    bool hasThumb = LoadOrCreateThumbnail(o.path, false, thumb);
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].overlayName     = o.title;
        m_Screens[i].overlayPath     = o.path;
        m_Screens[i].hasOverlayImage = true;
        m_Screens[i].overlayImage    = img;
        m_Screens[i].hasOverlayThumb = hasThumb;
        if (hasThumb) m_Screens[i].overlayThumb = thumb;
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
    SyncScreenOutputs();
}

void SlintApp::RefreshOverlayQueueModel() {
    m_OverlayQueueModel->clear();
    int refScr = (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size())) ? m_InspectedScreenIdx : 0;
    const bool hasImgLive = !m_Screens.empty() && m_Screens[refScr].hasOverlayImage;
    const std::string activeImgPath = !m_Screens.empty() ? m_Screens[refScr].overlayPath : m_ActiveOverlayPath;
    const bool hasCovLive = !m_Screens.empty() && m_Screens[refScr].hasCustomOverlay;
    const std::string activeCovPath = hasCovLive ? m_Screens[refScr].customOverlay.path : "";
    for (const auto& o : m_OverlayQueue) {
        QueueItem item;
        item.title = slint::SharedString(o.title);
        bool isOn = o.isCustom ? (hasCovLive && activeCovPath == o.path)
                               : (hasImgLive && activeImgPath == o.path);
        item.kind  = slint::SharedString(isOn ? "● ON" : (o.isCustom ? "TIEMPO" : "PNG"));
        m_OverlayQueueModel->push_back(item);
    }
}

// ── Hover Video Preview (dedicated silent low-res libmpv instance) ───────────
void SlintApp::StartHoverVideoPreview(const std::string& path) {
    if (path.empty()) {
        StopHoverVideoPreview();
        return;
    }
    if (m_HoverVideoActive && m_HoverVideoPath == path) return;
    // Clear previous cached hover frame immediately so the new item never flashes the old thumbnail
    m_Main->set_has_hover_video_frame(false);
    m_Main->set_hover_video_frame(slint::Image());
    m_HoverVideoPath   = path;
    m_HoverVideoActive = true;
    m_HoverPlayer.Play(path, true, true);
}

void SlintApp::StopHoverVideoPreview() {
    if (m_HoverVideoActive) {
        m_HoverPlayer.Stop();
        m_HoverVideoActive = false;
        m_HoverVideoPath.clear();
    }
    m_Main->set_has_hover_video_frame(false);
    m_Main->set_hover_video_frame(slint::Image());
}

// ── Scene Pads ───────────────────────────────────────────────────────────────
void SlintApp::SaveToPad(int index) {
    if (index < 0 || index >= 8) return;
    auto& p = m_Pads[index];
    p.assigned     = true;
    p.text         = std::string(m_Main->get_live_text());
    p.ref          = std::string(m_Main->get_live_ref());
    p.bgPath       = m_CurrentBgPath;
    p.bgIsImage    = m_HasStaticImage;
    p.bgIsVideo    = m_HasVideoBg;
    p.textSize     = m_Main->get_text_size();
    p.textAlign    = m_Main->get_text_align();
    p.textValign   = m_Main->get_text_valign();
    p.textColorIdx = m_Main->get_text_color_idx();

    if (!p.ref.empty()) {
        p.name = "Pad " + std::to_string(index + 1) + " • " + p.ref.substr(0, 14);
    } else if (!p.bgPath.empty()) {
        p.name = "Pad " + std::to_string(index + 1) + " • " + PathToUtf8(fs::path(p.bgPath).stem()).substr(0, 14);
    } else {
        p.name = "Pad " + std::to_string(index + 1) + " • Escena";
    }

    RefreshPadsModel();
    SavePersistedState();
}

void SlintApp::TriggerPad(int index) {
    if (index < 0 || index >= 8 || !m_Pads[index].assigned) return;
    const auto& p = m_Pads[index];

    m_Main->set_text_size(p.textSize);
    m_Main->set_text_align(p.textAlign);
    m_Main->set_text_valign(p.textValign);
    SelectTextColor(p.textColorIdx);

    if (!m_Screens.empty()) {
        m_Screens[0].textStyle.textSize   = p.textSize;
        m_Screens[0].textStyle.textAlign  = p.textAlign;
        m_Screens[0].textStyle.textValign = p.textValign;
        m_Screens[0].textStyle.colorR     = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_r(), 0.0f, 255.0f));
        m_Screens[0].textStyle.colorG     = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_g(), 0.0f, 255.0f));
        m_Screens[0].textStyle.colorB     = static_cast<uint8_t>(std::clamp(m_Main->get_text_color_b(), 0.0f, 255.0f));
        m_Screens[0].assignedStyleIdx     = -1;
        SetScreenLiveText(m_Screens[0], p.text, p.ref, false);
    }
    if (m_Screens.size() > 2) {
        SetScreenLiveText(m_Screens[2], p.text, p.ref, false);
    }
    SyncScreenOutputs();

    if (!p.bgPath.empty()) {
        m_CurrentBgPath = p.bgPath;
        m_Main->set_live_bg_name(slint::SharedString(PathToUtf8(fs::path(p.bgPath).stem())));
        if (p.bgIsImage) {
            m_Player.Stop();
            m_HasVideoBg = false;
            m_HasStaticImage = true;
            slint::Image img;
            RenderScaledStaticImage(p.bgPath, img);
            m_Main->set_live_preview_image(img);
            m_Main->set_has_live_preview_image(true);
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
            if (IsLanRunning()) UpdateLanStaticFrame(img);
        } else {
            m_HasStaticImage = false;
            m_HasVideoBg = p.bgIsVideo;
            m_Player.Play(p.bgPath, true, true);
        }
    }
}

void SlintApp::RefreshPadsModel() {
    m_PadsModel->clear();
    for (int i = 0; i < 8; ++i) {
        PadItem item;
        item.name     = slint::SharedString(m_Pads[i].name);
        item.assigned = m_Pads[i].assigned;
        if (m_Pads[i].assigned) {
            std::string sum;
            if (!m_Pads[i].text.empty()) {
                sum = m_Pads[i].text.substr(0, 60);
            } else if (!m_Pads[i].bgPath.empty()) {
                sum = "Fondo: " + PathToUtf8(fs::path(m_Pads[i].bgPath).filename());
            } else {
                sum = "Solo estilo guardado";
            }
            item.summary = slint::SharedString(sum);
        } else {
            item.summary = "";
        }
        m_PadsModel->push_back(item);
    }
}

// ── Persistence (settings_slint.json) ────────────────────────────────────────
template <typename StyleT>
static nlohmann::json SavedStyleToJson(const StyleT& st) {
    return {
        {"name", st.name}, {"textSize", st.textSize}, {"marginH", st.marginH},
        {"lyricsPosX", st.lyricsPosX}, {"lyricsPosY", st.lyricsPosY},
        {"lyricsWidth", st.lyricsWidth}, {"lyricsHeight", st.lyricsHeight},
        {"indexEnabled", st.indexEnabled}, {"indexPosX", st.indexPosX}, {"indexPosY", st.indexPosY},
        {"indexWidth", st.indexWidth}, {"indexHeight", st.indexHeight}, {"indexSize", st.indexSize}, {"indexAlign", st.indexAlign},
        {"indexFontFamIdx", st.indexFontFamIdx}, {"indexFontName", st.indexFontName}, {"indexFontWtIdx", st.indexFontWtIdx},
        {"indexColorR", st.indexColorR}, {"indexColorG", st.indexColorG}, {"indexColorB", st.indexColorB},
        {"indexShadow", st.indexShadow}, {"indexOutline", st.indexOutline}, {"indexGlow", st.indexGlow},
        {"indexBoxBg", st.indexBoxBg}, {"indexUppercase", st.indexUppercase},
        {"textAlign", st.textAlign}, {"textValign", st.textValign},
        {"colorR", st.colorR}, {"colorG", st.colorG}, {"colorB", st.colorB},
        {"fontFamIdx", st.fontFamIdx}, {"fontName", st.fontName}, {"fontWtIdx", st.fontWtIdx},
        {"shadow", st.shadow}, {"outline", st.outline}, {"glow", st.glow},
        {"neon", st.neon}, {"text3d", st.text3d}, {"underline", st.underline},
        {"uppercase", st.uppercase}, {"boxBg", st.boxBg}, {"textFitMode", st.textFitMode}
    };
}

void SlintApp::LoadPersistedState() {
    fs::path path = fs::path(ProyecThor::GetAppDataRoot()) / "settings_slint.json";
    std::ifstream f(path);
    if (!f.is_open()) {
        // Primer arranque sin settings guardados: en hardware modesto (<=2
        // núcleos o <=8 GB de RAM, p. ej. Pentium Gold + GT 710) activar el
        // modo rendimiento y 30 FPS por defecto para que la experiencia salga
        // fluida de entrada. El usuario puede cambiarlo después y se persiste.
        if (IsLowEndHardware()) {
            ApplyLowSpecPreset();
        }
        return;
    }

    try {
        nlohmann::json j;
        f >> j;
        m_Main->set_text_size(j.value("textSize", 60.0f));
        m_Main->set_text_align(j.value("textAlign", 1));
        m_Main->set_text_valign(j.value("textValign", 1));
        m_Main->set_text_shadow(j.value("textShadow", true));
        m_Main->set_text_outline(j.value("textOutline", false));
        m_Main->set_text_glow(j.value("textGlow", false));
        m_Main->set_text_uppercase(j.value("textUppercase", false));
        m_Main->set_text_margin_h(j.value("textMarginH", 64.0f));
        int savedFamIdx = j.value("fontFamIdx", 0);
        std::string savedFontName = j.value("fontName", std::string(""));
        if (savedFontName.empty() && savedFamIdx > 0) savedFontName = ResolveFontPreset(savedFamIdx);
        m_Main->set_font_family_idx(savedFamIdx);
        m_Main->set_font_family_name(slint::SharedString(savedFontName));
        std::string savedIdxFont = j.value("indexFontName", std::string(""));
        m_Main->set_index_font_family_name(slint::SharedString(savedIdxFont));
        m_Main->set_font_weight_idx(j.value("fontWtIdx", 1));
        m_Main->set_transition_type(j.value("transitionType", 1));
        m_Main->set_transition_duration_ms(j.value("transitionMs", 350.0f));
        m_Main->set_tiles_enabled(j.value("tilesEnabled", false));
        m_Main->set_tiles_layout(j.value("tilesLayout", 0));
        m_Main->set_text_box_bg(j.value("textBoxBg", false));

        auto parseStyleJson = [](const nlohmann::json& js, const SavedTextStyle& base) {
            SavedTextStyle st = base;
            st.name            = js.value("name", base.name);
            st.textSize        = js.value("textSize", base.textSize);
            st.marginH         = js.value("marginH", base.marginH);
            st.lyricsPosX      = js.value("lyricsPosX", base.lyricsPosX);
            st.lyricsPosY      = js.value("lyricsPosY", base.lyricsPosY);
            st.lyricsWidth     = js.value("lyricsWidth", base.lyricsWidth);
            st.lyricsHeight    = js.value("lyricsHeight", base.lyricsHeight);
            st.indexEnabled    = js.value("indexEnabled", base.indexEnabled);
            st.indexPosX       = js.value("indexPosX", base.indexPosX);
            st.indexPosY       = js.value("indexPosY", base.indexPosY);
            st.indexWidth      = js.value("indexWidth", base.indexWidth);
            st.indexHeight     = js.value("indexHeight", base.indexHeight);
            st.indexSize       = js.value("indexSize", base.indexSize);
            st.indexAlign      = js.value("indexAlign", base.indexAlign);
            st.indexFontFamIdx = js.value("indexFontFamIdx", base.indexFontFamIdx);
            st.indexFontName   = js.value("indexFontName", base.indexFontName);
            st.indexFontWtIdx  = js.value("indexFontWtIdx", base.indexFontWtIdx);
            st.indexColorR     = static_cast<uint8_t>(js.value("indexColorR", static_cast<int>(base.indexColorR)));
            st.indexColorG     = static_cast<uint8_t>(js.value("indexColorG", static_cast<int>(base.indexColorG)));
            st.indexColorB     = static_cast<uint8_t>(js.value("indexColorB", static_cast<int>(base.indexColorB)));
            st.indexShadow     = js.value("indexShadow", base.indexShadow);
            st.indexOutline    = js.value("indexOutline", base.indexOutline);
            st.indexGlow       = js.value("indexGlow", base.indexGlow);
            st.indexBoxBg      = js.value("indexBoxBg", base.indexBoxBg);
            st.indexUppercase  = js.value("indexUppercase", base.indexUppercase);
            st.textAlign       = js.value("textAlign", base.textAlign);
            st.textValign      = js.value("textValign", base.textValign);
            st.colorR          = static_cast<uint8_t>(js.value("colorR", static_cast<int>(base.colorR)));
            st.colorG          = static_cast<uint8_t>(js.value("colorG", static_cast<int>(base.colorG)));
            st.colorB          = static_cast<uint8_t>(js.value("colorB", static_cast<int>(base.colorB)));
            st.fontFamIdx      = js.value("fontFamIdx", base.fontFamIdx);
            st.fontName        = js.value("fontName", base.fontName);
            st.fontWtIdx       = js.value("fontWtIdx", base.fontWtIdx);
            st.shadow          = js.value("shadow", base.shadow);
            st.outline         = js.value("outline", base.outline);
            st.glow            = js.value("glow", base.glow);
            st.neon            = js.value("neon", base.neon);
            st.text3d          = js.value("text3d", base.text3d);
            st.underline       = js.value("underline", base.underline);
            st.uppercase       = js.value("uppercase", base.uppercase);
            st.boxBg           = js.value("boxBg", base.boxBg);
            st.textFitMode     = std::clamp(js.value("textFitMode", base.textFitMode), 0, 2);
            return st;
        };

        if (j.contains("savedStyles") && j["savedStyles"].is_array()) {
            m_SavedStyles.clear();
            SavedTextStyle defaults;
            for (const auto& js : j["savedStyles"]) {
                SavedTextStyle st = parseStyleJson(js, defaults);
                if (!st.name.empty()) m_SavedStyles.push_back(st);
            }
        }
        int colIdx = j.value("textColorIdx", 0);
        m_Main->set_text_color_idx(colIdx);
        m_Main->set_text_color(TextColorFromIndex(colIdx));

        SavedTextStyle globalFallback = CaptureCurrentStyleFromUI("");
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            m_Screens[i].textStyle = globalFallback;
            if (i == 2) m_Screens[i].textStyle.textSize = 40.0f; // Stage classic default
            m_Screens[i].assignedStyleIdx = -1;
            m_Screens[i].useOwnStyle      = false;
        }
        if (j.contains("screenStyles") && j["screenStyles"].is_array()) {
            const auto& arr = j["screenStyles"];
            for (int i = 0; i < static_cast<int>(arr.size()); ++i) {
                const auto& item = arr[i];
                if (i >= static_cast<int>(m_Screens.size())) {
                    ScreenOutputState extra;
                    extra.id           = i;
                    extra.name         = item.value("name", "Sala " + std::to_string(i));
                    extra.roleBadge    = "SALA INDEPENDIENTE";
                    extra.isCustomRoom = true;
                    extra.textStyle    = globalFallback;
                    extra.useOwnStyle  = false;
                    m_Screens.push_back(std::move(extra));
                }
                if (item.contains("style") && item["style"].is_object()) {
                    m_Screens[i].textStyle = parseStyleJson(item["style"], m_Screens[i].textStyle);
                }
                int aIdx = item.value("assignedStyleIdx", -1);
                if (aIdx >= 0 && aIdx < static_cast<int>(m_SavedStyles.size())) {
                    m_Screens[i].assignedStyleIdx = aIdx;
                } else {
                    m_Screens[i].assignedStyleIdx = -1;
                }
                if (i > 0) {
                    if (item.contains("useOwnStyle")) {
                        m_Screens[i].useOwnStyle = item.value("useOwnStyle", false);
                    } else {
                        m_Screens[i].useOwnStyle = (m_Screens[i].assignedStyleIdx != m_Screens[0].assignedStyleIdx);
                    }
                }
            }
        }
        m_StyleTargetMode = std::clamp(j.value("styleTargetMode", 0), 0, static_cast<int>(m_Screens.size()));
        m_Main->set_style_target_mode(m_StyleTargetMode);
        m_Main->set_style_target_label(slint::SharedString(GetStyleTargetShortLabel(m_StyleTargetMode)));
        m_Main->set_fullscreen_output(j.value("fullscreen", true));
        // Pantalla de proyección elegida (se valida contra los monitores
        // detectados en RefreshMonitorList; -1 => usa la secundaria por defecto).
        m_ProjectionMonitorIdx = j.value("projectionMonitorIdx", -1);
        m_Main->set_fsr_enabled(j.value("fsr", true));
        m_BgScaleMode = std::clamp(j.value("bgScaleMode", 0), 0, 2);
        m_OutputResPreset = std::clamp(j.value("outputResPreset", 1), 0, 4);
        m_Main->set_bg_scale_mode(m_BgScaleMode);
        m_Main->set_output_res_preset(m_OutputResPreset);
        m_Projector->set_bg_scale_mode(m_BgScaleMode);
        m_Player.SetScaleMode(m_BgScaleMode);
        m_Main->set_slide_columns(std::clamp(j.value("slideColumns", 3), 1, 4));
        m_Main->set_slide_card_size(std::clamp(j.value("slideCardSize", 1), 0, 2));
        bool defaultPerf = IsLowEndHardware();
        bool perf = j.value("performanceMode", defaultPerf);
        m_Main->set_performance_mode(perf);
        m_HoverPlayer.SetRenderSize(perf ? 256 : 320, perf ? 144 : 180);

        // Vista previa del operador (ahorro de energía). En hardware de gama
        // baja arrancamos con la vista previa limitada a 15 FPS por defecto:
        // suficiente para operar, deja CPU/GPU para la salida pública.
        m_Main->set_preview_enabled(j.value("previewEnabled", true));
        m_Main->set_preview_max_fps(j.value("previewMaxFps", defaultPerf ? 15 : 0));
        m_Main->set_prioritize_output(j.value("prioritizeOutput", false));

        // Idioma de la interfaz (i18n). Por defecto español.
        ApplyUiLanguage(j.value("uiLanguage", std::string("es")));

        m_GpuHwdecMode      = std::clamp(j.value("gpuHwdecMode", 0), 0, 4);
        m_NvencEnabled      = j.value("nvencEnabled", false);
        int defaultUpscaler = defaultPerf ? 0 : (j.value("fsr", true) ? 2 : 0);
        m_UpscalerMode      = std::clamp(j.value("upscalerMode", defaultUpscaler), 0, 4);
        m_UpscalerSharpness = std::clamp(j.value("upscalerSharpness", 0.55f), 0.0f, 1.0f);
        m_SmoothMotion60    = j.value("smoothMotion60", false);
        m_Main->set_gpu_hwdec_mode(m_GpuHwdecMode);
        m_Main->set_nvenc_enabled(m_NvencEnabled);
        m_Main->set_upscaler_mode(m_UpscalerMode);
        m_Main->set_upscaler_sharpness(m_UpscalerSharpness * 100.0f);
        m_Main->set_smooth_motion_60(m_SmoothMotion60);
        m_Main->set_fsr_enabled(m_UpscalerMode > 0);

        int savedFps = j.value("targetFps", defaultPerf ? 30 : 60);
        if (savedFps != 30 && savedFps != 60 && savedFps != 120) savedFps = 60;
        m_TargetFps = savedFps;
        m_Main->set_target_fps(m_TargetFps);
        m_Player.SetMaxFps(m_TargetFps);
        int tickMs = (m_TargetFps >= 120) ? 8 : (m_TargetFps >= 60 ? 16 : 33);
        m_TickTimer = std::make_unique<slint::Timer>(
            std::chrono::milliseconds(tickMs),
            [this]() { OnTick(); }
        );
        UpdatePlayerRenderSize();
        m_ActiveThemeIdx = j.value("themeIdx", 0);
        m_EnableGlassTranslucency = j.value("enableGlassTranslucency", true);
        m_EnableGlassSheen        = j.value("enableGlassSheen", true);
        m_EnableAmbientBg         = j.value("enableAmbientBg", true);
        m_Main->set_enable_glass_translucency(m_EnableGlassTranslucency);
        m_Main->set_enable_glass_sheen(m_EnableGlassSheen);
        m_Main->set_enable_ambient_bg(m_EnableAmbientBg);
        bool showTut = j.value("showTutorialOnStart", true);
        m_Main->set_show_tutorial_on_start(showTut);
        m_Main->set_tutorial_dont_show_again(!showTut);
        m_Main->set_tutorial_modal_open(showTut);
        m_MusicScanFolders.clear();
        if (j.contains("musicScanFolders") && j["musicScanFolders"].is_array()) {
            for (const auto& dirVal : j["musicScanFolders"]) {
                if (dirVal.is_string()) {
                    std::string p = dirVal.get<std::string>();
                    if (!p.empty()) m_MusicScanFolders.push_back(p);
                }
            }
        }
        m_Main->set_music_eq_enabled(j.value("musicEqEnabled", false));
        m_Main->set_music_dynamic_eq(j.value("musicDynamicEq", false));
        m_Main->set_music_eq_preset(j.value("musicEqPreset", 0));
        m_Main->set_music_eq_60(j.value("musicEq60", 0.0f));
        m_Main->set_music_eq_230(j.value("musicEq230", 0.0f));
        m_Main->set_music_eq_910(j.value("musicEq910", 0.0f));
        m_Main->set_music_eq_3600(j.value("musicEq3600", 0.0f));
        m_Main->set_music_eq_12000(j.value("musicEq12000", 0.0f));
        if (m_Main->get_music_dynamic_eq()) {
            m_Main->set_music_dynamic_eq_status("⚡ EQ Dinámica Activa • Balance espectral adaptativo listo");
        }
        std::string savedPin = j.value("mobilePin", std::string(""));
        if (savedPin.size() == 6) {
            m_MobileServer.SetPairingPin(savedPin);
            m_Main->set_mobile_pin(slint::SharedString(savedPin));
        }

        if (j.contains("pads") && j["pads"].is_array()) {
            for (size_t i = 0; i < std::min<size_t>(8, j["pads"].size()); ++i) {
                const auto& jp = j["pads"][i];
                m_Pads[i].assigned     = jp.value("assigned", false);
                m_Pads[i].name         = jp.value("name", "Pad " + std::to_string(i + 1));
                m_Pads[i].text         = jp.value("text", "");
                m_Pads[i].ref          = jp.value("ref", "");
                m_Pads[i].bgPath       = jp.value("bgPath", "");
                m_Pads[i].bgIsImage    = jp.value("bgIsImage", false);
                m_Pads[i].bgIsVideo    = jp.value("bgIsVideo", false);
                m_Pads[i].textSize     = jp.value("textSize", 60.0f);
                m_Pads[i].textAlign    = jp.value("textAlign", 1);
                m_Pads[i].textValign   = jp.value("textValign", 1);
                m_Pads[i].textColorIdx = jp.value("textColorIdx", 0);
            }
        }
    } catch (...) {}
}

void SlintApp::SavePersistedState() {
    fs::path dir = ProyecThor::GetAppDataRoot();
    std::error_code ec;
    fs::create_directories(dir, ec);

    nlohmann::json j;
    j["textSize"]        = m_Main->get_text_size();
    j["textAlign"]       = m_Main->get_text_align();
    j["textValign"]      = m_Main->get_text_valign();
    j["textShadow"]      = m_Main->get_text_shadow();
    j["textOutline"]     = m_Main->get_text_outline();
    j["textGlow"]        = m_Main->get_text_glow();
    j["textUppercase"]   = m_Main->get_text_uppercase();
    j["textMarginH"]     = m_Main->get_text_margin_h();
    j["fontFamIdx"]      = m_Main->get_font_family_idx();
    j["fontName"]        = std::string(m_Main->get_font_family_name());
    j["indexFontName"]   = std::string(m_Main->get_index_font_family_name());
    j["fontWtIdx"]       = m_Main->get_font_weight_idx();
    j["transitionType"]  = m_Main->get_transition_type();
    j["transitionMs"]    = m_Main->get_transition_duration_ms();
    j["tilesEnabled"]    = m_Main->get_tiles_enabled();
    j["tilesLayout"]     = m_Main->get_tiles_layout();
    j["textBoxBg"]       = m_Main->get_text_box_bg();

    nlohmann::json stylesArr = nlohmann::json::array();
    for (const auto& st : m_SavedStyles) {
        stylesArr.push_back(SavedStyleToJson(st));
    }
    j["savedStyles"]     = stylesArr;
    j["styleTargetMode"] = m_StyleTargetMode;

    nlohmann::json scrStylesArr = nlohmann::json::array();
    for (const auto& sc : m_Screens) {
        scrStylesArr.push_back({
            {"id", sc.id},
            {"name", sc.name},
            {"assignedStyleIdx", sc.assignedStyleIdx},
            {"useOwnStyle", sc.useOwnStyle},
            {"style", SavedStyleToJson(sc.textStyle)}
        });
    }
    j["screenStyles"]    = scrStylesArr;
    j["textColorIdx"]    = m_Main->get_text_color_idx();
    j["fullscreen"]      = m_Main->get_fullscreen_output();
    j["projectionMonitorIdx"] = m_ProjectionMonitorIdx;
    j["fsr"]             = m_Main->get_fsr_enabled();
    j["bgScaleMode"]       = m_BgScaleMode;
    j["outputResPreset"]   = m_OutputResPreset;
    j["targetFps"]         = m_TargetFps;
    j["gpuHwdecMode"]      = m_GpuHwdecMode;
    j["nvencEnabled"]      = m_NvencEnabled;
    j["upscalerMode"]      = m_UpscalerMode;
    j["upscalerSharpness"] = m_UpscalerSharpness;
    j["smoothMotion60"]    = m_SmoothMotion60;
    j["slideColumns"]      = m_Main->get_slide_columns();
    j["slideCardSize"]   = m_Main->get_slide_card_size();
    j["performanceMode"] = m_Main->get_performance_mode();
    j["previewEnabled"]    = m_Main->get_preview_enabled();
    j["previewMaxFps"]     = m_Main->get_preview_max_fps();
    j["prioritizeOutput"]  = m_Main->get_prioritize_output();
    j["uiLanguage"]        = m_UiLanguage;
    j["themeIdx"]        = m_ActiveThemeIdx;
    j["enableGlassTranslucency"] = m_EnableGlassTranslucency;
    j["enableGlassSheen"]        = m_EnableGlassSheen;
    j["enableAmbientBg"]         = m_EnableAmbientBg;
    j["musicScanFolders"] = m_MusicScanFolders;
    j["musicEqEnabled"]   = m_Main->get_music_eq_enabled();
    j["musicDynamicEq"]   = m_Main->get_music_dynamic_eq();
    j["musicEqPreset"]    = m_Main->get_music_eq_preset();
    j["musicEq60"]        = m_Main->get_music_eq_60();
    j["musicEq230"]       = m_Main->get_music_eq_230();
    j["musicEq910"]       = m_Main->get_music_eq_910();
    j["musicEq3600"]      = m_Main->get_music_eq_3600();
    j["musicEq12000"]        = m_Main->get_music_eq_12000();
    j["showTutorialOnStart"] = m_Main->get_show_tutorial_on_start();
    j["mobilePin"]       = m_MobileServer.GetPairingPin();

    nlohmann::json padsArr = nlohmann::json::array();
    for (int i = 0; i < 8; ++i) {
        padsArr.push_back({
            {"assigned", m_Pads[i].assigned},
            {"name", m_Pads[i].name},
            {"text", m_Pads[i].text},
            {"ref", m_Pads[i].ref},
            {"bgPath", m_Pads[i].bgPath},
            {"bgIsImage", m_Pads[i].bgIsImage},
            {"bgIsVideo", m_Pads[i].bgIsVideo},
            {"textSize", m_Pads[i].textSize},
            {"textAlign", m_Pads[i].textAlign},
            {"textValign", m_Pads[i].textValign},
            {"textColorIdx", m_Pads[i].textColorIdx}
        });
    }
    j["pads"] = padsArr;

    std::ofstream out(dir / "settings_slint.json", std::ios::trunc);
    if (out.is_open()) {
        out << j.dump(2);
    }
}

void SlintApp::OnTick() {
    ++m_PerfTickFrames;

    // 1. Drain any pending video frame if event-loop wakeup coalesced
    if (m_HasVideoBg) {
        DeliverVideoFrameToOutputs();
    }

    // 1a. ImGui StreamingPanel::Update() parity — async JPEG encode on dedicated FrameEncodeWorker thread
    UpdateLanFrameCapture();

    // 1b. Poll silent low-res hover video preview frame (never interrupts live output!)
    if (m_HoverVideoActive) {
        slint::Image hFrame;
        if (m_HoverPlayer.PollFrame(hFrame)) {
            m_Main->set_hover_video_frame(hFrame);
            m_Main->set_has_hover_video_frame(true);
        }
    }

    // 1c. Animate Public Alert Banner (Marquee Letrero & Pulse, speed normalized to 30 FPS)
    if (m_HasActivePublicAlert) {
        const float fpsScale = 30.0f / static_cast<float>(std::max(1, m_TargetFps));
        int anim = m_Main->get_alert_anim_mode();
        if (anim == 1) {
            m_AlertTickerPos += 0.0042f * fpsScale;
            if (m_AlertTickerPos > 1.0f) m_AlertTickerPos = 0.0f;
            m_Main->set_alert_ticker_pos(m_AlertTickerPos);
            if (!m_AudienceFrozen) m_Projector->set_alert_ticker_pos(m_AlertTickerPos);
        } else if (anim == 3) {
            m_AlertPulsePhase += 0.14f * fpsScale;
            float alpha = 0.72f + 0.28f * std::sin(m_AlertPulsePhase);
            m_Main->set_alert_pulse_alpha(alpha);
            if (!m_AudienceFrozen) m_Projector->set_alert_pulse_alpha(alpha);
        }
    }

    // 1d. Sync Independent Music Player + Live Audio Waveform + ProyecThor Dynamic EQ
    UpdateMusicWaveformAndDynamicEq();
    if (m_MusicPlayer.IsPlaying()) {
        if (!m_Main->get_music_seeking()) {
            float pos = m_MusicPlayer.GetPosition();
            if (std::abs(pos - m_LastMusicPos) > 0.0015f) {
                m_LastMusicPos = pos;
                m_Main->set_music_position(pos);
            }
        }
        int64_t tMs  = m_MusicPlayer.GetTimeMs();
        int64_t lMs  = m_MusicPlayer.GetLengthMs();
        int64_t tSec = tMs / 1000;
        int64_t lSec = lMs / 1000;
        if (tSec != m_LastMusicSec || lSec != m_LastMusicLenSec) {
            m_LastMusicSec    = tSec;
            m_LastMusicLenSec = lSec;
            std::string mLabel = FormatTimeMs(tMs) + " / " + FormatTimeMs(lMs);
            m_Main->set_music_time_label(slint::SharedString(mLabel));
        }
        bool paused = m_MusicPlayer.IsPaused();
        if (paused != m_LastMusicPaused) {
            m_LastMusicPaused = paused;
            m_Main->set_music_paused(paused);
        }
    }
    if (m_CurrentMusicIdx >= 0 && m_MusicPlayer.ConsumeEndReached()) {
        if (!m_MusicEntries.empty()) {
            int nextIdx = m_CurrentMusicIdx + 1;
            if (nextIdx < static_cast<int>(m_MusicEntries.size())) {
                PlayMusicTrack(nextIdx);
            } else if (m_Main->get_music_loop()) {
                PlayMusicTrack(0);
            } else {
                m_CurrentMusicIdx = -1;
                m_Main->set_music_playing(false);
                RefreshMusicModel();
            }
        }
    }

    // 2. Auto-advance Video Queue when libmpv reaches natural EOF
    if (m_VideoQueuePlaying && m_Player.ConsumeEndReached()) {
        int nextIdx = m_VideoQueueCurrentIdx + 1;
        if (nextIdx < static_cast<int>(m_VideoQueue.size())) {
            PlayVideoQueueIndex(nextIdx);
        } else if (m_Main->get_video_queue_loop() && !m_VideoQueue.empty()) {
            PlayVideoQueueIndex(0);
        } else {
            m_VideoQueuePlaying    = false;
            m_VideoQueueCurrentIdx = -1;
            m_Main->set_video_queue_playing(false);
            RefreshVideoQueueModel();
        }
    }

    // 3. Sync transport position & time label (respects ScrubBar drag + throttled writes)
    if (m_Player.IsPlaying()) {
        if (!m_Main->get_media_seeking()) {
            float pos = m_Player.GetPosition();
            if (std::abs(pos - m_LastMediaPos) > 0.0015f) {
                m_LastMediaPos = pos;
                m_Main->set_media_position(pos);
            }
        }
        int64_t tMs  = m_Player.GetTimeMs();
        int64_t lMs  = m_Player.GetLengthMs();
        int64_t tSec = tMs / 1000;
        int64_t lSec = lMs / 1000;
        if (tSec != m_LastMediaSec || lSec != m_LastMediaLenSec) {
            m_LastMediaSec    = tSec;
            m_LastMediaLenSec = lSec;
            std::string label = FormatTimeMs(tMs) + " / " + FormatTimeMs(lMs);
            m_Main->set_media_time_label(slint::SharedString(label));
        }
        bool paused = m_Player.IsPaused();
        if (paused != m_LastMediaPaused) {
            m_LastMediaPaused = paused;
            m_Main->set_media_paused(paused);
        }
    }

    // 3. Clock & Custom Overlay Timers (update every ~1 second = m_TargetFps ticks)
    const int oneSecTicks = std::max(1, m_TargetFps);
    if (++m_TickCounter >= oneSecTicks) {
        m_TickCounter = 0;

        auto tickConfig = [](CustomOverlayConfig& ov) {
            if (!ov.showClock || !ov.timerRunning) return;
            if (ov.clockMode == 1) {
                if (ov.currentSec > 0) --ov.currentSec;
                if (ov.currentSec == 0) ov.timerRunning = false;
            } else if (ov.clockMode == 2) {
                ++ov.currentSec;
            }
        };

        for (auto& cov : m_CustomOverlays) tickConfig(cov);
        if (m_OverlayEditorOpen) tickConfig(m_EditingOverlay);
        for (auto& sc : m_Screens) {
            if (sc.hasCustomOverlay) tickConfig(sc.customOverlay);
        }

        SyncClockOverlay();
        if (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size()) &&
            m_Screens[m_InspectedScreenIdx].hasCustomOverlay) {
            RefreshActiveLayersModel();
        }
        if (m_ActiveTab == 14) {
            RefreshScreenMatrixModel();
        }

        if (m_ChatServer.IsRunning()) {
            RefreshChatLog();
        }
    }

    SampleAppTelemetry();
}

void SlintApp::ScanSongs() {
    std::vector<fs::path> dirs = {
        ProyecThor::SongsPath(),
        fs::path("assets/songs"),
        fs::path("bin/assets/songs"),
        fs::path("assets/bin/assets/songs")
    };

    std::vector<std::string> seen;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".txt", ".song" })) continue;
            std::string stem = PathToUtf8(entry.path().stem());
            if (std::find(seen.begin(), seen.end(), stem) != seen.end()) continue;
            seen.push_back(stem);

            std::string title, author;
            ReadSongMeta(PathToUtf8(entry.path()), title, author);

            MediaEntry m;
            m.title    = title;
            m.author   = author;
            std::string folder = PathToUtf8(entry.path().parent_path().filename());
            m.subtitle = !author.empty() ? author : (folder != "songs" ? folder : "Sin autor");
            m.path     = PathToUtf8(entry.path());
            m_AllEntries.push_back(std::move(m));
        }
    }

    std::sort(m_AllEntries.begin(), m_AllEntries.end(),
              [](const MediaEntry& a, const MediaEntry& b) { return a.title < b.title; });
}

void SlintApp::ScanBibles() {
    m_BiblePaths.clear();

    std::vector<fs::path> dirs = {
        ProyecThor::BiblesPath(),
        fs::path("assets/bibles"),
        fs::path("bin/assets/bibles"),
        fs::path("assets/bin/assets/bibles")
    };

    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".xml" })) continue;
            std::string p = PathToUtf8(entry.path());
            if (std::find(m_BiblePaths.begin(), m_BiblePaths.end(), p) != m_BiblePaths.end()) continue;
            m_BiblePaths.push_back(p);
        }
    }

    std::sort(m_BiblePaths.begin(), m_BiblePaths.end());

    // Populate dropdown menu with all installed Bible versions
    m_BibleVersionNamesModel->clear();
    for (const auto& p : m_BiblePaths) {
        std::string label = PathToUtf8(fs::path(p).stem());
        auto pos = label.find("Bible");
        if (pos != std::string::npos && pos > 0) label = label.substr(0, pos);
        m_BibleVersionNamesModel->push_back(slint::SharedString(label));
    }

    // Prefer SpanishRVR1960 by default if present
    if (!m_BiblePaths.empty()) {
        if (m_CurrentBibleVersionIdx >= static_cast<int>(m_BiblePaths.size())) {
            m_CurrentBibleVersionIdx = 0;
        }
        for (size_t i = 0; i < m_BiblePaths.size(); ++i) {
            if (m_BiblePaths[i].find("RVR1960") != std::string::npos && m_Bible.books.empty()) {
                m_CurrentBibleVersionIdx = static_cast<int>(i);
                break;
            }
        }
        LoadSelectedBible(m_CurrentBibleVersionIdx);
    } else {
        m_Main->set_current_bible_name("Sin Biblias");
    }
}

void SlintApp::LoadSelectedBible(int versionIdx) {
    if (versionIdx < 0 || versionIdx >= static_cast<int>(m_BiblePaths.size())) return;
    if (!ProyecThor::UI::XmlIO::LoadBible(m_BiblePaths[versionIdx], m_Bible)) return;

    m_CurrentBibleVersionIdx = versionIdx;
    m_Main->set_selected_bible_version_idx(versionIdx);

    std::string shortName = PathToUtf8(fs::path(m_BiblePaths[versionIdx]).stem());
    auto pos = shortName.find("Bible");
    if (pos != std::string::npos && pos > 0) shortName = shortName.substr(0, pos);
    m_Main->set_current_bible_name(slint::SharedString(shortName));

    m_AllEntries.clear();
    for (const auto& b : m_Bible.books) {
        MediaEntry m;
        m.title    = b.name;
        m.subtitle = std::to_string(b.chapters.size()) + " capítulos";
        m.path     = b.name;
        m_AllEntries.push_back(std::move(m));
    }
    m_CurrentBibleBookIdx = -1;
    RefreshBibleBooks();
    if (!m_BibleFilteredBookIndices.empty()) {
        m_Main->set_selected_bible_book_idx(0);
        SelectBibleBook(0);
    }
}

static void ParseBibleCitation(const std::string& qLower, std::string& outBookPart, int& outChap, int& outVerse) {
    outBookPart.clear();
    outChap = -1;
    outVerse = -1;

    std::string tmp = qLower;
    for (char& ch : tmp) {
        if (ch == ':' || ch == ',' || ch == '.') ch = ' ';
    }
    std::istringstream iss(tmp);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);

    std::vector<int> nums;
    while (!tokens.empty() && std::all_of(tokens.back().begin(), tokens.back().end(),
                                          [](unsigned char c) { return std::isdigit(c); })) {
        try { nums.insert(nums.begin(), std::stoi(tokens.back())); } catch (...) {}
        tokens.pop_back();
        if (nums.size() == 2) break;
    }
    if (!tokens.empty() && !nums.empty()) {
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) outBookPart += " ";
            outBookPart += tokens[i];
        }
        outChap  = nums[0];
        outVerse = (nums.size() >= 2) ? nums[1] : -1;
    }
}

void SlintApp::RefreshBibleBooks(const std::string& bookFilter) {
    m_BibleBooksModel->clear();
    m_BibleFilteredBookIndices.clear();
    int filter = m_Main->get_bible_testament_filter(); // 0=Todos, 1=AT, 2=NT

    std::string q = ToLowerUtf8(bookFilter);
    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back() == ' ')  q.pop_back();
    std::string bookPart;
    int cNum = -1, vNum = -1;
    ParseBibleCitation(q, bookPart, cNum, vNum);
    const std::string queryBook = (!bookPart.empty() && cNum >= 1) ? bookPart : q;

    std::vector<int> prefixMatches;
    std::vector<int> containsMatches;
    for (int i = 0; i < static_cast<int>(m_Bible.books.size()); ++i) {
        const auto& b = m_Bible.books[i];
        bool isNT = (b.canonicalNumber >= 40) || (b.canonicalNumber == 0 && i >= 39);
        if (filter == 1 && isNT)  continue;
        if (filter == 2 && !isNT) continue;

        if (queryBook.empty()) {
            prefixMatches.push_back(i);
            continue;
        }
        std::string bLow = ToLowerUtf8(b.name);
        if (bLow.find(queryBook) == 0) {
            prefixMatches.push_back(i);
        } else if (bLow.find(queryBook) != std::string::npos) {
            containsMatches.push_back(i);
        }
    }

    auto appendBook = [&](int i) {
        const auto& b = m_Bible.books[i];
        bool isNT = (b.canonicalNumber >= 40) || (b.canonicalNumber == 0 && i >= 39);
        m_BibleFilteredBookIndices.push_back(i);
        BibleBookItem item;
        item.name     = slint::SharedString(b.name);
        item.chapters = static_cast<int>(b.chapters.size());
        item.is_nt    = isNT;
        m_BibleBooksModel->push_back(item);
    };
    for (int idx : prefixMatches)   appendBook(idx);
    for (int idx : containsMatches) appendBook(idx);
}

void SlintApp::SelectBibleBook(int filteredBookIdx) {
    if (filteredBookIdx < 0 || filteredBookIdx >= static_cast<int>(m_BibleFilteredBookIndices.size())) return;
    int realBookIdx = m_BibleFilteredBookIndices[filteredBookIdx];
    if (realBookIdx < 0 || realBookIdx >= static_cast<int>(m_Bible.books.size())) return;

    m_CurrentBibleBookIdx = realBookIdx;
    int chapCount = static_cast<int>(m_Bible.books[realBookIdx].chapters.size());
    m_Main->set_chapter_count(std::max(1, chapCount));
    m_Main->set_selected_chapter(1);

    m_BibleChaptersModel->clear();
    for (int c = 1; c <= chapCount; ++c) {
        m_BibleChaptersModel->push_back(c);
    }

    LoadBibleChapter(realBookIdx, 1);
}

void SlintApp::OnBibleSearchEdited(const std::string& rawQuery) {
    if (m_Bible.books.empty()) return;
    std::string q = ToLowerUtf8(rawQuery);
    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back() == ' ')  q.pop_back();

    m_Main->set_bible_submode(0);
    if (q.empty()) {
        RefreshBibleBooks();
        if (!m_BibleFilteredBookIndices.empty()) {
            m_Main->set_selected_bible_book_idx(0);
            SelectBibleBook(0);
        }
        return;
    }

    std::string bookPart;
    int chapNum = -1, verseNum = -1;
    ParseBibleCitation(q, bookPart, chapNum, verseNum);

    if (!bookPart.empty() && chapNum >= 1) {
        m_Main->set_bible_testament_filter(0);
        RefreshBibleBooks(bookPart);
        if (!m_BibleFilteredBookIndices.empty()) {
            m_Main->set_selected_bible_book_idx(0);
            SelectBibleBook(0);
            int realBookIdx = m_BibleFilteredBookIndices[0];
            int clampedChap = std::clamp(chapNum, 1, static_cast<int>(m_Bible.books[realBookIdx].chapters.size()));
            m_Main->set_selected_chapter(clampedChap);
            LoadBibleChapter(realBookIdx, clampedChap);
            if (verseNum >= 1) {
                int vIdx = std::clamp(verseNum - 1, 0, std::max(0, static_cast<int>(m_SlidesModel->row_count()) - 1));
                m_Main->set_active_slide_index(vIdx);
            }
        }
        return;
    }

    RefreshBibleBooks(q);
    if (!m_BibleFilteredBookIndices.empty()) {
        m_Main->set_selected_bible_book_idx(0);
        SelectBibleBook(0);
    }
}

void SlintApp::SearchBible(const std::string& rawQuery) {
    std::string q = ToLowerUtf8(rawQuery);
    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back() == ' ')  q.pop_back();
    if (q.empty() || m_Bible.books.empty()) return;

    // 1. Try citation parse: e.g. "juan 3:16", "gn 1 1", "sal 23", "1 cor 13:4"
    std::string bookPart;
    int chapNum = -1, verseNum = -1;
    ParseBibleCitation(q, bookPart, chapNum, verseNum);

    if (!bookPart.empty() && chapNum >= 1) {
        m_Main->set_bible_testament_filter(0);
        RefreshBibleBooks(bookPart);
        if (!m_BibleFilteredBookIndices.empty()) {
            m_Main->set_bible_submode(0);
            m_Main->set_selected_bible_book_idx(0);
            SelectBibleBook(0);
            int realBookIdx = m_BibleFilteredBookIndices[0];
            int clampedChap = std::clamp(chapNum, 1, static_cast<int>(m_Bible.books[realBookIdx].chapters.size()));
            m_Main->set_selected_chapter(clampedChap);
            LoadBibleChapter(realBookIdx, clampedChap);
            if (verseNum >= 1) {
                int vIdx = std::clamp(verseNum - 1, 0, std::max(0, static_cast<int>(m_SlidesModel->row_count()) - 1));
                m_Main->set_active_slide_index(vIdx);
                ProjectSlide(vIdx);
            }
            return;
        }
    }

    // 2. Full-text word search across the entire loaded Bible (up to 80 matches)
    m_Main->set_bible_submode(0);
    m_SlidesModel->clear();
    m_Main->set_active_slide_index(-1);
    int found = 0;
    for (const auto& b : m_Bible.books) {
        for (const auto& ch : b.chapters) {
            for (const auto& v : ch.verses) {
                if (ToLowerUtf8(v.text).find(q) != std::string::npos) {
                    SlideItem s;
                    s.badge    = slint::SharedString(b.name + " " + std::to_string(ch.number) + ":" + std::to_string(v.number));
                    s.body     = slint::SharedString(v.text);
                    s.ref_text = s.badge;
                    m_SlidesModel->push_back(s);
                    if (++found >= 80) break;
                }
            }
            if (found >= 80) break;
        }
        if (found >= 80) break;
    }
    m_Main->set_selected_item_title(slint::SharedString(
        "Resultados para \"" + rawQuery + "\" (" + std::to_string(found) + ")"));
}

void SlintApp::PushBibleHistory(const std::string& badge, const std::string& body, const std::string& ref) {
    if (body.empty()) return;
    // Avoid consecutive duplicate
    if (m_BibleHistoryModel->row_count() > 0) {
        if (auto first = m_BibleHistoryModel->row_data(0)) {
            if (std::string(first->badge) == badge) return;
        }
    }
    SlideItem s;
    s.badge    = slint::SharedString(badge);
    s.body     = slint::SharedString(body);
    s.ref_text = slint::SharedString(ref);
    m_BibleHistoryModel->insert(0, s);
    while (m_BibleHistoryModel->row_count() > 40) {
        m_BibleHistoryModel->erase(m_BibleHistoryModel->row_count() - 1);
    }
}

void SlintApp::AddCurrentVerseToFavorites() {
    int idx = m_Main->get_active_slide_index();
    if (idx < 0 || idx >= static_cast<int>(m_SlidesModel->row_count())) return;
    auto slide = m_SlidesModel->row_data(idx);
    if (!slide.has_value()) return;
    for (size_t i = 0; i < m_BibleFavoritesModel->row_count(); ++i) {
        if (auto f = m_BibleFavoritesModel->row_data(i)) {
            if (f->badge == slide->badge) return;
        }
    }
    m_BibleFavoritesModel->insert(0, *slide);
}

// ── Media Preview & Video Queue (MonitorQueueEngine parity) ──────────────────
void SlintApp::ProjectSelectedMedia() {
    if (!m_HasSelectedPreviewEntry) return;
    const auto& entry = m_SelectedPreviewEntry;

    if (entry.isOverlay) {
        m_ActiveOverlayPath = entry.path;
        auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
        slint::Image thumb;
        bool hasThumb = LoadOrCreateThumbnail(entry.path, false, thumb);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].overlayName     = entry.title;
            m_Screens[i].overlayPath     = entry.path;
            m_Screens[i].hasOverlayImage = true;
            m_Screens[i].overlayImage    = img;
            m_Screens[i].hasOverlayThumb = hasThumb;
            if (hasThumb) m_Screens[i].overlayThumb = thumb;
        }
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
        SyncScreenOutputs();
    } else if (entry.isImage) {
        m_VideoQueuePlaying = false;
        m_Main->set_video_queue_playing(false);
        m_CurrentBgPath = entry.path;
        slint::Image img, thumb;
        RenderScaledStaticImage(entry.path, img);
        bool hasThumb = LoadOrCreateThumbnail(entry.path, false, thumb);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = entry.title;
            m_Screens[i].bgPath     = entry.path;
            m_Screens[i].hasBgImage = true;
            m_Screens[i].bgIsStatic = true;
            m_Screens[i].bgIsVideo  = false;
            m_Screens[i].bgImage    = img;
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        bool anyVid = false;
        for (const auto& sc : m_Screens) if (sc.bgIsVideo) { anyVid = true; break; }
        if (!anyVid) { m_Player.Stop(); m_HasVideoBg = false; }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        if (IsScreenTargeted(1) && IsLanRunning()) UpdateLanStaticFrame(img);
        SyncScreenOutputs();
    } else {
        bool isBg = (m_ActiveTab == 3);
        if (!isBg) {
            m_VideoQueuePlaying = false;
            m_Main->set_video_queue_playing(false);
        }
        // Stop hover video decoder & free static image RGBA cache before playing main video
        StopHoverVideoPreview();
        m_StaticSrcPath.clear();
        m_StaticSrcRgba.clear();
        m_StaticSrcRgba.shrink_to_fit();
        m_StaticSrcW = 0;
        m_StaticSrcH = 0;
        m_CurrentBgPath  = entry.path;
        m_HasVideoBg     = entry.isVideo;
        slint::Image thumb;
        bool hasThumb = LoadOrCreateThumbnail(entry.path, entry.isVideo, thumb);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = entry.title;
            m_Screens[i].bgPath     = entry.path;
            m_Screens[i].bgIsStatic = false;
            m_Screens[i].bgIsVideo  = entry.isVideo;
            if (i > 0 && i != m_InspectedScreenIdx) m_Screens[i].bgImage = slint::Image();
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        m_Main->set_media_loop(isBg);
        m_Player.Play(entry.path, isBg, isBg);
        ApplyShaders();
        m_Main->set_media_paused(false);
        m_Main->set_media_muted(isBg);
        SyncScreenOutputs();
    }
}

void SlintApp::AddSelectedToVideoQueue() {
    if (!m_HasSelectedPreviewEntry) return;
    VideoQueueEntry e;
    e.title   = m_SelectedPreviewEntry.title;
    e.path    = m_SelectedPreviewEntry.path;
    e.kind    = m_SelectedPreviewEntry.subtitle;
    e.isImage = m_SelectedPreviewEntry.isImage;
    m_VideoQueue.push_back(std::move(e));
    m_Main->set_queue_category(1);
    RefreshVideoQueueModel();
}

void SlintApp::ToggleVideoQueuePlay() {
    if (m_VideoQueuePlaying) {
        m_VideoQueuePlaying    = false;
        m_VideoQueueCurrentIdx = -1;
        m_Main->set_video_queue_playing(false);
        m_Player.Stop();
        RefreshVideoQueueModel();
    } else if (!m_VideoQueue.empty()) {
        PlayVideoQueueIndex(0);
    }
}

void SlintApp::PrevVideoQueueItem() {
    if (m_VideoQueue.empty()) return;
    int n = static_cast<int>(m_VideoQueue.size());
    int target = (m_VideoQueueCurrentIdx <= 0)
        ? (m_Main->get_video_queue_loop() ? (n - 1) : 0)
        : (m_VideoQueueCurrentIdx - 1);
    PlayVideoQueueIndex(target);
}

void SlintApp::NextVideoQueueItem() {
    if (m_VideoQueue.empty()) return;
    int n = static_cast<int>(m_VideoQueue.size());
    int target = (m_VideoQueueCurrentIdx < 0)
        ? 0
        : ((m_VideoQueueCurrentIdx + 1 >= n)
            ? (m_Main->get_video_queue_loop() ? 0 : (n - 1))
            : (m_VideoQueueCurrentIdx + 1));
    PlayVideoQueueIndex(target);
}

void SlintApp::PlayVideoQueueIndex(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_VideoQueue.size())) return;
    m_VideoQueuePlaying    = true;
    m_VideoQueueCurrentIdx = idx;
    m_Main->set_video_queue_playing(true);

    const auto& item = m_VideoQueue[idx];
    m_CurrentBgPath = item.path;
    slint::Image thumb;
    bool hasThumb = LoadOrCreateThumbnail(item.path, !item.isImage, thumb);

    if (item.isImage) {
        slint::Image img;
        RenderScaledStaticImage(item.path, img);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = "Cola: " + item.title;
            m_Screens[i].bgPath     = item.path;
            m_Screens[i].hasBgImage = true;
            m_Screens[i].bgIsStatic = true;
            m_Screens[i].bgIsVideo  = false;
            m_Screens[i].bgImage    = img;
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        bool anyVid = false;
        for (const auto& sc : m_Screens) if (sc.bgIsVideo) { anyVid = true; break; }
        if (!anyVid) { m_Player.Stop(); m_HasVideoBg = false; }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        if (IsScreenTargeted(1) && IsLanRunning()) UpdateLanStaticFrame(img);
    } else {
        StopHoverVideoPreview();
        m_StaticSrcPath.clear();
        m_StaticSrcRgba.clear();
        m_StaticSrcRgba.shrink_to_fit();
        m_StaticSrcW = 0;
        m_StaticSrcH = 0;
        m_HasVideoBg     = true;
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = "Cola: " + item.title;
            m_Screens[i].bgPath     = item.path;
            m_Screens[i].bgIsStatic = false;
            m_Screens[i].bgIsVideo  = true;
            if (i > 0 && i != m_InspectedScreenIdx) m_Screens[i].bgImage = slint::Image();
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        m_Main->set_media_loop(false);
        m_Main->set_media_muted(false);
        m_Main->set_media_paused(false);
        m_Player.Play(item.path, false, false);
    }
    RefreshVideoQueueModel();
    SyncScreenOutputs();
}

void SlintApp::MoveVideoQueueItem(int from, int to) {
    int n = static_cast<int>(m_VideoQueue.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    std::swap(m_VideoQueue[from], m_VideoQueue[to]);
    if (m_VideoQueueCurrentIdx == from)      m_VideoQueueCurrentIdx = to;
    else if (m_VideoQueueCurrentIdx == to)   m_VideoQueueCurrentIdx = from;
    RefreshVideoQueueModel();
}

void SlintApp::RemoveVideoQueueItem(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_VideoQueue.size())) return;
    m_VideoQueue.erase(m_VideoQueue.begin() + idx);
    if (m_VideoQueueCurrentIdx == idx) {
        m_VideoQueueCurrentIdx = -1;
        m_VideoQueuePlaying    = false;
        m_Main->set_video_queue_playing(false);
    } else if (m_VideoQueueCurrentIdx > idx) {
        --m_VideoQueueCurrentIdx;
    }
    RefreshVideoQueueModel();
}

void SlintApp::ClearVideoQueue() {
    m_VideoQueue.clear();
    m_VideoQueueCurrentIdx = -1;
    m_VideoQueuePlaying    = false;
    m_Main->set_video_queue_playing(false);
    RefreshVideoQueueModel();
}

void SlintApp::RefreshVideoQueueModel() {
    m_VideoQueueModel->clear();
    for (int i = 0; i < static_cast<int>(m_VideoQueue.size()); ++i) {
        VideoQueueItem item;
        item.title   = slint::SharedString(m_VideoQueue[i].title);
        item.path    = slint::SharedString(m_VideoQueue[i].path);
        item.kind    = slint::SharedString(m_VideoQueue[i].kind);
        item.playing = (m_VideoQueuePlaying && i == m_VideoQueueCurrentIdx);
        m_VideoQueueModel->push_back(item);
    }
}

void SlintApp::LoadBibleChapter(int bookIdx, int chapterOneBased) {
    if (bookIdx < 0 || bookIdx >= static_cast<int>(m_Bible.books.size())) return;
    const auto& book = m_Bible.books[bookIdx];
    int chapIdx = std::clamp(chapterOneBased - 1, 0, std::max(0, static_cast<int>(book.chapters.size()) - 1));
    if (chapIdx >= static_cast<int>(book.chapters.size())) return;

    m_CurrentBibleChapter = chapIdx + 1;
    const auto& chap = book.chapters[chapIdx];
    m_Main->set_selected_item_title(slint::SharedString(book.name + " " + std::to_string(chap.number)));

    m_SlidesModel->clear();
    m_Main->set_active_slide_index(-1);
    const SavedTextStyle activeSt = !m_Screens.empty() ? m_Screens[0].textStyle : SavedTextStyle{};
    const bool autoSplit = (activeSt.textFitMode == 2);

    auto splitVerseToBox = [&activeSt](const std::string& text) -> std::vector<std::string> {
        const float boxW = std::clamp(activeSt.lyricsWidth, 0.15f, 1.0f) * 1280.0f;
        const float boxH = std::clamp(activeSt.lyricsHeight, 0.08f, 1.0f) * 720.0f;
        const float fSz  = std::max(14.0f, activeSt.textSize);
        const int charsPerLine = std::max(12, static_cast<int>(boxW / (fSz * 0.56f)));
        const int maxLines     = std::max(1,  static_cast<int>(boxH / (fSz * 1.32f)));
        const size_t maxChars  = static_cast<size_t>(charsPerLine * maxLines);
        if (text.size() <= maxChars) return { text };

        std::vector<std::string> chunks;
        size_t pos = 0;
        while (pos < text.size()) {
            if (text.size() - pos <= maxChars) {
                chunks.push_back(text.substr(pos));
                break;
            }
            size_t end = pos + maxChars;
            size_t cut = std::string::npos;
            // Prefer sentence or clause punctuation, then space
            for (size_t i = end; i > pos + maxChars / 2; --i) {
                char c = text[i - 1];
                if (c == '.' || c == ';' || c == ':' || c == '?' || c == '!') { cut = i; break; }
            }
            if (cut == std::string::npos) {
                for (size_t i = end; i > pos + maxChars / 3; --i) {
                    if (text[i - 1] == ',' || text[i - 1] == ' ') { cut = i; break; }
                }
            }
            if (cut == std::string::npos) cut = end;
            std::string part = text.substr(pos, cut - pos);
            while (!part.empty() && part.back() == ' ') part.pop_back();
            if (!part.empty()) chunks.push_back(std::move(part));
            pos = cut;
            while (pos < text.size() && text[pos] == ' ') ++pos;
        }
        return chunks.empty() ? std::vector<std::string>{ text } : chunks;
    };

    for (const auto& v : chap.verses) {
        std::string baseRef = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(v.number);
        if (!autoSplit) {
            SlideItem s;
            s.badge    = slint::SharedString(baseRef);
            s.body     = slint::SharedString(v.text);
            s.ref_text = s.badge;
            m_SlidesModel->push_back(s);
        } else {
            auto parts = splitVerseToBox(v.text);
            for (size_t pi = 0; pi < parts.size(); ++pi) {
                SlideItem s;
                std::string badge = (parts.size() > 1)
                    ? (baseRef + " (" + std::to_string(pi + 1) + "/" + std::to_string(parts.size()) + ")")
                    : baseRef;
                s.badge    = slint::SharedString(badge);
                s.body     = slint::SharedString(parts[pi]);
                s.ref_text = slint::SharedString(baseRef);
                m_SlidesModel->push_back(s);
            }
        }
    }
}

void SlintApp::ScanMedia(bool backgroundsOnly) {
    std::vector<fs::path> dirs;
    std::error_code dirEc;
    if (backgroundsOnly) {
        // Carpeta EXCLUSIVA de Fondos (igual que LayersBgTab::BgRootDir en ImGui)
        fs::path bgDir = Utf8ToFsPath(ProyecThor::GetAssetsPath()) / "backgrounds";
        fs::create_directories(bgDir, dirEc);
        dirs = { bgDir };
    } else {
        // Carpetas EXCLUSIVAS de Multimedia (SOLO Videos e Imágenes — Música tiene su propio panel!)
        dirs = {
            Utf8ToFsPath(ProyecThor::VideosPath()),
            Utf8ToFsPath(ProyecThor::ImagesPath())
        };
    }

    const std::vector<std::string> videoExts = { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" };
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };

    std::vector<std::string> seen;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            bool isVid = HasExtension(entry.path(), videoExts);
            bool isImg = HasExtension(entry.path(), imageExts);
            if (!isVid && !isImg) continue;

            std::string fullPath = PathToUtf8(entry.path());
            if (std::find(seen.begin(), seen.end(), fullPath) != seen.end()) continue;
            seen.push_back(fullPath);

            MediaEntry m;
            m.title    = PathToUtf8(entry.path().stem());
            m.subtitle = isVid ? "Video" : "Imagen";
            m.path     = fullPath;
            m.isVideo  = isVid;
            m.isImage  = isImg;
            m.isAudio  = false;
            m_AllEntries.push_back(std::move(m));
        }
    }

    std::sort(m_AllEntries.begin(), m_AllEntries.end(),
              [](const MediaEntry& a, const MediaEntry& b) { return a.title < b.title; });
}

void SlintApp::ScanOverlays() {
    std::vector<fs::path> dirs = {
        fs::path(ProyecThor::OverlaysPath()),
        fs::path("assets/overlays")
    };

    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".png", ".webp" })) continue;

            MediaEntry m;
            m.title     = PathToUtf8(entry.path().stem());
            m.subtitle  = "Overlay PNG";
            m.path      = PathToUtf8(entry.path());
            m.isOverlay = true;
            m_AllEntries.push_back(std::move(m));
        }
    }
}

void SlintApp::CheckGitHubUpdatesAsync() {
    m_Main->set_update_status("Consultando GitHub Releases (TheVixcho/ProyecThor)...");
    slint::ComponentWeakHandle<MainWindow> weakMain(m_Main);

    std::thread([weakMain]() {
#ifdef _WIN32
        std::string cmd =
            "curl -fsSL -H \"User-Agent: ProyecThor\" "
            "\"https://api.github.com/repos/TheVixcho/ProyecThor/releases/latest\" 2>nul";
#else
        std::string cmd =
            "curl -fsSL -H \"User-Agent: ProyecThor\" "
            "\"https://api.github.com/repos/TheVixcho/ProyecThor/releases/latest\" 2>/dev/null";
#endif
        std::string resp = RunCommandCapture(cmd);

        std::string msg;
        if (resp.empty()) {
            msg = "No hay releases publicados aún o no se pudo conectar con github.com/TheVixcho/ProyecThor.";
        } else {
            try {
                auto j = nlohmann::json::parse(resp);
                std::string tag  = j.value("tag_name", "");
                std::string name = j.value("name", tag);
                std::string url  = j.value("html_url", "https://github.com/TheVixcho/ProyecThor/releases");
                if (tag.empty()) {
                    msg = "No se encontró ninguna versión publicada en GitHub Releases.";
                } else {
                    std::string cleanTag = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
                    if (cleanTag == PROYECTHOR_VERSION_STRING) {
                        msg = "Ya estás en la última versión (v" + std::string(PROYECTHOR_VERSION_STRING) + ").";
                    } else {
                        msg = "Nueva versión disponible: " + name + " (" + tag + ") — " + url;
                    }
                }
            } catch (...) {
                msg = "Respuesta inesperada de la API de GitHub Releases.";
            }
        }

        slint::invoke_from_event_loop([weakMain, msg]() {
            if (auto locked = weakMain.lock()) {
                (*locked)->set_update_status(slint::SharedString(msg));
            }
        });
    }).detach();
}

std::vector<std::string> SlintApp::ParseSongStanzas(const std::string& path) {
    std::vector<std::string> stanzas;
    std::ifstream f(path);
    if (!f.is_open()) return stanzas;

    std::string line, current;
    bool inHeader = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        bool blank = line.find_first_not_of(" \t") == std::string::npos;
        if (inHeader && !blank) {
            std::string low = ToLowerUtf8(line);
            if (low.rfind("autor:", 0) == 0 || low.rfind("author:", 0) == 0 ||
                low.rfind("artista:", 0) == 0 || low.rfind("titulo:", 0) == 0 ||
                low.rfind("título:", 0) == 0 || low.rfind("title:", 0) == 0) {
                continue; // skip metadata header lines so they don't project as lyrics
            }
        }
        inHeader = false;
        if (blank) {
            if (!current.empty()) {
                stanzas.push_back(current);
                current.clear();
            }
        } else {
            if (!current.empty()) current += "\n";
            current += line;
        }
    }
    if (!current.empty()) stanzas.push_back(current);
    return stanzas;
}

std::string SlintApp::FormatTimeMs(int64_t ms) {
    if (ms < 0) ms = 0;
    int64_t totalSec = ms / 1000;
    int64_t mins = totalSec / 60;
    int64_t secs = totalSec % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << mins << ":"
        << std::setw(2) << std::setfill('0') << secs;
    return oss.str();
}

std::string SlintApp::FormatCountdownSec(int totalSec) {
    if (totalSec < 0) totalSec = 0;
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << mins << ":"
        << std::setw(2) << std::setfill('0') << secs;
    return oss.str();
}

std::string SlintApp::ToLowerUtf8(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// ── 14 Themes migrated from ImGui ────────────────────────────────────────────
void SlintApp::ApplyThemePreset(int idx) {
    m_ActiveThemeIdx = std::clamp(idx, 0, 13);
    m_Main->set_active_theme_idx(m_ActiveThemeIdx);

    auto col = [](float r, float g, float b, float a = 1.0f) {
        auto cl = [](float v) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
        };
        return slint::Color::from_argb_uint8(cl(a), cl(r), cl(g), cl(b));
    };

    auto& th = m_Main->global<Theme>();

    struct Palette {
        float base[3], s0[3], s1[3], s2[3], s3[3];
        float acc[3], accL[3], txt[3], txtD[3];
        bool  lightTheme;
    };

    static const Palette kPalettes[14] = {
        // 0: Oscuro (Carbón + Plomo Neutro)
        {{0.031f,0.031f,0.039f},{0.067f,0.067f,0.075f},{0.090f,0.090f,0.102f},{0.129f,0.129f,0.145f},{0.173f,0.173f,0.192f},
         {0.831f,0.831f,0.847f},{0.957f,0.957f,0.961f},{0.957f,0.957f,0.961f},{0.631f,0.631f,0.667f}, false},
        // 1: Claro
        {{0.86f,0.87f,0.89f},{0.92f,0.93f,0.94f},{0.95f,0.95f,0.96f},{0.82f,0.83f,0.85f},{0.76f,0.77f,0.80f},
         {0.25f,0.34f,0.46f},{0.35f,0.45f,0.58f},{0.12f,0.13f,0.15f},{0.38f,0.39f,0.42f}, true},
        // 2: Naranja y Negro
        {{0.050f,0.045f,0.040f},{0.080f,0.065f,0.045f},{0.110f,0.085f,0.060f},{0.140f,0.105f,0.070f},{0.170f,0.130f,0.090f},
         {0.78f,0.44f,0.12f},{0.88f,0.58f,0.25f},{0.92f,0.88f,0.82f},{0.70f,0.55f,0.40f}, false},
        // 3: Jazz
        {{0.09f,0.05f,0.07f},{0.13f,0.07f,0.10f},{0.17f,0.09f,0.13f},{0.22f,0.12f,0.17f},{0.27f,0.15f,0.20f},
         {0.72f,0.55f,0.26f},{0.84f,0.68f,0.38f},{0.90f,0.86f,0.80f},{0.70f,0.56f,0.51f}, false},
        // 4: Ko-fi
        {{0.93f,0.90f,0.85f},{0.95f,0.92f,0.87f},{0.96f,0.96f,0.94f},{0.88f,0.82f,0.74f},{0.83f,0.75f,0.66f},
         {0.85f,0.42f,0.42f},{0.90f,0.55f,0.55f},{0.20f,0.13f,0.10f},{0.45f,0.35f,0.30f}, true},
        // 5: Deadlock
        {{0.045f,0.065f,0.055f},{0.060f,0.095f,0.080f},{0.080f,0.125f,0.105f},{0.100f,0.155f,0.130f},{0.130f,0.195f,0.160f},
         {0.42f,0.70f,0.32f},{0.55f,0.80f,0.45f},{0.85f,0.90f,0.86f},{0.58f,0.70f,0.62f}, false},
        // 6: Galaxia
        {{0.045f,0.038f,0.075f},{0.065f,0.050f,0.125f},{0.090f,0.068f,0.170f},{0.120f,0.092f,0.220f},{0.155f,0.118f,0.280f},
         {0.55f,0.38f,0.80f},{0.68f,0.52f,0.90f},{0.88f,0.86f,0.94f},{0.62f,0.58f,0.75f}, false},
        // 7: Mek (Catppuccin Mocha)
        {{0.118f,0.118f,0.180f},{0.165f,0.169f,0.235f},{0.192f,0.196f,0.267f},{0.271f,0.278f,0.353f},{0.345f,0.357f,0.439f},
         {0.537f,0.706f,0.980f},{0.706f,0.745f,0.996f},{0.804f,0.839f,0.957f},{0.651f,0.678f,0.784f}, false},
        // 8: Cyberpunk Neón
        {{0.043f,0.047f,0.086f},{0.071f,0.075f,0.141f},{0.102f,0.110f,0.200f},{0.141f,0.149f,0.271f},{0.180f,0.192f,0.349f},
         {0.000f,0.920f,0.980f},{1.000f,0.150f,0.550f},{0.900f,1.000f,1.000f},{0.480f,0.600f,0.670f}, false},
        // 9: Emerald Studio
        {{0.027f,0.063f,0.051f},{0.047f,0.094f,0.078f},{0.071f,0.141f,0.118f},{0.102f,0.188f,0.157f},{0.133f,0.251f,0.212f},
         {0.063f,0.725f,0.506f},{0.204f,0.827f,0.600f},{0.900f,0.970f,0.940f},{0.430f,0.610f,0.530f}, false},
        // 10: Crimson Velvet
        {{0.059f,0.024f,0.031f},{0.090f,0.039f,0.055f},{0.133f,0.063f,0.082f},{0.180f,0.086f,0.110f},{0.239f,0.110f,0.145f},
         {0.937f,0.267f,0.267f},{0.973f,0.443f,0.443f},{1.000f,0.900f,0.900f},{0.650f,0.420f,0.450f}, false},
        // 11: Midnight Blue
        {{0.020f,0.031f,0.067f},{0.039f,0.059f,0.122f},{0.063f,0.090f,0.180f},{0.094f,0.133f,0.247f},{0.129f,0.176f,0.322f},
         {0.220f,0.741f,0.973f},{0.490f,0.827f,0.988f},{0.878f,0.949f,0.996f},{0.450f,0.540f,0.650f}, false},
        // 12: Amethyst Violet
        {{0.039f,0.020f,0.071f},{0.067f,0.035f,0.122f},{0.106f,0.059f,0.180f},{0.149f,0.086f,0.251f},{0.200f,0.118f,0.329f},
         {0.659f,0.333f,0.969f},{0.753f,0.518f,0.988f},{0.953f,0.910f,1.000f},{0.576f,0.463f,0.659f}, false},
        // 13: Titanium Silver
        {{0.055f,0.059f,0.071f},{0.078f,0.086f,0.102f},{0.106f,0.118f,0.141f},{0.137f,0.153f,0.184f},{0.176f,0.196f,0.235f},
         {0.620f,0.680f,0.760f},{0.820f,0.860f,0.900f},{0.945f,0.961f,0.976f},{0.500f,0.560f,0.640f}, false},
    };

    const auto& p = kPalettes[m_ActiveThemeIdx];
    const float aS0 = m_EnableGlassTranslucency ? 0.84f : 1.0f;
    const float aS1 = m_EnableGlassTranslucency ? (p.lightTheme ? 0.80f : 0.78f) : 1.0f;
    const float aS2 = m_EnableGlassTranslucency ? (p.lightTheme ? 0.84f : 0.82f) : 1.0f;
    const float aS3 = m_EnableGlassTranslucency ? 0.88f : 1.0f;
    const float aGl = m_EnableGlassTranslucency ? (p.lightTheme ? 0.80f : 0.78f) : 1.0f;

    th.set_bg_root(col(p.base[0], p.base[1], p.base[2]));
    th.set_bg_surface_0(col(p.s0[0], p.s0[1], p.s0[2], aS0));
    th.set_bg_surface_1(col(p.s1[0], p.s1[1], p.s1[2], aS1));
    th.set_bg_surface_2(col(p.s2[0], p.s2[1], p.s2[2], aS2));
    th.set_bg_surface_hover(col(p.s3[0], p.s3[1], p.s3[2], aS3));
    th.set_bg_glass(col(p.s0[0], p.s0[1], p.s0[2], aGl));
    th.set_bg_input(col(p.base[0], p.base[1], p.base[2], m_EnableGlassTranslucency ? 0.85f : 1.0f));
    th.set_accent(col(p.acc[0], p.acc[1], p.acc[2]));
    th.set_accent_strong(col(p.accL[0], p.accL[1], p.accL[2]));
    th.set_accent_soft(col(p.acc[0], p.acc[1], p.acc[2], 0.20f));
    th.set_border_accent(col(p.acc[0], p.acc[1], p.acc[2], 0.62f));
    th.set_text_primary(col(p.txt[0], p.txt[1], p.txt[2]));
    th.set_text_secondary(col(p.txtD[0], p.txtD[1], p.txtD[2]));

    if (p.lightTheme) {
        th.set_border_subtle(col(0, 0, 0, 0.11f));
        th.set_border_medium(col(0, 0, 0, 0.22f));
        th.set_glass_rim_top(col(1, 1, 1, m_EnableGlassSheen ? 0.72f : 0.0f));
        th.set_glass_sheen(col(1, 1, 1, m_EnableGlassSheen ? 0.42f : 0.0f));
        th.set_shadow_ambient(col(0, 0, 0, 0.14f));
        th.set_text_muted(col(0.35f, 0.35f, 0.38f));
        th.set_accent_fg(col(1, 1, 1));
    } else {
        th.set_border_subtle(col(1, 1, 1, 0.11f));
        th.set_border_medium(col(1, 1, 1, 0.21f));
        th.set_glass_rim_top(col(1, 1, 1, m_EnableGlassSheen ? 0.24f : 0.0f));
        th.set_glass_sheen(col(1, 1, 1, m_EnableGlassSheen ? 0.10f : 0.0f));
        th.set_shadow_ambient(col(0, 0, 0, 0.36f));
        th.set_text_muted(col(0.50f, 0.50f, 0.54f));
        // Dark text on bright accents, light text on dark/saturated accents
        float lum = 0.299f * p.acc[0] + 0.587f * p.acc[1] + 0.114f * p.acc[2];
        th.set_accent_fg(lum > 0.55f ? col(0.04f, 0.04f, 0.05f) : col(0.98f, 0.98f, 0.99f));
    }
}

// ── Import Files (Native File Dialog: Linux zenity/kdialog, Windows Win32) ───
void SlintApp::ImportFileAsync(bool forceBackgroundFolder, bool forceOverlayFolder, int explicitTargetTab) {
    int currentTab = (explicitTargetTab >= 0) ? explicitTargetTab : ((forceBackgroundFolder || forceOverlayFolder) ? 3 : m_ActiveTab);
    int currentBgSub = forceOverlayFolder ? 1 : (forceBackgroundFolder ? 0 : m_BgSubtab);

    std::thread([this, currentTab, currentBgSub]() {
        std::string selectedPath;
#ifdef _WIN32
        wchar_t fileBuf[4096] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"Todos los soportados\0*.txt;*.song;*.xml;*.pdf;*.mp4;*.mkv;*.mov;*.webm;*.avi;*.wmv;*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.mp3;*.wav;*.flac;*.ogg;*.m4a;*.opus\0Todos\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = 4096;
        ofn.lpstrTitle = L"Importar archivo a ProyecThor";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&ofn)) {
            int u8Len = WideCharToMultiByte(CP_UTF8, 0, fileBuf, -1, nullptr, 0, nullptr, nullptr);
            if (u8Len > 1) {
                selectedPath.resize(static_cast<size_t>(u8Len - 1));
                WideCharToMultiByte(CP_UTF8, 0, fileBuf, -1, selectedPath.data(), u8Len, nullptr, nullptr);
            }
        }
#elif defined(__APPLE__)
        selectedPath = RunCommandCapture(
            "osascript -e 'POSIX path of (choose file with prompt \"Importar archivo a ProyecThor\")' 2>/dev/null");
#else
        // Try zenity first, fallback to kdialog
        selectedPath = RunCommandCapture(
            "zenity --file-selection --title=\"Importar archivo a ProyecThor\" "
            "--file-filter=\"Archivos soportados | *.txt *.song *.xml *.pdf *.mp4 *.mkv *.mov *.webm *.avi *.png *.jpg *.jpeg *.webp *.mp3 *.wav *.flac *.ogg *.m4a\" "
            "--file-filter=\"Todos los archivos | *\" 2>/dev/null");
        if (selectedPath.empty()) {
            selectedPath = RunCommandCapture(
                "kdialog --getopenfilename ~ \"*.txt *.song *.xml *.pdf *.mp4 *.mkv *.mov *.webm *.png *.jpg *.jpeg *.webp *.mp3 *.wav *.flac\" 2>/dev/null");
        }
#endif
        while (!selectedPath.empty() && (selectedPath.back() == '\n' || selectedPath.back() == '\r')) {
            selectedPath.pop_back();
        }
        if (selectedPath.empty()) return;

        fs::path src = Utf8ToFsPath(selectedPath);
        std::error_code ec;
        if (!fs::exists(src, ec) || !fs::is_regular_file(src, ec)) return;

        fs::path destDir;
        int targetTab = currentTab;
        if (HasExtension(src, { ".pdf" }) || currentTab == 10) {
            destDir = Utf8ToFsPath(ProyecThor::DocumentsPath());
            targetTab = 10;
        } else if (HasExtension(src, { ".txt", ".song" })) {
            destDir = Utf8ToFsPath(ProyecThor::SongsPath());
            targetTab = 0;
        } else if (HasExtension(src, { ".xml" })) {
            destDir = Utf8ToFsPath(ProyecThor::BiblesPath());
            targetTab = 1;
        } else if (currentTab == 3 && currentBgSub == 1 && HasExtension(src, { ".png", ".webp" })) {
            destDir = Utf8ToFsPath(ProyecThor::OverlaysPath());
            targetTab = 3;
        } else if (HasExtension(src, { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" })) {
            destDir = (currentTab == 3)
                ? (Utf8ToFsPath(ProyecThor::GetAssetsPath()) / "backgrounds" / Utf8ToFsPath(m_BgCurrentRelDir))
                : Utf8ToFsPath(ProyecThor::VideosPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".png", ".jpg", ".jpeg", ".webp", ".bmp" })) {
            destDir = (currentTab == 3)
                ? (Utf8ToFsPath(ProyecThor::GetAssetsPath()) / "backgrounds" / Utf8ToFsPath(m_BgCurrentRelDir))
                : Utf8ToFsPath(ProyecThor::ImagesPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".opus" })) {
            destDir = Utf8ToFsPath(ProyecThor::GetAssetsPath()) / "audio";
            targetTab = 9;
        } else {
            destDir = Utf8ToFsPath(ProyecThor::DocumentsPath());
        }

        fs::create_directories(destDir, ec);
        fs::path destFile = destDir / src.filename();
        fs::copy_file(src, destFile, fs::copy_options::overwrite_existing, ec);

        slint::invoke_from_event_loop([this, targetTab]() {
            m_CachedTabEntries.erase(targetTab);
            RefreshBgDock();
            RefreshOverlayLayers();
            if (targetTab == 9) {
                ScanMusicTracks();
            } else if (targetTab == 0 || targetTab == 1 || targetTab == 2 || targetTab == 10) {
                if (targetTab == 1) m_BiblePaths.clear();
                m_ActiveTab = targetTab;
                m_Main->set_active_tab(targetTab);
                LoadTabData(targetTab);
            }
        });
    }).detach();
}

// ── Bottom Backgrounds Dock, Thumbnails & Legacy ImGui Launcher ──────────────
bool SlintApp::LoadOrCreateThumbnail(const std::string& path, bool isVideo, slint::Image& outImg) {
    if (path.empty()) return false;
    auto it = m_ThumbCache.find(path);
    if (it != m_ThumbCache.end()) {
        outImg = it->second;
        return true;
    }
    std::error_code ec;
    if (!fs::exists(Utf8ToFsPath(path), ec)) return false;

    // Bounded thumbnail cache so 4GB RAM systems never bloat (while avoiding thrashing on ~130+ video libraries)
    const size_t maxThumbCache = m_Main->get_performance_mode() ? 64 : 256;
    if (m_ThumbCache.size() >= maxThumbCache) {
        m_ThumbCache.clear();
    }

    if (!isVideo) {
        // Resize large static images to lightweight thumbnail (240x135 in Ultra-Lite, 320x180 normal)
        const int maxTw = m_Main->get_performance_mode() ? 240 : 320;
        const int maxTh = m_Main->get_performance_mode() ? 135 : 180;
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (data && w > 0 && h > 0) {
            if (w > maxTw || h > maxTh) {
                double sc = std::min(static_cast<double>(maxTw) / w, static_cast<double>(maxTh) / h);
                int tw = std::max(1, static_cast<int>(std::round(w * sc)));
                int th = std::max(1, static_cast<int>(std::round(h * sc)));
                slint::SharedPixelBuffer<slint::Rgba8Pixel> tb(
                    static_cast<uint32_t>(tw), static_cast<uint32_t>(th));
                stbir_resize_uint8_srgb(
                    data, w, h, w * 4,
                    reinterpret_cast<uint8_t*>(tb.begin()), tw, th, tw * 4,
                    STBIR_RGBA);
                stbi_image_free(data);
                outImg = slint::Image(tb);
            } else {
                slint::SharedPixelBuffer<slint::Rgba8Pixel> tb(
                    static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                std::memcpy(tb.begin(), data, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
                stbi_image_free(data);
                outImg = slint::Image(tb);
            }
        } else {
            if (data) stbi_image_free(data);
            outImg = slint::Image::load_from_path(slint::SharedString(path));
        }
        m_ThumbCache.emplace(path, outImg);
        return true;
    }

    // Cache video thumbnail via ffmpeg (uses CUDA/auto hwaccel when NVENC/GPU mode is active, with safe fallback)
    fs::path thumbDir = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails";
    fs::create_directories(thumbDir, ec);
    size_t h = std::hash<std::string>{}(path);
    fs::path thumbPath = thumbDir / ("slint_" + std::to_string(h) + ".jpg");
    const std::string hwArg = m_NvencEnabled ? "-hwaccel auto " : "";

    if (!fs::exists(thumbPath, ec)) {
#ifdef _WIN32
        std::string cmd = "ffmpeg -y " + hwArg + "-ss 00:00:01 -i \"" + path +
                          "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + PathToUtf8(thumbPath) +
                          "\" >nul 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0 || !fs::exists(thumbPath, ec)) {
            cmd = "ffmpeg -y -ss 00:00:00 -i \"" + path +
                  "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + PathToUtf8(thumbPath) +
                  "\" >nul 2>&1";
            std::system(cmd.c_str());
        }
#else
        std::string cmd = "ffmpeg -y " + hwArg + "-ss 00:00:01 -i \"" + path +
                          "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + PathToUtf8(thumbPath) +
                          "\" >/dev/null 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0 || !fs::exists(thumbPath, ec)) {
            // Fallback at 00:00:00 for very short loops
            cmd = "ffmpeg -y -ss 00:00:00 -i \"" + path +
                  "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + PathToUtf8(thumbPath) +
                  "\" >/dev/null 2>&1";
            std::system(cmd.c_str());
        }
#endif
    }

    if (fs::exists(thumbPath, ec) && fs::file_size(thumbPath, ec) > 0) {
        outImg = slint::Image::load_from_path(slint::SharedString(PathToUtf8(thumbPath)));
        m_ThumbCache.emplace(path, outImg);
        return true;
    }
    return false;
}

void SlintApp::CreateBgSubfolder(const std::string& folderName) {
    std::string safe = SanitizeFilename(folderName);
    if (safe.empty()) return;
    fs::path rootDir = fs::path(ProyecThor::GetAssetsPath()) / "backgrounds";
    fs::path curDir  = m_BgCurrentRelDir.empty() ? rootDir : (rootDir / m_BgCurrentRelDir);
    std::error_code ec;
    fs::create_directories(curDir / safe, ec);
    RefreshBgDock();
}

void SlintApp::RefreshBgDock() {
    m_BgDockEntries.clear();
    m_BgDockModel->clear();
    m_BgFoldersModel->clear();

    // Carpeta EXCLUSIVA de Fondos con árbol de subcarpetas (como ImGui LayersBgTab)
    fs::path rootDir = fs::path(ProyecThor::GetAssetsPath()) / "backgrounds";
    std::error_code ec;
    fs::create_directories(rootDir, ec);

    fs::path curDir = m_BgCurrentRelDir.empty() ? rootDir : (rootDir / m_BgCurrentRelDir);
    if (!fs::exists(curDir, ec) || !fs::is_directory(curDir, ec)) {
        m_BgCurrentRelDir.clear();
        curDir = rootDir;
    }

    m_Main->set_bg_can_go_up(!m_BgCurrentRelDir.empty());
    m_Main->set_bg_current_folder_label(slint::SharedString(
        m_BgCurrentRelDir.empty() ? "Raíz" : ("Raíz / " + m_BgCurrentRelDir)));

    const std::vector<std::string> videoExts = { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" };
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };

    std::vector<fs::path> subdirs;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(curDir, ec)) {
        if (entry.is_directory(ec)) {
            subdirs.push_back(entry.path());
        } else if (entry.is_regular_file(ec)) {
            if (HasExtension(entry.path(), videoExts) || HasExtension(entry.path(), imageExts)) {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(subdirs.begin(), subdirs.end());
    std::sort(files.begin(), files.end());

    for (const auto& sd : subdirs) {
        int count = 0;
        for (const auto& sub : fs::recursive_directory_iterator(sd, ec)) {
            if (sub.is_regular_file(ec) &&
                (HasExtension(sub.path(), videoExts) || HasExtension(sub.path(), imageExts))) {
                ++count;
            }
        }
        BgFolderItem fi;
        fi.name     = slint::SharedString(PathToUtf8(sd.filename()));
        fi.rel_path = slint::SharedString(fs::relative(sd, rootDir, ec).generic_string());
        fi.count    = count;
        m_BgFoldersModel->push_back(fi);
    }

    for (const auto& fp : files) {
        bool isVid = HasExtension(fp, videoExts);
        bool isImg = HasExtension(fp, imageExts);
        std::string fullPath = PathToUtf8(fp);
        std::string stem     = PathToUtf8(fp.stem());

        MediaEntry m;
        m.title    = stem;
        m.subtitle = isVid ? "VIDEO LOOP" : "IMAGEN";
        m.path     = fullPath;
        m.isVideo  = isVid;
        m.isImage  = isImg;
        m_BgDockEntries.push_back(m);

        LibraryItem item;
        item.title    = slint::SharedString(m.title);
        item.subtitle = slint::SharedString(m.subtitle);
        item.path     = slint::SharedString(m.path);
        slint::Image thumb;
        item.has_thumbnail = LoadOrCreateThumbnail(fullPath, isVid, thumb);
        if (item.has_thumbnail) item.thumbnail = thumb;
        m_BgDockModel->push_back(item);
    }
}

void SlintApp::SelectDockBackground(int index) {
    if (index < 0 || index >= static_cast<int>(m_BgDockEntries.size())) return;
    const auto& entry = m_BgDockEntries[index];

    m_CurrentBgPath = entry.path;
    slint::Image thumb;
    bool hasThumb = LoadOrCreateThumbnail(entry.path, entry.isVideo, thumb);

    if (entry.isImage) {
        slint::Image img;
        RenderScaledStaticImage(entry.path, img);
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = entry.title;
            m_Screens[i].bgPath     = entry.path;
            m_Screens[i].hasBgImage = true;
            m_Screens[i].bgIsStatic = true;
            m_Screens[i].bgIsVideo  = false;
            m_Screens[i].bgImage    = img;
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        bool anyVid = false;
        for (const auto& sc : m_Screens) if (sc.bgIsVideo) { anyVid = true; break; }
        if (!anyVid) { m_Player.Stop(); m_HasVideoBg = false; }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        if (IsScreenTargeted(1) && IsLanRunning()) UpdateLanStaticFrame(img);
    } else {
        StopHoverVideoPreview();
        m_StaticSrcPath.clear();
        m_StaticSrcRgba.clear();
        m_StaticSrcRgba.shrink_to_fit();
        m_StaticSrcW = 0;
        m_StaticSrcH = 0;
        m_HasVideoBg = true;
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            m_Screens[i].bgName     = entry.title;
            m_Screens[i].bgPath     = entry.path;
            m_Screens[i].bgIsStatic = false;
            m_Screens[i].bgIsVideo  = true;
            if (i > 0 && i != m_InspectedScreenIdx) m_Screens[i].bgImage = slint::Image();
            m_Screens[i].hasBgThumb = hasThumb;
            if (hasThumb) m_Screens[i].bgThumb = thumb;
        }
        m_HasStaticImage = m_Screens[0].bgIsStatic;
        m_Main->set_media_loop(true);
        m_Main->set_media_muted(true);
        m_Main->set_media_paused(false);
        m_Player.Play(entry.path, true, true);
        ApplyShaders();
    }
    SyncScreenOutputs();
}

std::string SlintApp::FormatOverlayClockText(const CustomOverlayConfig& cfg) const {
    if (!cfg.showClock) return "";
    if (cfg.clockMode == 0) {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        char buf[32];
        switch (cfg.timeFormat) {
            case 1: std::strftime(buf, sizeof(buf), "%H:%M", &localTm); break;
            case 2: std::strftime(buf, sizeof(buf), "%I:%M %p", &localTm); break;
            case 3: std::strftime(buf, sizeof(buf), "%I:%M:%S %p", &localTm); break;
            default: std::strftime(buf, sizeof(buf), "%H:%M:%S", &localTm); break;
        }
        return std::string(buf);
    }

    int sec = std::max(0, cfg.currentSec);
    int h   = sec / 3600;
    int m   = (sec % 3600) / 60;
    int s   = sec % 60;
    char buf[32];
    if (h > 0 || cfg.timeFormat == 0 || cfg.timeFormat == 3) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    } else if (cfg.timeFormat == 1) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, s);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, s);
    }
    return std::string(buf);
}

void SlintApp::LoadCustomOverlaysFromDisk() {
    m_CustomOverlays.clear();
    fs::path dir(ProyecThor::OverlaysPath());
    std::error_code ec;
    fs::create_directories(dir, ec);

    auto writeCfgJson = [](const fs::path& fp, const CustomOverlayConfig& ov) -> bool {
        nlohmann::json j;
        j["id"]           = ov.id;
        j["name"]         = ov.name;
        j["showClock"]    = ov.showClock;
        j["clockMode"]    = ov.clockMode;
        j["timeFormat"]   = ov.timeFormat;
        j["initialSec"]   = ov.initialSec;
        j["titleText"]    = ov.titleText;
        j["subText"]      = ov.subText;
        j["textR"]        = ov.textR;
        j["textG"]        = ov.textG;
        j["textB"]        = ov.textB;
        j["bgR"]          = ov.bgR;
        j["bgG"]          = ov.bgG;
        j["bgB"]          = ov.bgB;
        j["bgOpacity"]    = ov.bgOpacity;
        j["showBorder"]   = ov.showBorder;
        j["borderR"]      = ov.borderR;
        j["borderG"]      = ov.borderG;
        j["borderB"]      = ov.borderB;
        j["fontSize"]     = ov.fontSize;
        j["borderRadius"] = ov.borderRadius;
        j["boxPadding"]   = ov.boxPadding;
        j["minBoxWidth"]  = ov.minBoxWidth;
        j["posPreset"]    = ov.posPreset;
        j["posX"]         = ov.posX;
        j["posY"]         = ov.posY;
        std::ofstream out(fp, std::ios::binary);
        if (!out.is_open()) return false;
        out << j.dump(2);
        return out.good();
    };

    std::vector<fs::path> found;
    if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (HasExtension(entry.path(), { ".ptoverlay" })) {
                found.push_back(entry.path());
            }
        }
    }
    std::sort(found.begin(), found.end());

    // Seed 2 starter overlays only when directory has zero custom overlays yet
    if (found.empty()) {
        CustomOverlayConfig liveClk;
        liveClk.id         = "reloj_en_vivo";
        liveClk.name       = "Reloj en Vivo";
        liveClk.showClock  = true;
        liveClk.clockMode  = 0;
        liveClk.timeFormat = 0;
        liveClk.posPreset  = 3;
        liveClk.fontSize   = 44.0f;
        fs::path p1 = dir / "Reloj_en_Vivo.ptoverlay";
        if (writeCfgJson(p1, liveClk)) found.push_back(p1);

        CustomOverlayConfig cd5;
        cd5.id         = "cuenta_regresiva_5m";
        cd5.name       = "Cuenta Regresiva 5m";
        cd5.showClock  = true;
        cd5.clockMode  = 1;
        cd5.timeFormat = 4;
        cd5.initialSec = 300;
        cd5.currentSec = 300;
        cd5.titleText  = "INICIAMOS EN";
        cd5.posPreset  = 0;
        cd5.fontSize   = 68.0f;
        cd5.borderR = 245; cd5.borderG = 158; cd5.borderB = 11;
        fs::path p2 = dir / "Cuenta_Regresiva_5m.ptoverlay";
        if (writeCfgJson(p2, cd5)) found.push_back(p2);
    }

    for (const auto& fp : found) {
        try {
            std::ifstream in(fp, std::ios::binary);
            if (!in.is_open()) continue;
            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object()) continue;

            CustomOverlayConfig ov;
            ov.path         = PathToUtf8(fp);
            ov.id           = j.value("id", PathToUtf8(fp.stem()));
            ov.name         = j.value("name", PathToUtf8(fp.stem()));
            ov.showClock    = j.value("showClock", true);
            ov.clockMode    = std::clamp(j.value("clockMode", 0), 0, 2);
            ov.timeFormat   = std::clamp(j.value("timeFormat", 0), 0, 4);
            ov.initialSec   = std::max(1, j.value("initialSec", 300));
            ov.currentSec   = (ov.clockMode == 1) ? ov.initialSec : 0;
            ov.timerRunning = false;
            ov.titleText    = j.value("titleText", "");
            ov.subText      = j.value("subText", "");
            ov.textR        = static_cast<uint8_t>(std::clamp(j.value("textR", 255), 0, 255));
            ov.textG        = static_cast<uint8_t>(std::clamp(j.value("textG", 255), 0, 255));
            ov.textB        = static_cast<uint8_t>(std::clamp(j.value("textB", 255), 0, 255));
            ov.bgR          = static_cast<uint8_t>(std::clamp(j.value("bgR", 9), 0, 255));
            ov.bgG          = static_cast<uint8_t>(std::clamp(j.value("bgG", 9), 0, 255));
            ov.bgB          = static_cast<uint8_t>(std::clamp(j.value("bgB", 11), 0, 255));
            ov.bgOpacity    = std::clamp(j.value("bgOpacity", 0.82f), 0.0f, 1.0f);
            ov.showBorder   = j.value("showBorder", true);
            ov.borderR      = static_cast<uint8_t>(std::clamp(j.value("borderR", 56), 0, 255));
            ov.borderG      = static_cast<uint8_t>(std::clamp(j.value("borderG", 189), 0, 255));
            ov.borderB      = static_cast<uint8_t>(std::clamp(j.value("borderB", 248), 0, 255));
            ov.fontSize     = std::clamp(j.value("fontSize", 42.0f), 16.0f, 280.0f);
            ov.borderRadius = std::clamp(j.value("borderRadius", 12.0f), 0.0f, 48.0f);
            ov.boxPadding   = std::clamp(j.value("boxPadding", 16.0f), 6.0f, 100.0f);
            ov.minBoxWidth  = std::clamp(j.value("minBoxWidth", 0.0f), 0.0f, 1800.0f);
            ov.posPreset    = std::clamp(j.value("posPreset", 3), 0, 7);
            ov.posX         = std::clamp(j.value("posX", 0.85f), 0.05f, 0.95f);
            ov.posY         = std::clamp(j.value("posY", 0.10f), 0.05f, 0.95f);
            m_CustomOverlays.push_back(std::move(ov));
        } catch (...) {}
    }
}

bool SlintApp::SaveCustomOverlayToDisk(CustomOverlayConfig& cfg) {
    fs::path dir(ProyecThor::OverlaysPath());
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::string cleanName = cfg.name;
    while (!cleanName.empty() && cleanName.front() == ' ') cleanName.erase(cleanName.begin());
    while (!cleanName.empty() && cleanName.back() == ' ')  cleanName.pop_back();
    if (cleanName.empty()) cleanName = "Overlay Sin Título";
    cfg.name = cleanName;

    if (cfg.path.empty()) {
        std::string stem = SanitizeFilename(cleanName);
        fs::path target = dir / (stem + ".ptoverlay");
        if (fs::exists(target, ec)) {
            for (int n = 1; n < 1000; ++n) {
                fs::path cand = dir / (stem + "_" + std::to_string(n) + ".ptoverlay");
                if (!fs::exists(cand, ec)) { target = cand; break; }
            }
        }
        cfg.path = PathToUtf8(target);
        cfg.id   = PathToUtf8(target.stem());
    }

    nlohmann::json j;
    j["id"]           = cfg.id;
    j["name"]         = cfg.name;
    j["showClock"]    = cfg.showClock;
    j["clockMode"]    = cfg.clockMode;
    j["timeFormat"]   = cfg.timeFormat;
    j["initialSec"]   = cfg.initialSec;
    j["titleText"]    = cfg.titleText;
    j["subText"]      = cfg.subText;
    j["textR"]        = cfg.textR;
    j["textG"]        = cfg.textG;
    j["textB"]        = cfg.textB;
    j["bgR"]          = cfg.bgR;
    j["bgG"]          = cfg.bgG;
    j["bgB"]          = cfg.bgB;
    j["bgOpacity"]    = cfg.bgOpacity;
    j["showBorder"]   = cfg.showBorder;
    j["borderR"]      = cfg.borderR;
    j["borderG"]      = cfg.borderG;
    j["borderB"]      = cfg.borderB;
    j["fontSize"]     = cfg.fontSize;
    j["borderRadius"] = cfg.borderRadius;
    j["boxPadding"]   = cfg.boxPadding;
    j["minBoxWidth"]  = cfg.minBoxWidth;
    j["posPreset"]    = cfg.posPreset;
    j["posX"]         = cfg.posX;
    j["posY"]         = cfg.posY;

    std::ofstream out(cfg.path, std::ios::binary);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return out.good();
}

void SlintApp::SyncOverlayEditorUiFromState() {
    char secBuf[8];
    std::snprintf(secBuf, sizeof(secBuf), "%02d", std::max(0, m_EditingOverlay.initialSec % 60));
    m_Main->set_ov_ed_name(slint::SharedString(m_EditingOverlay.name));
    m_Main->set_ov_ed_show_clock(m_EditingOverlay.showClock);
    m_Main->set_ov_ed_clock_mode(m_EditingOverlay.clockMode);
    m_Main->set_ov_ed_time_format(m_EditingOverlay.timeFormat);
    m_Main->set_ov_ed_initial_min(std::max(0, m_EditingOverlay.initialSec / 60));
    m_Main->set_ov_ed_manual_min(slint::SharedString(std::to_string(std::max(0, m_EditingOverlay.initialSec / 60))));
    m_Main->set_ov_ed_manual_sec(slint::SharedString(secBuf));
    m_Main->set_ov_ed_timer_running(m_EditingOverlay.timerRunning);
    m_Main->set_ov_ed_clock_preview(slint::SharedString(FormatOverlayClockText(m_EditingOverlay)));
    m_Main->set_ov_ed_title(slint::SharedString(m_EditingOverlay.titleText));
    m_Main->set_ov_ed_sub(slint::SharedString(m_EditingOverlay.subText));
    m_Main->set_ov_ed_text_color(slint::Color::from_rgb_uint8(m_EditingOverlay.textR, m_EditingOverlay.textG, m_EditingOverlay.textB));
    m_Main->set_ov_ed_text_r(static_cast<float>(m_EditingOverlay.textR));
    m_Main->set_ov_ed_text_g(static_cast<float>(m_EditingOverlay.textG));
    m_Main->set_ov_ed_text_b(static_cast<float>(m_EditingOverlay.textB));
    m_Main->set_ov_ed_bg_color(slint::Color::from_rgb_uint8(m_EditingOverlay.bgR, m_EditingOverlay.bgG, m_EditingOverlay.bgB));
    m_Main->set_ov_ed_bg_opacity(m_EditingOverlay.bgOpacity);
    m_Main->set_ov_ed_show_border(m_EditingOverlay.showBorder);
    m_Main->set_ov_ed_border_color(slint::Color::from_rgb_uint8(m_EditingOverlay.borderR, m_EditingOverlay.borderG, m_EditingOverlay.borderB));
    m_Main->set_ov_ed_font_size(m_EditingOverlay.fontSize);
    m_Main->set_ov_ed_radius(m_EditingOverlay.borderRadius);
    m_Main->set_ov_ed_box_padding(m_EditingOverlay.boxPadding);
    m_Main->set_ov_ed_min_width(m_EditingOverlay.minBoxWidth);
    m_Main->set_ov_ed_pos_preset(m_EditingOverlay.posPreset);
    m_Main->set_ov_ed_pos_x(m_EditingOverlay.posX);
    m_Main->set_ov_ed_pos_y(m_EditingOverlay.posY);
}

void SlintApp::SyncOverlayEditorStateFromUi() {
    m_EditingOverlay.name         = std::string(m_Main->get_ov_ed_name());
    m_EditingOverlay.showClock    = m_Main->get_ov_ed_show_clock();
    int oldMode                   = m_EditingOverlay.clockMode;
    m_EditingOverlay.clockMode    = std::clamp(m_Main->get_ov_ed_clock_mode(), 0, 2);
    if (oldMode != m_EditingOverlay.clockMode) {
        m_EditingOverlay.timerRunning = false;
        m_EditingOverlay.currentSec   = (m_EditingOverlay.clockMode == 1) ? m_EditingOverlay.initialSec : 0;
        m_Main->set_ov_ed_timer_running(false);
    }
    m_EditingOverlay.timeFormat   = std::clamp(m_Main->get_ov_ed_time_format(), 0, 4);
    m_EditingOverlay.titleText    = std::string(m_Main->get_ov_ed_title());
    m_EditingOverlay.subText      = std::string(m_Main->get_ov_ed_sub());
    m_EditingOverlay.textR        = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(m_Main->get_ov_ed_text_r())), 0, 255));
    m_EditingOverlay.textG        = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(m_Main->get_ov_ed_text_g())), 0, 255));
    m_EditingOverlay.textB        = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(m_Main->get_ov_ed_text_b())), 0, 255));
    m_EditingOverlay.bgOpacity    = std::clamp(m_Main->get_ov_ed_bg_opacity(), 0.0f, 1.0f);
    m_EditingOverlay.showBorder   = m_Main->get_ov_ed_show_border();
    m_EditingOverlay.fontSize     = std::clamp(m_Main->get_ov_ed_font_size(), 16.0f, 280.0f);
    m_EditingOverlay.borderRadius = std::clamp(m_Main->get_ov_ed_radius(), 0.0f, 48.0f);
    m_EditingOverlay.boxPadding   = std::clamp(m_Main->get_ov_ed_box_padding(), 6.0f, 100.0f);
    m_EditingOverlay.minBoxWidth  = std::clamp(m_Main->get_ov_ed_min_width(), 0.0f, 1800.0f);
    m_EditingOverlay.posPreset    = std::clamp(m_Main->get_ov_ed_pos_preset(), 0, 7);
    m_EditingOverlay.posX         = std::clamp(m_Main->get_ov_ed_pos_x(), 0.05f, 0.95f);
    m_EditingOverlay.posY         = std::clamp(m_Main->get_ov_ed_pos_y(), 0.05f, 0.95f);

    m_Main->set_ov_ed_text_color(slint::Color::from_rgb_uint8(m_EditingOverlay.textR, m_EditingOverlay.textG, m_EditingOverlay.textB));
    m_Main->set_ov_ed_clock_preview(slint::SharedString(FormatOverlayClockText(m_EditingOverlay)));
}

void SlintApp::OpenNewOverlayEditor(bool withClock) {
    m_EditingOverlayIdx = -1;
    m_EditingOverlay = CustomOverlayConfig{};
    m_EditingOverlay.name      = withClock ? ("Overlay Tiempo " + std::to_string(m_CustomOverlays.size() + 1))
                                           : ("Overlay Texto " + std::to_string(m_CustomOverlays.size() + 1));
    m_EditingOverlay.showClock = withClock;
    if (!withClock) {
        m_EditingOverlay.titleText = "Iglesia Central";
        m_EditingOverlay.subText   = "Bienvenidos al Servicio";
        m_EditingOverlay.posPreset = 4;
        m_EditingOverlay.fontSize  = 34.0f;
    }
    m_OverlayEditorOpen = true;
    m_Main->set_overlay_editor_open(true);
    m_Main->set_song_editor_open(false);
    SyncOverlayEditorUiFromState();
}

void SlintApp::OpenEditOverlayEditor(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_OverlayLayerEntries.size())) return;
    const auto& entry = m_OverlayLayerEntries[layerIndex];
    if (!entry.isCustomOverlay || entry.customOverlayIdx < 0 ||
        entry.customOverlayIdx >= static_cast<int>(m_CustomOverlays.size())) return;
    m_EditingOverlayIdx = entry.customOverlayIdx;
    m_EditingOverlay    = m_CustomOverlays[m_EditingOverlayIdx];
    m_OverlayEditorOpen = true;
    m_Main->set_overlay_editor_open(true);
    m_Main->set_song_editor_open(false);
    SyncOverlayEditorUiFromState();
}

void SlintApp::OpenEditActiveScreenOverlay() {
    int idx = std::clamp(m_InspectedScreenIdx, 0, static_cast<int>(m_Screens.size()) - 1);
    const auto& sc = m_Screens[idx];
    if (sc.hasCustomOverlay) {
        m_EditingOverlayIdx = -1;
        for (int i = 0; i < static_cast<int>(m_CustomOverlays.size()); ++i) {
            if (!sc.customOverlay.path.empty() && m_CustomOverlays[i].path == sc.customOverlay.path) {
                m_EditingOverlayIdx = i;
                break;
            }
        }
        m_EditingOverlay = sc.customOverlay;
        m_OverlayEditorOpen = true;
        m_Main->set_overlay_editor_open(true);
        m_Main->set_song_editor_open(false);
        SyncOverlayEditorUiFromState();
    } else {
        OpenNewOverlayEditor(true);
    }
}

void SlintApp::SyncOverlayTimerToAll(const CustomOverlayConfig& src) {
    if (src.path.empty()) return;
    for (auto& cov : m_CustomOverlays) {
        if (cov.path == src.path) {
            cov.initialSec   = src.initialSec;
            cov.currentSec   = src.currentSec;
            cov.timerRunning = src.timerRunning;
            cov.clockMode    = src.clockMode;
            cov.timeFormat   = src.timeFormat;
        }
    }
    for (auto& sc : m_Screens) {
        if (sc.hasCustomOverlay && sc.customOverlay.path == src.path) {
            sc.customOverlay.initialSec   = src.initialSec;
            sc.customOverlay.currentSec   = src.currentSec;
            sc.customOverlay.timerRunning = src.timerRunning;
            sc.customOverlay.clockMode    = src.clockMode;
            sc.customOverlay.timeFormat   = src.timeFormat;
        }
    }
}

void SlintApp::ApplyOverlayConfigToTargets(const CustomOverlayConfig& cfg, int explicitTargetMode) {
    int prevTarget = m_OutputTargetMode;
    if (explicitTargetMode >= 0) {
        m_OutputTargetMode = explicitTargetMode;
        m_Main->set_output_target_mode(m_OutputTargetMode);
        if (m_OutputTargetMode >= 1 && (m_OutputTargetMode - 1) < static_cast<int>(m_Screens.size())) {
            m_InspectedScreenIdx = m_OutputTargetMode - 1;
            m_Main->set_inspected_screen_idx(m_InspectedScreenIdx);
        }
    }
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].customOverlay    = cfg;
        m_Screens[i].hasCustomOverlay = true;
    }
    if (explicitTargetMode >= 0) {
        // Keep user's selected explicit target visible in monitor
    } else {
        m_OutputTargetMode = prevTarget;
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::SaveOverlayEditor(bool alsoApply) {
    SyncOverlayEditorStateFromUi();
    SaveCustomOverlayToDisk(m_EditingOverlay);

    if (m_EditingOverlayIdx >= 0 && m_EditingOverlayIdx < static_cast<int>(m_CustomOverlays.size())) {
        m_CustomOverlays[m_EditingOverlayIdx] = m_EditingOverlay;
    } else {
        m_CustomOverlays.push_back(m_EditingOverlay);
        m_EditingOverlayIdx = static_cast<int>(m_CustomOverlays.size()) - 1;
    }

    // Update any screen or queue entry referencing this overlay path
    for (auto& oq : m_OverlayQueue) {
        if (oq.isCustom && oq.path == m_EditingOverlay.path) {
            oq.title = m_EditingOverlay.name;
        }
    }
    for (auto& sc : m_Screens) {
        if (sc.hasCustomOverlay && sc.customOverlay.path == m_EditingOverlay.path) {
            sc.customOverlay = m_EditingOverlay;
        }
    }

    if (alsoApply) {
        ApplyOverlayConfigToTargets(m_EditingOverlay);
    } else {
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
        SyncClockOverlay();
        SyncScreenOutputs();
    }
    m_OverlayEditorOpen = false;
    m_Main->set_overlay_editor_open(false);
    AppendDebugLog("Overlay guardado: " + m_EditingOverlay.name);
}

void SlintApp::CancelOverlayEditor() {
    m_OverlayEditorOpen = false;
    m_Main->set_overlay_editor_open(false);
}

void SlintApp::OverlayEditorSetMinutes(int mins) {
    OverlayEditorSetManualTime(std::to_string(std::max(1, mins)), "00");
}

void SlintApp::OverlayEditorSetManualTime(const std::string& minStr, const std::string& secStr) {
    auto parseInt = [](const std::string& raw) -> int {
        int val = 0;
        bool any = false;
        for (unsigned char c : raw) {
            if (std::isdigit(c)) {
                val = std::min(val * 10 + (c - '0'), 99999);
                any = true;
            }
        }
        return any ? val : 0;
    };

    int total = 0;
    if (minStr.find(':') != std::string::npos) {
        std::vector<int> parts;
        std::stringstream ss(minStr);
        std::string tok;
        while (std::getline(ss, tok, ':')) parts.push_back(parseInt(tok));
        if (parts.size() >= 3) {
            total = parts[0] * 3600 + parts[1] * 60 + parts[2];
        } else if (parts.size() == 2) {
            total = parts[0] * 60 + parts[1];
        } else if (parts.size() == 1) {
            total = parts[0] * 60;
        }
    } else {
        int mins = std::clamp(parseInt(minStr), 0, 999);
        int secs = std::clamp(parseInt(secStr), 0, 3599);
        total = mins * 60 + secs;
    }

    total = std::clamp(total, 1, 359999);
    int outMin = total / 60;
    int outSec = total % 60;
    char secBuf[8];
    std::snprintf(secBuf, sizeof(secBuf), "%02d", outSec);

    m_EditingOverlay.initialSec = total;
    m_EditingOverlay.currentSec = total;
    m_Main->set_ov_ed_initial_min(outMin);
    m_Main->set_ov_ed_manual_min(slint::SharedString(std::to_string(outMin)));
    m_Main->set_ov_ed_manual_sec(slint::SharedString(secBuf));
    m_Main->set_ov_ed_clock_preview(slint::SharedString(FormatOverlayClockText(m_EditingOverlay)));
    SyncOverlayTimerToAll(m_EditingOverlay);
    SyncClockOverlay();
}

void SlintApp::OverlayEditorTimerToggle() {
    m_EditingOverlay.timerRunning = !m_EditingOverlay.timerRunning;
    m_Main->set_ov_ed_timer_running(m_EditingOverlay.timerRunning);
    SyncOverlayTimerToAll(m_EditingOverlay);
    SyncClockOverlay();
}

void SlintApp::OverlayEditorTimerReset() {
    m_EditingOverlay.timerRunning = false;
    m_EditingOverlay.currentSec   = (m_EditingOverlay.clockMode == 1) ? m_EditingOverlay.initialSec : 0;
    m_Main->set_ov_ed_timer_running(false);
    m_Main->set_ov_ed_clock_preview(slint::SharedString(FormatOverlayClockText(m_EditingOverlay)));
    SyncOverlayTimerToAll(m_EditingOverlay);
    SyncClockOverlay();
}

void SlintApp::SetOverlayEditorTextColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    m_EditingOverlay.textR = r; m_EditingOverlay.textG = g; m_EditingOverlay.textB = b;
    m_Main->set_ov_ed_text_r(static_cast<float>(r));
    m_Main->set_ov_ed_text_g(static_cast<float>(g));
    m_Main->set_ov_ed_text_b(static_cast<float>(b));
    m_Main->set_ov_ed_text_color(slint::Color::from_rgb_uint8(r, g, b));
}

void SlintApp::SetOverlayEditorBgColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    m_EditingOverlay.bgR = r; m_EditingOverlay.bgG = g; m_EditingOverlay.bgB = b;
    m_Main->set_ov_ed_bg_color(slint::Color::from_rgb_uint8(r, g, b));
}

void SlintApp::SetOverlayEditorBorderColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    m_EditingOverlay.borderR = r; m_EditingOverlay.borderG = g; m_EditingOverlay.borderB = b;
    m_Main->set_ov_ed_border_color(slint::Color::from_rgb_uint8(r, g, b));
}

void SlintApp::RefreshOverlayLayers() {
    m_OverlayLayerEntries.clear();
    m_OverlayLayersModel->clear();

    int refScr = (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size())) ? m_InspectedScreenIdx : 0;
    const bool covLive = !m_Screens.empty() && m_Screens[refScr].hasCustomOverlay;
    const std::string activeCovPath = covLive ? m_Screens[refScr].customOverlay.path : "";
    const std::string activeCovName = covLive ? m_Screens[refScr].customOverlay.name : "";

    auto modeTag = [](const CustomOverlayConfig& ov) -> std::string {
        if (!ov.showClock) return "Texto / Pleca";
        if (ov.clockMode == 1) {
            int rem = ov.initialSec % 60;
            if (rem == 0) return "Retroceso (" + std::to_string(std::max(1, ov.initialSec / 60)) + "m)";
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02d:%02d", ov.initialSec / 60, rem);
            return std::string("Retroceso (") + buf + ")";
        }
        if (ov.clockMode == 2) return "Cronómetro (↑)";
        return "Hora en Vivo";
    };

    for (int i = 0; i < static_cast<int>(m_CustomOverlays.size()); ++i) {
        const auto& cov = m_CustomOverlays[i];
        bool isOn = covLive && ((!cov.path.empty() && activeCovPath == cov.path) || activeCovName == cov.name);

        MediaEntry m;
        m.title            = cov.name;
        m.subtitle         = isOn ? "● ACTIVO" : modeTag(cov);
        m.path             = cov.path;
        m.isOverlay        = true;
        m.isCustomOverlay  = true;
        m.customOverlayIdx = i;
        m_OverlayLayerEntries.push_back(m);

        LibraryItem item;
        item.title         = slint::SharedString(m.title);
        item.subtitle      = slint::SharedString(m.subtitle);
        item.path          = slint::SharedString(m.path);
        item.has_thumbnail = false;
        m_OverlayLayersModel->push_back(item);
    }

    std::vector<fs::path> dirs = {
        fs::path(ProyecThor::OverlaysPath()),
        fs::path("assets/overlays")
    };

    bool live = !m_Screens.empty() && m_Screens[refScr].hasOverlayImage;
    std::string activePath = !m_Screens.empty() ? m_Screens[refScr].overlayPath : m_ActiveOverlayPath;
    std::vector<std::string> seen;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".png", ".webp" })) continue;

            std::string fullPath = PathToUtf8(entry.path());
            if (std::find(seen.begin(), seen.end(), fullPath) != seen.end()) continue;
            seen.push_back(fullPath);

            MediaEntry m;
            m.title     = PathToUtf8(entry.path().stem());
            m.subtitle  = (live && activePath == fullPath) ? "● ACTIVO" : "Overlay PNG";
            m.path      = fullPath;
            m.isOverlay = true;
            m_OverlayLayerEntries.push_back(m);

            LibraryItem item;
            item.title    = slint::SharedString(m.title);
            item.subtitle = slint::SharedString(m.subtitle);
            item.path     = slint::SharedString(m.path);
            slint::Image thumb;
            item.has_thumbnail = LoadOrCreateThumbnail(fullPath, false, thumb);
            if (item.has_thumbnail) item.thumbnail = thumb;
            m_OverlayLayersModel->push_back(item);
        }
    }
}

void SlintApp::SelectOverlayLayer(int index) {
    if (index < 0 || index >= static_cast<int>(m_OverlayLayerEntries.size())) return;
    const auto& entry = m_OverlayLayerEntries[index];
    int refScr = (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size())) ? m_InspectedScreenIdx : 0;

    if (entry.isCustomOverlay) {
        if (entry.customOverlayIdx < 0 || entry.customOverlayIdx >= static_cast<int>(m_CustomOverlays.size())) return;
        const auto& cov = m_CustomOverlays[entry.customOverlayIdx];
        if (!m_Screens.empty() && m_Screens[refScr].hasCustomOverlay &&
            ((!cov.path.empty() && m_Screens[refScr].customOverlay.path == cov.path) ||
             m_Screens[refScr].customOverlay.name == cov.name)) {
            for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
                if (!IsScreenTargeted(i)) continue;
                m_Screens[i].hasCustomOverlay = false;
            }
            RefreshOverlayQueueModel();
            RefreshOverlayLayers();
            SyncClockOverlay();
            SyncScreenOutputs();
            return;
        }
        ApplyOverlayConfigToTargets(cov);
        return;
    }

    if (!m_Screens.empty() && m_Screens[refScr].overlayPath == entry.path && m_Screens[refScr].hasOverlayImage) {
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            if (!IsScreenTargeted(i)) continue;
            RemoveLayerOnScreen(i, "overlay");
        }
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
        SyncScreenOutputs();
        return;
    }
    m_ActiveOverlayPath = entry.path;
    auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
    slint::Image thumb;
    bool hasThumb = LoadOrCreateThumbnail(entry.path, false, thumb);
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].overlayName     = entry.title;
        m_Screens[i].overlayPath     = entry.path;
        m_Screens[i].hasOverlayImage = true;
        m_Screens[i].overlayImage    = img;
        m_Screens[i].hasOverlayThumb = hasThumb;
        if (hasThumb) m_Screens[i].overlayThumb = thumb;
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
    SyncScreenOutputs();
}

void SlintApp::AddOverlayLayerToQueue(int index) {
    if (index < 0 || index >= static_cast<int>(m_OverlayLayerEntries.size())) return;
    const auto& entry = m_OverlayLayerEntries[index];
    for (const auto& o : m_OverlayQueue) {
        if (o.path == entry.path) {
            m_Main->set_queue_category(2);
            return;
        }
    }
    OverlayQueueEntry o;
    o.title    = entry.title;
    o.path     = entry.path;
    o.isCustom = entry.isCustomOverlay;
    m_OverlayQueue.push_back(std::move(o));
    m_Main->set_queue_category(2);
    RefreshOverlayQueueModel();
}

void SlintApp::SelectSolidBgColor(int colorIdx) {
    static const struct { const char* name; uint8_t r, g, b; } kSolids[] = {
        {"Negro",      0,   0,   0},
        {"Carbón",     18,  18,  21},
        {"Azul Noche", 8,   18,  38},
        {"Burdeo",     36,  9,   17},
        {"Esmeralda",  7,   34,  25},
        {"Violeta",    23,  10,  43}
    };
    int idx = std::clamp(colorIdx, 0, 5);
    auto c = slint::Color::from_rgb_uint8(kSolids[idx].r, kSolids[idx].g, kSolids[idx].b);
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].bgName        = std::string("Color: ") + kSolids[idx].name;
        m_Screens[i].bgPath.clear();
        m_Screens[i].hasBgImage    = false;
        m_Screens[i].bgIsStatic    = false;
        m_Screens[i].bgIsVideo     = false;
        m_Screens[i].hasBgThumb    = false;
        m_Screens[i].bgSolidColor  = c;
        m_Screens[i].bgSolidRgb[0] = kSolids[idx].r / 255.0f;
        m_Screens[i].bgSolidRgb[1] = kSolids[idx].g / 255.0f;
        m_Screens[i].bgSolidRgb[2] = kSolids[idx].b / 255.0f;
    }
    bool anyVid = false;
    for (const auto& sc : m_Screens) if (sc.bgIsVideo) { anyVid = true; break; }
    if (!anyVid) {
        m_Player.Stop();
        m_HasVideoBg = false;
        m_CurrentBgPath.clear();
    }
    m_HasStaticImage = m_Screens[0].bgIsStatic;
    if (IsScreenTargeted(1)) ClearLanFrame();
    SyncScreenOutputs();
}

void SlintApp::LaunchLegacyImGui() {
#ifdef _WIN32
    std::system("start \"\" ProyecThor.exe");
#else
    std::system("./ProyecThor >/dev/null 2>&1 &");
#endif
}

// ── Capture Devices (V4L2 / DirectShow) & URL to Screen via libmpv ───────────
void SlintApp::RefreshCaptureDevices() {
    m_CaptureDevices.clear();
    m_CaptureModel->clear();
    m_Main->set_selected_capture_idx(-1);

#ifdef _WIN32
    // Windows DirectShow devices via PowerShell / WMI friendly names
    std::string out = RunCommandCapture(
        "powershell -NoProfile -Command \"Get-PnpDevice -Class Camera,Image -Status OK | Select-Object -ExpandProperty FriendlyName\" 2>nul");
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        CaptureDev d;
        d.name = line;
        d.uri  = "av://dshow:video=" + line;
        m_CaptureDevices.push_back(d);
    }
#elif defined(__APPLE__)
    // macOS AVFoundation video capture devices via ffmpeg listing
    std::string out = RunCommandCapture("ffmpeg -f avfoundation -list_devices true -i \"\" 2>&1");
    std::istringstream iss(out);
    std::string line;
    bool inVideoSection = false;
    while (std::getline(iss, line)) {
        if (line.find("AVFoundation video devices:") != std::string::npos) {
            inVideoSection = true;
            continue;
        }
        if (line.find("AVFoundation audio devices:") != std::string::npos) {
            break;
        }
        if (!inVideoSection) continue;
        auto lb = line.rfind('[');
        auto rb = (lb != std::string::npos) ? line.find(']', lb) : std::string::npos;
        if (lb != std::string::npos && rb != std::string::npos && rb > lb + 1) {
            std::string idxStr = line.substr(lb + 1, rb - lb - 1);
            bool allDigits = !idxStr.empty() && std::all_of(idxStr.begin(), idxStr.end(),
                                                            [](unsigned char c) { return std::isdigit(c); });
            if (allDigits) {
                std::string name = line.substr(rb + 1);
                while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
                while (!name.empty() && (name.back() == '\r' || name.back() == '\n')) name.pop_back();
                if (!name.empty()) {
                    CaptureDev d;
                    d.name = name + " (AVFoundation #" + idxStr + ")";
                    d.uri  = "av://avfoundation:" + idxStr;
                    m_CaptureDevices.push_back(d);
                }
            }
        }
    }
#else
    // Linux V4L2 devices (/sys/class/video4linux/video*)
    std::error_code ec;
    fs::path v4lDir("/sys/class/video4linux");
    if (fs::exists(v4lDir, ec) && fs::is_directory(v4lDir, ec)) {
        std::vector<fs::path> nodes;
        for (const auto& entry : fs::directory_iterator(v4lDir, ec)) {
            nodes.push_back(entry.path());
        }
        std::sort(nodes.begin(), nodes.end());
        for (const auto& node : nodes) {
            std::string devName = PathToUtf8(node.filename()); // e.g. video0
            std::string friendly = devName;
            std::ifstream nf(node / "name");
            if (nf.is_open()) {
                std::getline(nf, friendly);
                while (!friendly.empty() && (friendly.back() == '\r' || friendly.back() == '\n'))
                    friendly.pop_back();
            }
            // Skip metadata-only nodes if index > 0 when readable
            std::ifstream idxF(node / "index");
            int devIdx = 0;
            if (idxF.is_open()) idxF >> devIdx;
            if (devIdx != 0) continue;

            CaptureDev d;
            d.name = friendly + " (/dev/" + devName + ")";
            d.uri  = "av://v4l2:/dev/" + devName;
            m_CaptureDevices.push_back(d);
        }
    }
#endif

    for (const auto& d : m_CaptureDevices) {
        CaptureDeviceItem item;
        item.name = slint::SharedString(d.name);
        item.uri  = slint::SharedString(d.uri);
        m_CaptureModel->push_back(item);
    }

    if (m_CaptureDevices.empty()) {
        m_Main->set_capture_status("No se detectaron cámaras ni capturadoras USB activas.");
    } else {
        m_Main->set_capture_status(slint::SharedString(
            std::to_string(m_CaptureDevices.size()) + " dispositivo(s) de captura detectado(s). Haz clic en uno para proyectarlo."));
    }
}

void SlintApp::StartCaptureDevice(int index) {
    if (index < 0 || index >= static_cast<int>(m_CaptureDevices.size())) return;
    const auto& dev = m_CaptureDevices[index];

    m_HasStaticImage = false;
    m_HasVideoBg     = true;
    m_CurrentBgPath  = dev.uri;
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].bgName     = "Captura: " + dev.name;
        m_Screens[i].bgPath     = dev.uri;
        m_Screens[i].bgIsStatic = false;
        m_Screens[i].bgIsVideo  = true;
    }
    m_Main->set_capture_status(slint::SharedString("Capturando en vivo: " + dev.name + " (" + dev.uri + ")"));
    m_Main->set_media_paused(false);
    m_Main->set_media_muted(true);
    m_Player.PlayLiveStream(dev.uri, false, true, true);
    SyncScreenOutputs();
}

// ── Network Hub: Full ImGui StreamingPanel + FrameEncodeWorker LAN Engine ────
void SlintApp::CopyToClipboard(const std::string& text) {
    if (text.empty()) return;
#ifdef _WIN32
    FILE* p = _popen("clip", "w");
    if (p) { std::fwrite(text.data(), 1, text.size(), p); _pclose(p); }
#elif defined(__APPLE__)
    FILE* p = popen("pbcopy", "w");
    if (p) { std::fwrite(text.data(), 1, text.size(), p); pclose(p); }
#else
    // Try Wayland wl-copy first, then X11 xclip / xsel
    FILE* p = popen("wl-copy 2>/dev/null || xclip -selection clipboard 2>/dev/null || xsel --clipboard --input 2>/dev/null", "w");
    if (p) { std::fwrite(text.data(), 1, text.size(), p); pclose(p); }
#endif
}

void SlintApp::WireLanServerProviders(ProyecThor::Core::NetworkStreamServer& srv) {
    // Keep Chat LAN strictly separate (port 8082)
    srv.SetChatStore(nullptr);

    srv.SetSnapshotProvider([this]() -> ProyecThor::Core::StreamSnapshot {
        ProyecThor::Core::StreamSnapshot s;
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        bool hasFrame = m_LanHasFrame.load();
        s.currentText        = m_SnapText;
        s.showText           = !m_SnapText.empty();
        s.textSize           = m_SnapTextSize;
        s.textAlignment      = m_SnapTextAlign;
        s.vAlignment         = m_SnapTextValign;
        s.autoScale          = false;
        s.fontFamily         = m_SnapFontFamily;
        s.fontWeight         = m_SnapFontWeight;
        s.textShadow         = m_SnapTextShadow;
        s.textOutline        = m_SnapTextOutline;
        s.textGlow           = m_SnapTextGlow;
        s.textNeon           = m_SnapTextNeon;
        s.text3d             = m_SnapText3d;
        s.textUnderline      = m_SnapTextUnderline;
        s.textBoxBg          = m_SnapTextBoxBg;
        s.refW               = 1920;
        s.refH               = 1080;
        for (int i = 0; i < 4; ++i) s.textColor[i] = m_SnapTextColor[i];
        for (int i = 0; i < 3; ++i) s.bgColor[i]   = m_SnapBgColor[i];
        for (int i = 0; i < 4; ++i) s.margins[i]   = m_SnapMargins[i];
        s.isBgVideo          = m_HasVideoBg;
        s.hasFrame           = hasFrame;
        s.showPngOverlay     = !m_SnapOverlayPngPath.empty();
        s.overlayVersion     = m_LanOverlayVersion;
        s.overlay            = m_SnapOverlay;
        s.isProjecting       = m_SnapProjecting || !m_SnapText.empty() || hasFrame || s.overlay.active || s.showPngOverlay;
        s.transitionTrigger  = m_SnapTransTrigger;
        s.transitionType     = m_SnapTransType;
        s.transitionDuration = m_SnapTransDur;
        s.version            = m_StreamVersion;
        s.fontVersion        = m_LanFontVersion;
        return s;
    });

    srv.SetFrameProvider([this]() -> std::vector<uint8_t> {
        std::lock_guard<std::mutex> lk(m_LanFrameMutex);
        return m_LanLatestJpeg;
    });

    srv.SetFontPathProvider([this]() -> std::string {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        return m_LanFontCachePath;
    });

    srv.SetOverlayPathProvider([this]() -> std::string {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        return m_SnapOverlayPngPath;
    });
}

void SlintApp::UpdateLanFrameCapture() {
    if (!IsLanRunning() || m_LanConfig.transparentMode || !m_LanConfig.sendBackground) return;
    if (!m_HasVideoBg) return;
    if (m_Screens.size() > 1 && !m_Screens[1].bgIsVideo) return;

    int fps = 30;
    if (m_LanConfig.videoMode == ProyecThor::Core::StreamConfig::VideoMode::LowLatency) {
        fps = 8;
    } else if (m_LanConfig.videoMode == ProyecThor::Core::StreamConfig::VideoMode::UltraStable) {
        fps = std::clamp(m_LanConfig.targetFPS, 24, 60);
    }

    const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const int64_t intervalMs = std::max<int64_t>(16, 1000 / std::max(1, fps));
    if (nowMs - m_LanLastCaptureMs < intervalMs) return;
    m_LanLastCaptureMs = nowMs;

    std::vector<uint8_t> rgb;
    int w = 0, h = 0;
    if (!m_Player.CopyLatestFrameRgb(rgb, m_LanConfig.frameWidth, m_LanConfig.frameHeight, w, h)) {
        return;
    }

    int q = std::clamp(m_LanConfig.jpegQuality, 20, 100);
    m_LanEncodeWorker.SubmitFrame(std::move(rgb), w, h, q, [this](std::vector<uint8_t> jpeg) {
        if (jpeg.empty()) return;
        {
            std::lock_guard<std::mutex> lk(m_LanFrameMutex);
            m_LanLatestJpeg = std::move(jpeg);
        }
        if (!m_LanHasFrame.exchange(true)) {
            std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
            ++m_StreamVersion;
        }
    });
}

void SlintApp::UpdateLanStaticFrame(const slint::Image& img) {
    if (m_LanConfig.transparentMode || !m_LanConfig.sendBackground) return;
    auto rgbaOpt = img.to_rgba8();
    if (!rgbaOpt.has_value()) return;
    const auto& buf = *rgbaOpt;
    const int srcW = static_cast<int>(buf.width());
    const int srcH = static_cast<int>(buf.height());
    if (srcW <= 0 || srcH <= 0) return;

    int dstW = std::min(srcW, std::max(320, m_LanConfig.frameWidth));
    int dstH = std::min(srcH, std::max(180, m_LanConfig.frameHeight));
    std::vector<uint8_t> rgb(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 3);
    const auto* src = reinterpret_cast<const uint8_t*>(buf.begin());
    for (int y = 0; y < dstH; ++y) {
        int sy = (y * srcH) / dstH;
        const uint8_t* sRow = src + static_cast<size_t>(sy) * static_cast<size_t>(srcW) * 4;
        uint8_t* dRow = rgb.data() + static_cast<size_t>(y) * static_cast<size_t>(dstW) * 3;
        for (int x = 0; x < dstW; ++x) {
            int sx = (x * srcW) / dstW;
            dRow[x * 3 + 0] = sRow[sx * 4 + 0];
            dRow[x * 3 + 1] = sRow[sx * 4 + 1];
            dRow[x * 3 + 2] = sRow[sx * 4 + 2];
        }
    }

    int q = std::clamp(m_LanConfig.jpegQuality, 20, 100);
    m_LanEncodeWorker.SubmitFrame(std::move(rgb), dstW, dstH, q, [this](std::vector<uint8_t> jpeg) {
        if (jpeg.empty()) return;
        {
            std::lock_guard<std::mutex> lk(m_LanFrameMutex);
            m_LanLatestJpeg = std::move(jpeg);
        }
        m_LanHasFrame.store(true);
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        ++m_StreamVersion;
    });
}

void SlintApp::ClearLanFrame() {
    {
        std::lock_guard<std::mutex> lk(m_LanFrameMutex);
        m_LanLatestJpeg.clear();
    }
    m_LanHasFrame.store(false);
    std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
    ++m_StreamVersion;
}

void SlintApp::ToggleLanStream() {
    if (IsLanRunning()) {
        m_LanServer->Stop();
        m_LanServer.reset();
        m_Player.SetLanCaptureEnabled(false);
        ClearLanFrame();
        UpdatePlayerRenderSize();
        m_Main->set_lan_running(false);
        m_Main->set_lan_url("Inactivo — pulsa Iniciar para transmitir video/letras en tu red local");
        m_Main->set_has_lan_qr(false);
        AppendDebugLog("Servidor LAN detenido.");
    } else {
        ApplyLanConfig();
        int port = 8080;
        try {
            std::string pStr = std::string(m_Main->get_lan_port_input());
            if (!pStr.empty()) port = std::clamp(std::stoi(pStr), 1024, 65535);
        } catch (...) { port = 8080; }

        std::string hostIp = std::string(m_Main->get_lan_host_ip());
        while (!hostIp.empty() && hostIp.front() == ' ') hostIp.erase(hostIp.begin());
        while (!hostIp.empty() && hostIp.back() == ' ')  hostIp.pop_back();

        // Exact ImGui PresentationCore::ToggleNetworkStream lifecycle (fresh server instance)
        m_LanServer = std::make_unique<ProyecThor::Core::NetworkStreamServer>();
        m_LanServer->SetConfig(m_LanConfig);
        WireLanServerProviders(*m_LanServer);

        if (m_LanServer->Start(port, hostIp)) {
            m_LanServer->SetConfig(m_LanConfig);
            std::string url = m_LanServer->GetBaseURL();
            m_Main->set_lan_running(true);
            m_Main->set_lan_url(slint::SharedString(url));
            m_Main->set_lan_qr_image(GenerateQrImage(url));
            m_Main->set_has_lan_qr(true);

            const bool captureBg = !m_LanConfig.transparentMode && m_LanConfig.sendBackground;
            m_Player.SetLanCaptureEnabled(captureBg);
            UpdatePlayerRenderSize();
            m_LanLastCaptureMs = 0;

            if (captureBg && m_HasStaticImage && m_Main->get_has_live_preview_image()) {
                UpdateLanStaticFrame(m_Main->get_live_preview_image());
            } else {
                std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
                ++m_StreamVersion;
            }
            AppendDebugLog("Transmisión LAN iniciada en " + url);
        } else {
            m_LanServer.reset();
            m_Main->set_lan_url(slint::SharedString(
                "Error al iniciar servidor LAN en puerto " + std::to_string(port) + " (¿puerto ocupado?)."));
        }
    }
}

void SlintApp::RefreshSmartTvModel() {
    m_SmartTvModel->clear();
    for (int i = 0; i < static_cast<int>(m_SmartTvDevices.size()); ++i) {
        const auto& d = m_SmartTvDevices[i];
        SmartTvItem item;
        item.name   = slint::SharedString(d.name);
        item.ip     = slint::SharedString(d.ip);
        item.info   = slint::SharedString(
            (d.manufacturer.empty() ? "DLNA / UPnP" : d.manufacturer) + " • " + d.ip + ":" + std::to_string(d.port));
        item.active = (i == m_ActiveSmartTvIdx);
        m_SmartTvModel->push_back(item);
    }
}

void SlintApp::ScanSmartTvsAsync() {
    m_Main->set_smart_tv_scanning(true);
    m_Main->set_smart_tv_status("Buscando Smart TVs y pantallas DLNA / UPnP en tu red Wi-Fi / LAN...");

    std::thread([this]() {
        auto found = ProyecThor::Core::SmartTvCaster::DiscoverDevices(1800);
        slint::invoke_from_event_loop([this, found = std::move(found)]() mutable {
            m_SmartTvDevices   = std::move(found);
            m_ActiveSmartTvIdx = -1;
            m_Main->set_smart_tv_scanning(false);
            RefreshSmartTvModel();
            if (m_SmartTvDevices.empty()) {
                m_Main->set_smart_tv_status(
                    "No se detectaron Smart TVs DLNA automáticamente. Puedes ingresar la IP del TV arriba o abrir la URL LAN en el navegador de tu Smart TV.");
            } else {
                m_Main->set_smart_tv_status(slint::SharedString(
                    "Se encontraron " + std::to_string(m_SmartTvDevices.size()) +
                    " Smart TV(s) en tu red. Pulsa 'Enviar a TV' para proyectar."));
            }
        });
    }).detach();
}

void SlintApp::CastToSmartTv(int index) {
    if (index < 0 || index >= static_cast<int>(m_SmartTvDevices.size())) return;
    if (!IsLanRunning()) {
        ToggleLanStream();
        if (!IsLanRunning()) {
            m_Main->set_smart_tv_status("No se pudo iniciar el servidor LAN local para enviar al Smart TV.");
            return;
        }
    }

    auto tv = m_SmartTvDevices[index];
    std::string baseUrl  = m_LanServer->GetBaseURL();
    std::string mjpegUrl = baseUrl + "/live.mjpg";
    std::string jpgUrl   = baseUrl + "/live.jpg";
    m_Main->set_smart_tv_status(slint::SharedString("Enviando señal en vivo a " + tv.name + " (" + tv.ip + ")..."));

    std::thread([this, index, tv, mjpegUrl, jpgUrl, baseUrl]() {
        std::string err;
        bool ok = ProyecThor::Core::SmartTvCaster::CastStreamToTv(tv, mjpegUrl, "ProyecThor En Vivo", true, err);
        if (!ok) {
            std::string err2;
            ok = ProyecThor::Core::SmartTvCaster::CastStreamToTv(tv, jpgUrl, "ProyecThor En Vivo", false, err2);
            if (!ok) err += " | Fallback JPEG: " + err2;
        }
        slint::invoke_from_event_loop([this, index, tv, ok, err, baseUrl]() {
            if (ok) {
                m_ActiveSmartTvIdx = index;
                RefreshSmartTvModel();
                m_Main->set_smart_tv_status(slint::SharedString(
                    "● Enviando en vivo a " + tv.name + " (" + tv.ip + "). También disponible en navegador: " + baseUrl));
            } else {
                m_Main->set_smart_tv_status(slint::SharedString(
                    "El TV (" + tv.name + ") rechazó DLNA (" + err + "). Abre " + baseUrl + " en el navegador del Smart TV."));
            }
        });
    }).detach();
}

void SlintApp::CastToSmartTvIp(const std::string& rawIp) {
    if (rawIp.empty()) return;
    if (!IsLanRunning()) {
        ToggleLanStream();
        if (!IsLanRunning()) return;
    }

    std::string baseUrl  = m_LanServer->GetBaseURL();
    std::string mjpegUrl = baseUrl + "/live.mjpg";
    std::string jpgUrl   = baseUrl + "/live.jpg";
    m_Main->set_smart_tv_status(slint::SharedString("Conectando con Smart TV en " + rawIp + "..."));

    std::thread([this, rawIp, mjpegUrl, jpgUrl, baseUrl]() {
        ProyecThor::Core::SmartTvDevice dev;
        std::string err;
        if (!ProyecThor::Core::SmartTvCaster::ProbeManualIp(rawIp, dev, err)) {
            slint::invoke_from_event_loop([this, err, baseUrl]() {
                m_Main->set_smart_tv_status(slint::SharedString(err + " URL LAN: " + baseUrl));
            });
            return;
        }

        bool ok = ProyecThor::Core::SmartTvCaster::CastStreamToTv(dev, mjpegUrl, "ProyecThor En Vivo", true, err);
        if (!ok) {
            std::string err2;
            ok = ProyecThor::Core::SmartTvCaster::CastStreamToTv(dev, jpgUrl, "ProyecThor En Vivo", false, err2);
            if (!ok) err += " | Fallback JPEG: " + err2;
        }

        slint::invoke_from_event_loop([this, dev, ok, err, baseUrl]() {
            int foundIdx = -1;
            for (int i = 0; i < static_cast<int>(m_SmartTvDevices.size()); ++i) {
                if (m_SmartTvDevices[i].ip == dev.ip) { foundIdx = i; break; }
            }
            if (foundIdx < 0) {
                m_SmartTvDevices.push_back(dev);
                foundIdx = static_cast<int>(m_SmartTvDevices.size()) - 1;
            }
            if (ok) {
                m_ActiveSmartTvIdx = foundIdx;
                RefreshSmartTvModel();
                m_Main->set_smart_tv_status(slint::SharedString(
                    "● Transmitiendo a " + dev.name + " (" + dev.ip + ")"));
            } else {
                RefreshSmartTvModel();
                m_Main->set_smart_tv_status(slint::SharedString(
                    "Conectado a " + dev.name + ", pero DLNA devolvió error (" + err + "). Abre " + baseUrl + " en el navegador del TV."));
            }
        });
    }).detach();
}

void SlintApp::StopSmartTvCast(int index) {
    if (index < 0 || index >= static_cast<int>(m_SmartTvDevices.size())) return;
    auto tv = m_SmartTvDevices[index];
    std::thread([this, tv]() {
        std::string err;
        ProyecThor::Core::SmartTvCaster::StopCastOnTv(tv, err);
        slint::invoke_from_event_loop([this, tv]() {
            m_ActiveSmartTvIdx = -1;
            RefreshSmartTvModel();
            m_Main->set_smart_tv_status(slint::SharedString("Transmisión detenida en " + tv.name + "."));
        });
    }).detach();
}

void SlintApp::ApplyLanConfig() {
    m_LanConfig.transparentMode = m_Main->get_lan_transparent();
    m_LanConfig.sendBackground  = m_Main->get_lan_send_bg();
    m_LanConfig.sendText        = m_Main->get_lan_send_text();
    m_LanConfig.sendOverlay     = m_Main->get_lan_send_overlay();

    int resPreset = m_Main->get_lan_res_preset();
    if (resPreset == 0)      { m_LanConfig.frameWidth = 640;  m_LanConfig.frameHeight = 360;  }
    else if (resPreset == 2) { m_LanConfig.frameWidth = 1920; m_LanConfig.frameHeight = 1080; }
    else                     { m_LanConfig.frameWidth = 1280; m_LanConfig.frameHeight = 720;  }

    int vMode = m_Main->get_lan_video_mode();
    if (vMode == 0)      m_LanConfig.videoMode = ProyecThor::Core::StreamConfig::VideoMode::LowLatency;
    else if (vMode == 2) m_LanConfig.videoMode = ProyecThor::Core::StreamConfig::VideoMode::UltraStable;
    else                 m_LanConfig.videoMode = ProyecThor::Core::StreamConfig::VideoMode::HighQuality;

    m_LanConfig.targetFPS   = (m_Main->get_lan_target_fps() >= 60) ? 60 : 30;
    m_LanConfig.jpegQuality = std::clamp(static_cast<int>(std::round(m_Main->get_lan_jpeg_quality())), 20, 100);

    if (m_LanServer) {
        m_LanServer->SetConfig(m_LanConfig);
    }

    const bool captureBg = IsLanRunning() && !m_LanConfig.transparentMode && m_LanConfig.sendBackground;
    m_Player.SetLanCaptureEnabled(captureBg);
    UpdatePlayerRenderSize();

    if (m_LanConfig.transparentMode || !m_LanConfig.sendBackground) {
        ClearLanFrame();
    } else if (IsLanRunning() && m_HasStaticImage && m_Main->get_has_live_preview_image()) {
        UpdateLanStaticFrame(m_Main->get_live_preview_image());
    }
}

void SlintApp::ToggleChatServer() {
    if (m_ChatServer.IsRunning()) {
        m_ChatServer.Stop();
        m_Main->set_chat_running(false);
        m_Main->set_chat_url("Inactivo — inicia el Chat de Equipo sin activar transmisión de video");
        m_Main->set_has_chat_qr(false);
    } else {
        if (m_ChatServer.Start(8082)) {
            std::string chatUrl = m_ChatServer.GetBaseURL() + "/chat";
            m_Main->set_chat_running(true);
            m_Main->set_chat_url(slint::SharedString(chatUrl));
            m_Main->set_chat_qr_image(GenerateQrImage(chatUrl));
            m_Main->set_has_chat_qr(true);
            m_LastChatMsgId = 0;
            RefreshChatLog();
        } else {
            m_Main->set_chat_url("Error al iniciar Chat LAN en puerto 8082 (¿puerto ocupado?).");
        }
    }
}

void SlintApp::ToggleMobileServer() {
    if (m_MobileServer.IsRunning()) {
        m_MobileServer.Stop();
        m_Main->set_mobile_running(false);
        m_Main->set_mobile_url("Inactivo — pulsa Iniciar para conectar tu celular");
        m_Main->set_has_mobile_qr(false);
    } else {
        if (m_MobileServer.Start(8090)) {
            std::string url = m_MobileServer.GetBaseURL() + "/?pin=" + m_MobileServer.GetPairingPin();
            m_Main->set_mobile_running(true);
            m_Main->set_mobile_url(slint::SharedString(url));
            m_Main->set_mobile_qr_image(GenerateQrImage(url));
            m_Main->set_has_mobile_qr(true);
        }
    }
}

void SlintApp::RegenerateMobilePin() {
    std::string pin = GenerateRandomPin6();
    m_MobileServer.SetPairingPin(pin);
    m_Main->set_mobile_pin(slint::SharedString(pin));
    if (m_MobileServer.IsRunning()) {
        std::string url = m_MobileServer.GetBaseURL() + "/?pin=" + pin;
        m_Main->set_mobile_url(slint::SharedString(url));
        m_Main->set_mobile_qr_image(GenerateQrImage(url));
    }
    SavePersistedState();
}

void SlintApp::RefreshChatLog() {
    auto msgs = m_ChatStore.GetMessagesSince(0);
    if (msgs.empty()) {
        if (m_ChatServer.IsRunning()) {
            m_Main->set_chat_log("Chat activo en " + slint::SharedString(m_ChatServer.GetBaseURL()) + "/chat — sin mensajes aún.");
        } else {
            m_Main->set_chat_log("Pulsa 'Iniciar Chat LAN (Puerto 8082)' para abrir el chat ligero sin video.");
        }
        return;
    }
    if (msgs.back().id == m_LastChatMsgId) return;
    m_LastChatMsgId = msgs.back().id;

    std::ostringstream oss;
    size_t start = msgs.size() > 10 ? msgs.size() - 10 : 0;
    for (size_t i = start; i < msgs.size(); ++i) {
        if (i > start) oss << "\n";
        oss << "[" << msgs[i].nickname << "] " << msgs[i].text;
    }
    m_Main->set_chat_log(slint::SharedString(oss.str()));
}

void SlintApp::SendOscCommand(const std::string& host, const std::string& portStr,
                              const std::string& address, const std::string& args) {
    int port = 9000;
    try { port = std::stoi(portStr); } catch (...) {}
    auto parsed = ProyecThor::Core::ParseOSCArgs(args);
    std::string err;
    bool ok = ProyecThor::Core::SendOSCMessage(host, port, address, parsed, &err);
    if (ok) {
        m_Main->set_osc_status(slint::SharedString("Enviado OK -> " + host + ":" + std::to_string(port) + " " + address));
    } else {
        m_Main->set_osc_status(slint::SharedString("Error OSC: " + err));
    }
}

std::string SlintApp::GenerateRandomPin6() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(100000, 999999);
    return std::to_string(dist(gen));
}

// ── Music Player (Tab 9 - independent audio) ─────────────────────────────────
void SlintApp::ScanMusicTracks() {
    m_MusicEntries.clear();
    m_MusicScanned = true;

    std::vector<fs::path> dirs = {
        fs::path(ProyecThor::GetAssetsPath()) / "audio",
        fs::path(ProyecThor::GetAssetsPath()) / "music"
    };
    for (const auto& customDir : m_MusicScanFolders) {
        if (!customDir.empty()) dirs.emplace_back(customDir);
    }
    const std::vector<std::string> audioExts = { ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".opus" };
    std::vector<std::string> seen;

    for (const auto& dir : dirs) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), audioExts)) continue;
            std::string p = PathToUtf8(entry.path());
            if (std::find(seen.begin(), seen.end(), p) != seen.end()) continue;
            seen.push_back(p);

            MediaEntry m;
            m.title    = PathToUtf8(entry.path().stem());
            m.subtitle = PathToUtf8(entry.path().extension()) + " • " + PathToUtf8(entry.path().parent_path().filename());
            m.path     = p;
            m.isAudio  = true;
            m_MusicEntries.push_back(std::move(m));
        }
    }
    std::sort(m_MusicEntries.begin(), m_MusicEntries.end(),
              [](const MediaEntry& a, const MediaEntry& b) { return a.title < b.title; });
    RefreshMusicModel();
    AppendDebugLog("Música escaneada: " + std::to_string(m_MusicEntries.size()) + " pista(s).");
}

bool SlintApp::ExtractMusicCoverArt(const std::string& audioPath, slint::Image& outImg) {
    if (audioPath.empty()) return false;
    std::string cacheKey = "music_cover:" + audioPath;
    auto it = m_ThumbCache.find(cacheKey);
    if (it != m_ThumbCache.end()) {
        outImg = it->second;
        return true;
    }

    std::error_code ec;
    fs::path thumbDir = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails";
    fs::create_directories(thumbDir, ec);
    size_t h = std::hash<std::string>{}(audioPath);
    fs::path coverPath = thumbDir / ("music_" + std::to_string(h) + ".jpg");
    fs::path noCoverMarker = thumbDir / ("music_" + std::to_string(h) + ".none");

    if (!fs::exists(coverPath, ec) && !fs::exists(noCoverMarker, ec)) {
#ifdef _WIN32
        std::string cmd = "ffmpeg -y -i \"" + audioPath + "\" -an -vcodec mjpeg -vframes 1 -vf scale=240:-1 \"" + PathToUtf8(coverPath) + "\" 2>nul";
#else
        std::string cmd = "ffmpeg -y -i \"" + audioPath + "\" -an -vcodec mjpeg -vframes 1 -vf scale=240:-1 \"" + PathToUtf8(coverPath) + "\" >/dev/null 2>&1";
#endif
        std::system(cmd.c_str());
        if (!fs::exists(coverPath, ec) || fs::file_size(coverPath, ec) == 0) {
            fs::remove(coverPath, ec);
            // Fallback: check cover.jpg / folder.jpg in the same directory
            fs::path dir = fs::path(audioPath).parent_path();
            for (const char* name : { "cover.jpg", "cover.png", "folder.jpg", "folder.png" }) {
                if (fs::exists(dir / name, ec)) {
                    outImg = slint::Image::load_from_path(slint::SharedString(PathToUtf8(dir / name)));
                    m_ThumbCache.emplace(cacheKey, outImg);
                    return true;
                }
            }
            std::ofstream m(noCoverMarker);
            return false;
        }
    }

    if (fs::exists(coverPath, ec) && fs::file_size(coverPath, ec) > 0) {
        outImg = slint::Image::load_from_path(slint::SharedString(PathToUtf8(coverPath)));
        m_ThumbCache.emplace(cacheKey, outImg);
        return true;
    }
    return false;
}

void SlintApp::PlayMusicTrack(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_MusicEntries.size())) return;
    m_CurrentMusicIdx = idx;
    const auto& trk = m_MusicEntries[idx];
    m_MusicPlayer.Play(trk.path, m_Main->get_music_loop(), false);
    m_Main->set_music_playing(true);
    m_Main->set_music_paused(false);
    m_Main->set_music_now_playing(slint::SharedString(trk.title));
    m_Main->set_music_now_subtitle(slint::SharedString(trk.subtitle));
    slint::Image cov;
    if (ExtractMusicCoverArt(trk.path, cov)) {
        m_Main->set_music_now_cover(cov);
        m_Main->set_has_music_now_cover(true);
    } else {
        m_Main->set_has_music_now_cover(false);
    }
    ApplyMusicEqualizer(true);
    ExtractMusicWaveformAsync(trk.path);
    RefreshMusicModel();
    RefreshActiveLayersModel();
    AppendDebugLog("Reproduciendo música: " + trk.title);
}

void SlintApp::ExtractMusicWaveformAsync(const std::string& audioPath) {
    if (audioPath.empty()) return;
    const uint64_t gen = m_WaveExtractGen.fetch_add(1) + 1;

    // Reset real spectrum buffer immediately for the new track
    {
        std::lock_guard<std::mutex> lk(m_SpectrumMutex);
        m_TrackSpectrumFrames.clear();
        m_TrackSpectrumPeak = 0.06f;
    }

    std::thread([this, audioPath, gen]() {
        // Real 256-point Radix-2 Cooley-Tukey FFT on 16kHz mono PCM (50ms = 800 samples/frame -> 20 real FFT frames/sec!)
        constexpr int kSampleRate = 16000;
        constexpr int kFrameSamples = 800; // 50ms per real spectrum frame
        constexpr int kFftN = 256;
        constexpr int kWaveBins = 56;
        constexpr float kPi = 3.14159265358979323846f;

        // Precompute Hann window + bit-reversal table
        float hann[kFftN];
        for (int i = 0; i < kFftN; ++i) {
            hann[i] = 0.5f * (1.0f - std::cos((2.0f * kPi * static_cast<float>(i)) / static_cast<float>(kFftN - 1)));
        }

        // 12 logarithmic frequency bands spanning 62.5Hz .. 8000Hz (bin k = k * 62.5 Hz)
        static const int kBandRanges[12][2] = {
            { 1, 1 },   //  62 Hz  (Sub)
            { 2, 2 },   // 125 Hz  (Kick/Bass)
            { 3, 4 },   // 187-250 Hz (Warm Bass)
            { 5, 6 },   // 312-375 Hz (Low Mid)
            { 7, 10 },  // 437-625 Hz (Body)
            { 11, 15 }, // 687-937 Hz (Mid)
            { 16, 22 }, // 1.0-1.4 kHz (Vocal Core)
            { 23, 32 }, // 1.4-2.0 kHz (Vocal Upper)
            { 33, 46 }, // 2.1-2.9 kHz (Presence)
            { 47, 66 }, // 2.9-4.1 kHz (Clarity)
            { 67, 94 }, // 4.2-5.9 kHz (Brilliance)
            { 95, 127 } // 5.9-7.9 kHz (Air/Cymbals)
        };
        // Perceptual tilt so high-frequency cymbals/air match sub-bass visually (pink-noise compensation)
        static const float kBandTilt[12] = {
            0.78f, 0.88f, 0.96f, 1.05f, 1.15f, 1.25f, 1.38f, 1.55f, 1.75f, 2.00f, 2.30f, 2.65f
        };

        auto computeRealFftBands = [&](const int16_t* pcm, std::array<float, 12>& outBands) {
            float re[kFftN];
            float im[kFftN];
            // Sample centered 256-sample window inside the 800-sample 50ms frame
            const int offset = (kFrameSamples - kFftN) / 2;
            for (int i = 0; i < kFftN; ++i) {
                re[i] = (static_cast<float>(pcm[offset + i]) / 32768.0f) * hann[i];
                im[i] = 0.0f;
            }
            // Bit-reversal permutation
            for (int i = 1, j = 0; i < kFftN; ++i) {
                int bit = kFftN >> 1;
                for (; j & bit; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) {
                    std::swap(re[i], re[j]);
                    std::swap(im[i], im[j]);
                }
            }
            // Iterative Radix-2 FFT butterflies
            for (int len = 2; len <= kFftN; len <<= 1) {
                float ang = -2.0f * kPi / static_cast<float>(len);
                float wlenRe = std::cos(ang);
                float wlenIm = std::sin(ang);
                for (int i = 0; i < kFftN; i += len) {
                    float wRe = 1.0f, wIm = 0.0f;
                    int half = len >> 1;
                    for (int j = 0; j < half; ++j) {
                        int u = i + j;
                        int v = i + j + half;
                        float tRe = re[v] * wRe - im[v] * wIm;
                        float tIm = re[v] * wIm + im[v] * wRe;
                        re[v] = re[u] - tRe;
                        im[v] = im[u] - tIm;
                        re[u] += tRe;
                        im[u] += tIm;
                        float nextWRe = wRe * wlenRe - wIm * wlenIm;
                        wIm = wRe * wlenIm + wIm * wlenRe;
                        wRe = nextWRe;
                    }
                }
            }
            // Extract true RMS magnitude per logarithmic frequency band
            for (int b = 0; b < 12; ++b) {
                int lo = kBandRanges[b][0];
                int hi = kBandRanges[b][1];
                float sumSq = 0.0f;
                for (int k = lo; k <= hi; ++k) {
                    sumSq += re[k] * re[k] + im[k] * im[k];
                }
                float mag = std::sqrt(sumSq / static_cast<float>(hi - lo + 1)) / (static_cast<float>(kFftN) * 0.25f);
                outBands[b] = mag * kBandTilt[b];
            }
        };

        std::vector<float> frameRms;
        frameRms.reserve(4096);
        std::vector<std::array<float, 12>> batch;
        batch.reserve(16);

#ifdef _WIN32
        std::string cmd = "ffmpeg -v error -i \"" + audioPath + "\" -ac 1 -ar " + std::to_string(kSampleRate) + " -f s16le - 2>nul";
        FILE* pipe = _popen(cmd.c_str(), "rb");
#else
        std::string cmd = "ffmpeg -v error -i \"" + audioPath + "\" -ac 1 -ar " + std::to_string(kSampleRate) + " -f s16le - 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (pipe) {
            int16_t pcmBuf[kFrameSamples];
            while (m_WaveExtractGen.load() == gen) {
                size_t got = std::fread(pcmBuf, sizeof(int16_t), kFrameSamples, pipe);
                if (got < static_cast<size_t>(kFftN)) break;
                if (got < static_cast<size_t>(kFrameSamples)) {
                    std::memset(pcmBuf + got, 0, (kFrameSamples - got) * sizeof(int16_t));
                }

                // Exact frame RMS for the full-song waveform scrubbar
                double sumSq = 0.0;
                for (size_t i = 0; i < got; ++i) {
                    double s = static_cast<double>(pcmBuf[i]) / 32768.0;
                    sumSq += s * s;
                }
                frameRms.push_back(static_cast<float>(std::sqrt(sumSq / static_cast<double>(got))));

                // Real 12-band FFT spectrum for this 50ms slice of the song
                std::array<float, 12> bands{};
                computeRealFftBands(pcmBuf, bands);
                float localMax = 0.0f;
                for (float v : bands) localMax = std::max(localMax, v);
                batch.push_back(bands);

                // Stream progressively every 10 frames (~0.5s of audio) so real waves start dancing immediately!
                if (batch.size() >= 10) {
                    std::lock_guard<std::mutex> lk(m_SpectrumMutex);
                    if (m_WaveExtractGen.load() != gen) break;
                    for (const auto& f : batch) {
                        for (float v : f) m_TrackSpectrumPeak = std::max(m_TrackSpectrumPeak, v);
                        m_TrackSpectrumFrames.push_back(f);
                    }
                    batch.clear();
                }
            }
#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }

        if (m_WaveExtractGen.load() != gen) return;

        // Flush remaining frames
        if (!batch.empty()) {
            std::lock_guard<std::mutex> lk(m_SpectrumMutex);
            for (const auto& f : batch) {
                for (float v : f) m_TrackSpectrumPeak = std::max(m_TrackSpectrumPeak, v);
                m_TrackSpectrumFrames.push_back(f);
            }
        }

        // Build high-res 56-bar real full-track waveform envelope
        std::vector<float> bins(kWaveBins, 0.12f);
        float avgE = 0.45f;
        if (!frameRms.empty()) {
            float maxE = 0.001f;
            for (int b = 0; b < kWaveBins; ++b) {
                size_t start = (static_cast<size_t>(b) * frameRms.size()) / kWaveBins;
                size_t end   = std::max(start + 1, (static_cast<size_t>(b + 1) * frameRms.size()) / kWaveBins);
                end = std::min(end, frameRms.size());
                float acc = 0.0f;
                for (size_t k = start; k < end; ++k) acc += frameRms[k];
                float v = acc / static_cast<float>(std::max<size_t>(1, end - start));
                bins[b] = v;
                if (v > maxE) maxE = v;
            }
            float sumNorm = 0.0f;
            for (int b = 0; b < kWaveBins; ++b) {
                float norm = std::pow(std::clamp(bins[b] / maxE, 0.0f, 1.0f), 0.72f);
                bins[b] = std::clamp(0.08f + norm * 0.92f, 0.08f, 1.0f);
                sumNorm += bins[b];
            }
            avgE = sumNorm / static_cast<float>(kWaveBins);
        }

        slint::invoke_from_event_loop([this, gen, bins = std::move(bins), avgE]() {
            if (m_WaveExtractGen.load() != gen) return;
            m_MusicTrackEnvelope  = bins;
            m_MusicTrackAvgEnergy = std::max(0.12f, avgE);
            m_MusicTrackWaveModel->clear();
            for (float v : m_MusicTrackEnvelope) {
                m_MusicTrackWaveModel->push_back(v);
            }
        });
    }).detach();
}

void SlintApp::ApplyMusicEqualizer(bool force) {
    bool eqOn  = m_Main->get_music_eq_enabled();
    bool dynOn = m_Main->get_music_dynamic_eq();

    float base[5] = {
        eqOn ? m_Main->get_music_eq_60()    : 0.0f,
        eqOn ? m_Main->get_music_eq_230()   : 0.0f,
        eqOn ? m_Main->get_music_eq_910()   : 0.0f,
        eqOn ? m_Main->get_music_eq_3600()  : 0.0f,
        eqOn ? m_Main->get_music_eq_12000() : 0.0f
    };

    float finalBands[5];
    for (int i = 0; i < 5; ++i) {
        finalBands[i] = std::clamp(base[i] + (dynOn ? m_DynEqOffsets[i] : 0.0f), -12.0f, 12.0f);
    }

    if (!force && dynOn) {
        float maxDelta = 0.0f;
        for (int i = 0; i < 5; ++i) {
            maxDelta = std::max(maxDelta, std::abs(finalBands[i] - m_DynEqApplied[i]));
        }
        if (maxDelta < 0.45f) return;
    }

    for (int i = 0; i < 5; ++i) m_DynEqApplied[i] = finalBands[i];

    if (m_MusicPlayer.IsPlaying() || m_CurrentMusicIdx >= 0) {
        m_MusicPlayer.SetMusicAudioFx(
            eqOn, dynOn,
            finalBands[0], finalBands[1], finalBands[2], finalBands[3], finalBands[4]);
    }
}

void SlintApp::ApplyMusicEqPreset(int presetIdx) {
    m_Main->set_music_eq_enabled(true);
    m_Main->set_music_eq_preset(presetIdx);
    switch (presetIdx) {
        case 1: // Alabanza (Warm lows + open highs)
            m_Main->set_music_eq_60(4.0f);  m_Main->set_music_eq_230(2.0f);
            m_Main->set_music_eq_910(1.0f); m_Main->set_music_eq_3600(3.0f); m_Main->set_music_eq_12000(4.5f);
            break;
        case 2: // Voz / Predicación
            m_Main->set_music_eq_60(-4.0f); m_Main->set_music_eq_230(-1.5f);
            m_Main->set_music_eq_910(4.0f); m_Main->set_music_eq_3600(3.5f); m_Main->set_music_eq_12000(1.5f);
            break;
        case 3: // Acústico
            m_Main->set_music_eq_60(1.5f);  m_Main->set_music_eq_230(2.5f);
            m_Main->set_music_eq_910(1.5f); m_Main->set_music_eq_3600(2.5f); m_Main->set_music_eq_12000(3.0f);
            break;
        case 4: // Bajos Profundos
            m_Main->set_music_eq_60(7.0f);  m_Main->set_music_eq_230(4.0f);
            m_Main->set_music_eq_910(0.0f); m_Main->set_music_eq_3600(1.0f); m_Main->set_music_eq_12000(2.0f);
            break;
        case 5: // Estudio (Hi-Fi)
            m_Main->set_music_eq_60(2.0f);  m_Main->set_music_eq_230(0.5f);
            m_Main->set_music_eq_910(0.0f); m_Main->set_music_eq_3600(2.0f); m_Main->set_music_eq_12000(3.5f);
            break;
        default: // 0: Plano
            m_Main->set_music_eq_60(0.0f);  m_Main->set_music_eq_230(0.0f);
            m_Main->set_music_eq_910(0.0f); m_Main->set_music_eq_3600(0.0f); m_Main->set_music_eq_12000(0.0f);
            break;
    }
    ApplyMusicEqualizer(true);
}

void SlintApp::UpdateMusicWaveformAndDynamicEq() {
    const bool active = m_MusicPlayer.IsPlaying() && !m_MusicPlayer.IsPaused();
    if (!active) {
        if (!m_WaveIdleSettled) {
            bool allCalm = true;
            for (int i = 0; i < 12; ++i) {
                m_LiveWaveSmoothed[i] += (0.08f - m_LiveWaveSmoothed[i]) * 0.25f;
                if (std::abs(m_LiveWaveSmoothed[i] - 0.08f) > 0.008f) allCalm = false;
                m_MusicLiveWaveModel->set_row_data(i, m_LiveWaveSmoothed[i]);
            }
            m_WaveIdleSettled = allCalm;
        }
        return;
    }

    m_WaveIdleSettled = false;

    // Sample 100% REAL millisecond-synced 12-band FFT spectrum of the currently playing song!
    const int64_t timeMs = std::max<int64_t>(0, m_MusicPlayer.GetTimeMs());
    std::array<float, 12> rawBands{};
    float normPeak = 0.08f;
    bool hasRealSpectrum = false;
    {
        std::lock_guard<std::mutex> lk(m_SpectrumMutex);
        if (!m_TrackSpectrumFrames.empty()) {
            hasRealSpectrum = true;
            normPeak = std::max(0.04f, m_TrackSpectrumPeak * 0.82f);
            float exactFrame = static_cast<float>(timeMs) / 50.0f; // 50ms per FFT frame
            int maxIdx = static_cast<int>(m_TrackSpectrumFrames.size()) - 1;
            int i0 = std::clamp(static_cast<int>(std::floor(exactFrame)), 0, maxIdx);
            int i1 = std::clamp(i0 + 1, 0, maxIdx);
            float frac = std::clamp(exactFrame - static_cast<float>(i0), 0.0f, 1.0f);
            for (int b = 0; b < 12; ++b) {
                rawBands[b] = m_TrackSpectrumFrames[i0][b] * (1.0f - frac)
                            + m_TrackSpectrumFrames[i1][b] * frac;
            }
        }
    }

    const bool eqActive = m_Main->get_music_eq_enabled() || m_Main->get_music_dynamic_eq();
    const float eqGains[5] = {
        eqActive ? ((m_Main->get_music_eq_enabled() ? m_Main->get_music_eq_60()    : 0.0f) + (m_Main->get_music_dynamic_eq() ? m_DynEqOffsets[0] : 0.0f)) : 0.0f,
        eqActive ? ((m_Main->get_music_eq_enabled() ? m_Main->get_music_eq_230()   : 0.0f) + (m_Main->get_music_dynamic_eq() ? m_DynEqOffsets[1] : 0.0f)) : 0.0f,
        eqActive ? ((m_Main->get_music_eq_enabled() ? m_Main->get_music_eq_910()   : 0.0f) + (m_Main->get_music_dynamic_eq() ? m_DynEqOffsets[2] : 0.0f)) : 0.0f,
        eqActive ? ((m_Main->get_music_eq_enabled() ? m_Main->get_music_eq_3600()  : 0.0f) + (m_Main->get_music_dynamic_eq() ? m_DynEqOffsets[3] : 0.0f)) : 0.0f,
        eqActive ? ((m_Main->get_music_eq_enabled() ? m_Main->get_music_eq_12000() : 0.0f) + (m_Main->get_music_dynamic_eq() ? m_DynEqOffsets[4] : 0.0f)) : 0.0f
    };

    float volFactor = std::clamp(m_Main->get_music_volume() / 100.0f, 0.12f, 1.25f);
    float lowSum = 0.0f, midSum = 0.0f, highSum = 0.0f;

    for (int i = 0; i < 12; ++i) {
        float targetAmp = 0.08f;
        if (hasRealSpectrum) {
            int eqIdx = std::clamp((i * 5) / 12, 0, 4);
            float eqScale = std::pow(10.0f, eqGains[eqIdx] / 28.0f);
            // Perceptual dB compression (pow 0.62) so both quiet acoustic passages and loud drums read clearly
            float norm = std::pow(std::clamp(rawBands[i] / normPeak, 0.0f, 1.25f), 0.62f);
            targetAmp = std::clamp(0.06f + norm * 0.94f * eqScale * volFactor, 0.06f, 1.0f);
        }
        // Real studio RTA ballistics: fast attack (0.72) for kicks/snares, smooth release (0.28)
        float coeff = (targetAmp > m_LiveWaveSmoothed[i]) ? 0.72f : 0.28f;
        m_LiveWaveSmoothed[i] += (targetAmp - m_LiveWaveSmoothed[i]) * coeff;
        m_MusicLiveWaveModel->set_row_data(i, m_LiveWaveSmoothed[i]);

        if (i < 3)      lowSum  += m_LiveWaveSmoothed[i];
        else if (i < 8) midSum  += m_LiveWaveSmoothed[i];
        else            highSum += m_LiveWaveSmoothed[i];
    }

    // ProyecThor Dynamic EQ Real-Time Spectral Controller (evaluates real FFT low/mid/high balance every ~0.4s)
    if (m_Main->get_music_dynamic_eq() && hasRealSpectrum && (++m_DynEqTickCounter >= 12)) {
        m_DynEqTickCounter = 0;
        float lowAvg  = lowSum / 3.0f;
        float midAvg  = midSum / 5.0f;
        float highAvg = highSum / 4.0f;
        float totalE  = (lowAvg + midAvg + highAvg) / 3.0f;

        float target[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::string statusMsg;

        if (totalE < 0.34f) {
            // Pasaje suave / íntimo real: realce psicoacústico cálido
            target[0] = 2.4f; target[1] = 1.2f; target[2] = 0.8f; target[3] = 1.4f; target[4] = 2.0f;
            statusMsg = "⚡ EQ Dinámica • Realce Íntimo (Pasaje suave: +2.4dB Graves / +2.0dB Brillo)";
        } else if (midAvg > lowAvg * 1.28f && totalE > 0.58f) {
            // Picos en medios/presencia: suaviza dureza y refuerza base
            target[0] = 1.2f; target[1] = 0.4f; target[2] = -0.6f; target[3] = -1.8f; target[4] = 0.4f;
            statusMsg = "⚡ EQ Dinámica • Control de Presencia (-1.8dB 3.6kHz / Refuerzo cálido)";
        } else if (lowAvg > midAvg * 1.35f && totalE > 0.62f) {
            // Subgraves muy cargados: limpia graves y abre claridad vocal
            target[0] = -1.8f; target[1] = -0.8f; target[2] = 0.8f; target[3] = 1.2f; target[4] = 1.0f;
            statusMsg = "⚡ EQ Dinámica • Control de Graves (-1.8dB 60Hz / +1.2dB Claridad)";
        } else {
            // Balance espectral equilibrado
            target[0] = 0.6f; target[1] = 0.0f; target[2] = 0.4f; target[3] = 0.6f; target[4] = 0.9f;
            statusMsg = "⚡ EQ Dinámica • Balance Espectral Real (FFT 12 Bandas + dynaudnorm)";
        }

        for (int b = 0; b < 5; ++b) {
            m_DynEqOffsets[b] += (target[b] - m_DynEqOffsets[b]) * 0.45f;
        }
        m_Main->set_music_dynamic_eq_status(slint::SharedString(statusMsg));
        ApplyMusicEqualizer(false);
    }
}

void SlintApp::RefreshMusicModel() {
    m_MusicTracksModel->clear();
    for (int i = 0; i < static_cast<int>(m_MusicEntries.size()); ++i) {
        MusicTrackItem item;
        item.title    = slint::SharedString(m_MusicEntries[i].title);
        item.subtitle = slint::SharedString(m_MusicEntries[i].subtitle);
        item.path     = slint::SharedString(m_MusicEntries[i].path);
        item.playing  = (i == m_CurrentMusicIdx && m_Main->get_music_playing());
        slint::Image cov;
        item.has_cover = ExtractMusicCoverArt(m_MusicEntries[i].path, cov);
        if (item.has_cover) item.cover = cov;
        m_MusicTracksModel->push_back(item);
    }
    RebuildMusicGridRows();
}

void SlintApp::RebuildMusicGridRows() {
    m_MusicGridRowsModel->clear();
    constexpr int kCols = 4;
    const int total = static_cast<int>(m_MusicTracksModel->row_count());
    for (int i = 0; i < total; i += kCols) {
        auto cells = std::make_shared<slint::VectorModel<MusicGridCell>>();
        for (int c = 0; c < kCols; ++c) {
            MusicGridCell cell;
            int idx = i + c;
            if (idx < total) {
                if (auto trk = m_MusicTracksModel->row_data(idx)) {
                    cell.index     = idx;
                    cell.title     = trk->title;
                    cell.subtitle  = trk->subtitle;
                    cell.path      = trk->path;
                    cell.playing   = trk->playing;
                    cell.cover     = trk->cover;
                    cell.has_cover = trk->has_cover;
                    cell.valid     = true;
                }
            } else {
                cell.index = -1;
                cell.valid = false;
            }
            cells->push_back(cell);
        }
        MusicGridRow row;
        row.cells = cells;
        m_MusicGridRowsModel->push_back(row);
    }
}

void SlintApp::RefreshMusicScanFoldersModel() {
    m_MusicScanFoldersModel->clear();
    for (const auto& dir : m_MusicScanFolders) {
        m_MusicScanFoldersModel->push_back(slint::SharedString(dir));
    }
}

void SlintApp::AddMusicScanFolder(const std::string& rawPath) {
    std::string p = rawPath;
    while (!p.empty() && (p.back() == '\n' || p.back() == '\r' || p.back() == ' ')) p.pop_back();
    while (!p.empty() && p.front() == ' ') p.erase(p.begin());
    if (p.empty()) return;
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) return;
    if (std::find(m_MusicScanFolders.begin(), m_MusicScanFolders.end(), p) == m_MusicScanFolders.end()) {
        m_MusicScanFolders.push_back(p);
        RefreshMusicScanFoldersModel();
        SavePersistedState();
        ScanMusicTracks();
        if (m_ActiveTab == 13) RefreshLibraryHub();
        AppendDebugLog("Carpeta de música agregada: " + p);
    }
}

void SlintApp::RemoveMusicScanFolder(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_MusicScanFolders.size())) return;
    m_MusicScanFolders.erase(m_MusicScanFolders.begin() + idx);
    RefreshMusicScanFoldersModel();
    SavePersistedState();
    ScanMusicTracks();
    if (m_ActiveTab == 13) RefreshLibraryHub();
}

void SlintApp::BrowseMusicFolderAsync() {
    std::thread([this]() {
        std::string selectedDir;
#ifdef _WIN32
        char cmd[] = "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
                     "$d = New-Object System.Windows.Forms.FolderBrowserDialog; "
                     "$d.Description = 'Selecciona una carpeta de música para ProyecThor'; "
                     "if($d.ShowDialog() -eq 'OK'){ Write-Output $d.SelectedPath }\"";
        selectedDir = RunCommandCapture(cmd);
#elif defined(__APPLE__)
        selectedDir = RunCommandCapture(
            "osascript -e 'POSIX path of (choose folder with prompt \"Selecciona una carpeta de música para ProyecThor\")' 2>/dev/null");
#else
        selectedDir = RunCommandCapture(
            "zenity --file-selection --directory --title=\"Seleccionar carpeta de música\" 2>/dev/null");
        if (selectedDir.empty()) {
            selectedDir = RunCommandCapture(
                "kdialog --getexistingdirectory ~ 2>/dev/null");
        }
#endif
        while (!selectedDir.empty() && (selectedDir.back() == '\n' || selectedDir.back() == '\r')) {
            selectedDir.pop_back();
        }
        if (selectedDir.empty()) return;
        slint::invoke_from_event_loop([this, selectedDir]() {
            AddMusicScanFolder(selectedDir);
        });
    }).detach();
}

void SlintApp::RefreshLibraryHub() {
    // Ensure lightweight caches exist for Songs (0), Media (2), Documents (10), Music (9), Bibles (1)
    auto ensureTab = [this](int tab) -> const std::vector<MediaEntry>& {
        auto it = m_CachedTabEntries.find(tab);
        if (it == m_CachedTabEntries.end()) {
            std::vector<MediaEntry> saved = std::move(m_AllEntries);
            m_AllEntries.clear();
            if (tab == 0)       ScanSongs();
            else if (tab == 2)  ScanMedia(false);
            else if (tab == 10) ScanDocuments();
            m_CachedTabEntries[tab] = m_AllEntries;
            m_AllEntries = std::move(saved);
            it = m_CachedTabEntries.find(tab);
        }
        return it->second;
    };

    const auto& songs = ensureTab(0);
    const auto& media = ensureTab(2);
    const auto& docs  = ensureTab(10);
    if (!m_MusicScanned) ScanMusicTracks();

    // Count Bible XML files cheaply without parsing
    int bibleCount = static_cast<int>(m_BiblePaths.size());
    if (bibleCount == 0) {
        std::vector<fs::path> bDirs = { ProyecThor::BiblesPath(), fs::path("assets/bibles") };
        std::vector<std::string> bSeen;
        for (const auto& d : bDirs) {
            std::error_code ec;
            if (!fs::exists(d, ec) || !fs::is_directory(d, ec)) continue;
            for (const auto& e : fs::directory_iterator(d, ec)) {
                if (e.is_regular_file(ec) && HasExtension(e.path(), { ".xml" })) {
                    std::string s = PathToUtf8(e.path().filename());
                    if (std::find(bSeen.begin(), bSeen.end(), s) == bSeen.end()) bSeen.push_back(s);
                }
            }
        }
        bibleCount = static_cast<int>(bSeen.size());
    }

    m_Main->set_hub_songs_count(static_cast<int>(songs.size()));
    m_Main->set_hub_media_count(static_cast<int>(media.size()));
    m_Main->set_hub_music_count(static_cast<int>(m_MusicEntries.size()));
    m_Main->set_hub_docs_count(static_cast<int>(docs.size()));
    m_Main->set_hub_bibles_count(bibleCount);

    m_HubSongsModel->clear();
    for (size_t i = 0; i < std::min<size_t>(6, songs.size()); ++i) {
        LibraryItem item;
        item.title    = slint::SharedString(songs[i].title);
        item.subtitle = slint::SharedString(songs[i].subtitle);
        item.path     = slint::SharedString(songs[i].path);
        m_HubSongsModel->push_back(item);
    }

    m_HubMediaModel->clear();
    for (size_t i = 0; i < std::min<size_t>(5, media.size()); ++i) {
        LibraryItem item;
        item.title    = slint::SharedString(media[i].title);
        item.subtitle = slint::SharedString(media[i].subtitle);
        item.path     = slint::SharedString(media[i].path);
        m_HubMediaModel->push_back(item);
    }

    m_HubDocsModel->clear();
    for (size_t i = 0; i < std::min<size_t>(5, docs.size()); ++i) {
        LibraryItem item;
        item.title    = slint::SharedString(docs[i].title);
        item.subtitle = slint::SharedString(docs[i].subtitle);
        item.path     = slint::SharedString(docs[i].path);
        m_HubDocsModel->push_back(item);
    }

    m_HubMusicModel->clear();
    size_t mCount = std::min<size_t>(5, m_MusicTracksModel->row_count());
    for (size_t i = 0; i < mCount; ++i) {
        if (auto trk = m_MusicTracksModel->row_data(i)) {
            m_HubMusicModel->push_back(*trk);
        }
    }
}

// ── Documents (Tab 10 - PDF & Images) ────────────────────────────────────────
void SlintApp::ScanDocuments() {
    fs::path dir = ProyecThor::DocumentsPath();
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::vector<std::string> docExts = { ".pdf", ".png", ".jpg", ".jpeg", ".webp" };

    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (!HasExtension(entry.path(), docExts)) continue;
        MediaEntry m;
        m.title      = PathToUtf8(entry.path().stem());
        m.subtitle   = HasExtension(entry.path(), { ".pdf" }) ? "Documento PDF" : "Diapositiva Imagen";
        m.path       = PathToUtf8(entry.path());
        m.isDocument = true;
        m.isImage    = !HasExtension(entry.path(), { ".pdf" });
        m_AllEntries.push_back(std::move(m));
    }
    std::sort(m_AllEntries.begin(), m_AllEntries.end(),
              [](const MediaEntry& a, const MediaEntry& b) { return a.title < b.title; });
}

void SlintApp::OpenDocumentAsync(const std::string& path, bool autoProjectFirst) {
    m_DocPagePaths.clear();
    m_DocPagesModel->clear();
    m_ActiveDocPageIdx = -1;
    m_Main->set_active_doc_page_idx(-1);
    m_Main->set_doc_title(slint::SharedString(PathToUtf8(fs::path(path).stem())));

    if (!HasExtension(path, { ".pdf" })) {
        m_DocPagePaths.push_back(path);
        DocPageItem pg;
        pg.number      = 1;
        pg.preview     = slint::Image::load_from_path(slint::SharedString(path));
        pg.has_preview = true;
        m_DocPagesModel->push_back(pg);
        m_Main->set_doc_status("1 página lista. Haz clic para proyectar.");
        if (autoProjectFirst) {
            m_Main->set_active_doc_page_idx(0);
            ProjectDocPage(0);
        }
        return;
    }

    m_Main->set_doc_status("Renderizando páginas PDF con pdftoppm...");
    std::thread([this, path, autoProjectFirst]() {
        fs::path outDir = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails" / "docs" /
                          std::to_string(std::hash<std::string>{}(path));
        std::error_code ec;
        fs::create_directories(outDir, ec);

        std::vector<std::string> pages;
        for (const auto& e : fs::directory_iterator(outDir, ec)) {
            if (e.is_regular_file(ec) && HasExtension(e.path(), { ".png" })) {
                pages.push_back(PathToUtf8(e.path()));
            }
        }
        if (pages.empty()) {
            std::string prefix = PathToUtf8(outDir / "page");
#ifdef _WIN32
            std::string cmd = "pdftoppm -png -r 140 \"" + path + "\" \"" + prefix + "\" 2>nul";
#else
            std::string cmd = "pdftoppm -png -r 140 \"" + path + "\" \"" + prefix + "\" >/dev/null 2>&1";
#endif
            std::system(cmd.c_str());
            for (const auto& e : fs::directory_iterator(outDir, ec)) {
                if (e.is_regular_file(ec) && HasExtension(e.path(), { ".png" })) {
                    pages.push_back(PathToUtf8(e.path()));
                }
            }
        }
        std::sort(pages.begin(), pages.end());

        slint::invoke_from_event_loop([this, pages = std::move(pages), autoProjectFirst]() mutable {
            m_DocPagePaths = std::move(pages);
            m_DocPagesModel->clear();
            for (size_t i = 0; i < m_DocPagePaths.size(); ++i) {
                DocPageItem pg;
                pg.number      = static_cast<int>(i + 1);
                pg.preview     = slint::Image::load_from_path(slint::SharedString(m_DocPagePaths[i]));
                pg.has_preview = true;
                m_DocPagesModel->push_back(pg);
            }
            if (m_DocPagePaths.empty()) {
                m_Main->set_doc_status("No se pudo renderizar el PDF (instala poppler/pdftoppm).");
            } else {
                m_Main->set_doc_status(slint::SharedString(
                    std::to_string(m_DocPagePaths.size()) + " página(s) lista(s). Haz clic para proyectar."));
                if (autoProjectFirst) {
                    m_Main->set_active_doc_page_idx(0);
                    ProjectDocPage(0);
                }
            }
        });
    }).detach();
}

void SlintApp::ProjectDocPage(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= static_cast<int>(m_DocPagePaths.size())) return;
    m_ActiveDocPageIdx = pageIdx;
    const std::string& p = m_DocPagePaths[pageIdx];

    m_CurrentBgPath  = p;
    slint::Image img, thumb;
    RenderScaledStaticImage(p, img);
    bool hasThumb = LoadOrCreateThumbnail(p, false, thumb);

    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (!IsScreenTargeted(i)) continue;
        m_Screens[i].bgName     = "Doc Pág. " + std::to_string(pageIdx + 1);
        m_Screens[i].bgPath     = p;
        m_Screens[i].hasBgImage = true;
        m_Screens[i].bgIsStatic = true;
        m_Screens[i].bgIsVideo  = false;
        m_Screens[i].bgImage    = img;
        m_Screens[i].hasBgThumb = hasThumb;
        if (hasThumb) m_Screens[i].bgThumb = thumb;
    }
    bool anyVid = false;
    for (const auto& sc : m_Screens) if (sc.bgIsVideo) { anyVid = true; break; }
    if (!anyVid) { m_Player.Stop(); m_HasVideoBg = false; }
    m_HasStaticImage = m_Screens[0].bgIsStatic;
    if (IsScreenTargeted(1) && IsLanRunning()) UpdateLanStaticFrame(img);
    SyncScreenOutputs();
}

// ── Unified Library File Management (Play/Screen, Queue, Rename, Delete) ────
void SlintApp::HandleItemPlay(int kind, const std::string& path) {
    if (path.empty()) return;
    const std::vector<std::string> videoExts = { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" };
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };

    if (kind == 0) {
        // Canción: abrir y proyectar la primera estrofa inmediatamente
        m_ActiveTab = 0;
        m_Main->set_active_tab(0);
        LoadTabData(0);
        for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
            if (m_AllEntries[m_FilteredIndices[i]].path == path) {
                m_Main->set_selected_library_index(i);
                SelectLibraryItem(i);
                if (m_SlidesModel->row_count() > 0) {
                    m_Main->set_active_slide_index(0);
                    ProjectSlide(0);
                }
                break;
            }
        }
    } else if (kind == 1) {
        // Multimedia (Video o Imagen): proyectar directamente en la(s) pantalla(s) destino
        fs::path fp(path);
        MediaEntry entry;
        entry.title    = PathToUtf8(fp.stem());
        entry.path     = path;
        entry.isVideo  = HasExtension(fp, videoExts);
        entry.isImage  = HasExtension(fp, imageExts);
        entry.subtitle = entry.isVideo ? "Video" : "Imagen";
        m_SelectedPreviewEntry    = entry;
        m_HasSelectedPreviewEntry = true;
        ProjectSelectedMedia();
    } else if (kind == 2) {
        // Documento (PDF o Imagen): abrir y proyectar página 1 automáticamente
        m_ActiveTab = 10;
        m_Main->set_active_tab(10);
        LoadTabData(10);
        for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
            if (m_AllEntries[m_FilteredIndices[i]].path == path) {
                m_Main->set_selected_library_index(i);
                m_Main->set_selected_library_path(slint::SharedString(path));
                break;
            }
        }
        OpenDocumentAsync(path, true);
    } else if (kind == 3) {
        // Música (Audio independiente)
        if (!m_MusicScanned) ScanMusicTracks();
        for (int i = 0; i < static_cast<int>(m_MusicEntries.size()); ++i) {
            if (m_MusicEntries[i].path == path) {
                PlayMusicTrack(i);
                break;
            }
        }
    } else if (kind == 4) {
        // Fondo en Bucle
        for (int i = 0; i < static_cast<int>(m_BgDockEntries.size()); ++i) {
            if (m_BgDockEntries[i].path == path) {
                SelectDockBackground(i);
                return;
            }
        }
        fs::path fp(path);
        MediaEntry entry;
        entry.title   = PathToUtf8(fp.stem());
        entry.path    = path;
        entry.isVideo = HasExtension(fp, videoExts);
        entry.isImage = HasExtension(fp, imageExts);
        m_SelectedPreviewEntry    = entry;
        m_HasSelectedPreviewEntry = true;
        ProjectSelectedMedia();
    } else if (kind == 5) {
        // Overlay PNG
        for (int i = 0; i < static_cast<int>(m_OverlayLayerEntries.size()); ++i) {
            if (m_OverlayLayerEntries[i].path == path) {
                SelectOverlayLayer(i);
                return;
            }
        }
    }
}

void SlintApp::HandleItemQueue(int kind, const std::string& path) {
    if (path.empty()) return;
    fs::path fp(path);
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };

    if (kind == 0) {
        std::string title, author;
        ReadSongMeta(path, title, author);
        ServiceQueueEntry q;
        q.tab   = 0;
        q.path  = path;
        q.kind  = "CANCIÓN";
        q.title = title;
        m_Queue.push_back(std::move(q));
        m_Main->set_queue_category(0);
        RefreshQueueModel();
    } else if (kind == 1 || kind == 4) {
        bool isImg = HasExtension(fp, imageExts);
        VideoQueueEntry e;
        e.title   = PathToUtf8(fp.stem());
        e.path    = path;
        e.kind    = isImg ? "Imagen" : (kind == 4 ? "Fondo" : "Video");
        e.isImage = isImg;
        m_VideoQueue.push_back(std::move(e));
        m_Main->set_queue_category(1);
        RefreshVideoQueueModel();
    } else if (kind == 2) {
        ServiceQueueEntry q;
        q.tab   = 10;
        q.path  = path;
        q.kind  = "DOC";
        q.title = PathToUtf8(fp.stem());
        m_Queue.push_back(std::move(q));
        m_Main->set_queue_category(0);
        RefreshQueueModel();
    } else if (kind == 3) {
        ServiceQueueEntry q;
        q.tab   = 9;
        q.path  = path;
        q.kind  = "MÚSICA";
        q.title = PathToUtf8(fp.stem());
        m_Queue.push_back(std::move(q));
        m_Main->set_queue_category(0);
        RefreshQueueModel();
    } else if (kind == 5) {
        OverlayQueueEntry oq;
        oq.title    = PathToUtf8(fp.stem());
        oq.path     = path;
        oq.isCustom = HasExtension(fp, { ".ptoverlay" });
        m_OverlayQueue.push_back(std::move(oq));
        m_Main->set_queue_category(2);
        RefreshOverlayQueueModel();
    }
}

void SlintApp::HandleItemRename(int kind, const std::string& path, const std::string& rawNewName) {
    if (path.empty()) return;
    std::string displayTitle = rawNewName;
    while (!displayTitle.empty() && displayTitle.front() == ' ') displayTitle.erase(displayTitle.begin());
    while (!displayTitle.empty() && displayTitle.back() == ' ')  displayTitle.pop_back();
    if (displayTitle.empty()) return;

    fs::path oldFs(path);
    std::error_code ec;
    if (!fs::exists(oldFs, ec)) return;

    std::string oldTitle, oldAuthor;
    if (kind == 0) {
        ReadSongMeta(path, oldTitle, oldAuthor);
    }

    std::string safeStem = SanitizeFilename(displayTitle);
    std::string ext      = PathToUtf8(oldFs.extension());
    fs::path newFs       = oldFs.parent_path() / (safeStem + ext);

    bool wasPlayingVideo = false;
    bool wasVideoLoop    = m_Main->get_media_loop();
    bool wasVideoMuted   = m_Main->get_media_muted();
    bool wasPlayingMusic = false;

    if (m_HoverVideoPath == path) {
        StopHoverVideoPreview();
    }

    if (newFs != oldFs) {
        if (fs::exists(newFs, ec)) {
            for (int n = 1; n < 1000; ++n) {
                fs::path candidate = oldFs.parent_path() / (safeStem + "_" + std::to_string(n) + ext);
                if (!fs::exists(candidate, ec)) {
                    newFs = candidate;
                    break;
                }
            }
        }
        if (m_HasVideoBg && m_CurrentBgPath == path) {
            wasPlayingVideo = true;
            m_Player.Stop();
        }
        if (m_CurrentMusicIdx >= 0 && m_CurrentMusicIdx < static_cast<int>(m_MusicEntries.size()) &&
            m_MusicEntries[m_CurrentMusicIdx].path == path && m_Main->get_music_playing()) {
            wasPlayingMusic = true;
            m_MusicPlayer.Stop();
        }

        fs::rename(oldFs, newFs, ec);
        if (ec) {
            AppendDebugLog("Error al renombrar archivo: " + ec.message());
            return;
        }
    }

    std::string newPath  = PathToUtf8(newFs);
    std::string newTitle = (kind == 0) ? displayTitle : PathToUtf8(newFs.stem());

    // Update song metadata sidecar and playlists
    if (kind == 0) {
        if (newPath != path) {
            std::string oldMetaName = PathToUtf8(oldFs.filename());
            for (char& ch : oldMetaName) { if (ch == ':') ch = '_'; }
            fs::path oldMetaPath = fs::path(ProyecThor::GetAssetsPath()) / "songs_meta" / (oldMetaName + ".json");
            fs::remove(oldMetaPath, ec);
        }
        WriteSongMeta(newPath, displayTitle, oldAuthor);

        bool plChanged = false;
        for (auto& pl : m_SongPlaylists) {
            for (auto& sp : pl.songPaths) {
                if (sp == path) { sp = newPath; plChanged = true; }
            }
        }
        if (plChanged) SaveSongPlaylists();
        if (m_EditingSongPath == path) {
            m_EditingSongPath = newPath;
            m_Main->set_editor_song_title(slint::SharedString(displayTitle));
        }
    }

    // Update queues, live screens, thumbnails and preview references
    for (auto& q : m_Queue) {
        if (q.path == path) { q.path = newPath; q.title = newTitle; }
    }
    for (auto& vq : m_VideoQueue) {
        if (vq.path == path) { vq.path = newPath; vq.title = newTitle; }
    }
    for (auto& oq : m_OverlayQueue) {
        if (oq.path == path) { oq.path = newPath; oq.title = newTitle; }
    }
    for (auto& sc : m_Screens) {
        if (sc.bgPath == path)      { sc.bgPath = newPath;      sc.bgName = newTitle; }
        if (sc.overlayPath == path) { sc.overlayPath = newPath; sc.overlayName = newTitle; }
        if (sc.hasCustomOverlay && sc.customOverlay.path == path) {
            sc.customOverlay.path = newPath;
            sc.customOverlay.name = displayTitle;
        }
    }
    if (kind == 5 && HasExtension(newFs, { ".ptoverlay" })) {
        for (auto& cov : m_CustomOverlays) {
            if (cov.path == path) {
                cov.path = newPath;
                cov.name = displayTitle;
                SaveCustomOverlayToDisk(cov);
                newTitle = displayTitle;
            }
        }
        if (m_OverlayEditorOpen && m_EditingOverlay.path == path) {
            m_EditingOverlay.path = newPath;
            m_EditingOverlay.name = displayTitle;
            m_Main->set_ov_ed_name(slint::SharedString(displayTitle));
        }
    }
    if (m_CurrentBgPath == path)     m_CurrentBgPath = newPath;
    if (m_ActiveOverlayPath == path) m_ActiveOverlayPath = newPath;
    if (m_StaticSrcPath == path)     m_StaticSrcPath = newPath;
    if (m_HasSelectedPreviewEntry && m_SelectedPreviewEntry.path == path) {
        m_SelectedPreviewEntry.path  = newPath;
        m_SelectedPreviewEntry.title = newTitle;
        m_Main->set_selected_item_title(slint::SharedString(newTitle));
        m_Main->set_selected_library_path(slint::SharedString(newPath));
        m_Main->set_selected_media_info(slint::SharedString(
            m_SelectedPreviewEntry.subtitle + " • " + PathToUtf8(newFs.filename())));
    }
    auto thIt = m_ThumbCache.find(path);
    if (thIt != m_ThumbCache.end()) {
        slint::Image img = thIt->second;
        m_ThumbCache.erase(thIt);
        m_ThumbCache[newPath] = img;
    }

    if (wasPlayingVideo) {
        m_Player.Play(newPath, wasVideoLoop, wasVideoMuted);
        ApplyShaders();
    }
    if (wasPlayingMusic) {
        if (m_CurrentMusicIdx >= 0 && m_CurrentMusicIdx < static_cast<int>(m_MusicEntries.size())) {
            m_MusicEntries[m_CurrentMusicIdx].path  = newPath;
            m_MusicEntries[m_CurrentMusicIdx].title = newTitle;
        }
        m_MusicPlayer.Play(newPath, m_Main->get_music_loop(), false);
        m_Main->set_music_now_playing(slint::SharedString(newTitle));
        ApplyMusicEqualizer(true);
    }

    RefreshAllLibraryViewsAfterMutation(kind, newPath);
    AppendDebugLog("Renombrado: " + PathToUtf8(oldFs.filename()) + " -> " + PathToUtf8(newFs.filename()));
}

void SlintApp::HandleItemDelete(int kind, const std::string& path) {
    if (path.empty()) return;
    fs::path targetFs(path);

    if (m_HoverVideoPath == path) {
        StopHoverVideoPreview();
    }

    // Stop & clear live layers referencing this file
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        if (m_Screens[i].bgPath == path)      RemoveLayerOnScreen(i, "bg");
        if (m_Screens[i].overlayPath == path) RemoveLayerOnScreen(i, "overlay");
        if (m_Screens[i].hasCustomOverlay && m_Screens[i].customOverlay.path == path) {
            RemoveLayerOnScreen(i, "clock");
        }
    }
    if (m_CurrentBgPath == path) {
        m_Player.Stop();
        m_HasVideoBg     = false;
        m_HasStaticImage = false;
        m_CurrentBgPath.clear();
    }
    if (m_ActiveOverlayPath == path) {
        m_ActiveOverlayPath.clear();
    }
    if (m_CurrentMusicIdx >= 0 && m_CurrentMusicIdx < static_cast<int>(m_MusicEntries.size()) &&
        m_MusicEntries[m_CurrentMusicIdx].path == path) {
        RemoveLayerOnScreen(0, "music");
    }
    if (m_HasSelectedPreviewEntry && m_SelectedPreviewEntry.path == path) {
        m_HasSelectedPreviewEntry = false;
        m_Main->set_has_selected_media_preview(false);
        m_Main->set_selected_media_info("");
    }
    if (m_EditingSongPath == path) {
        m_EditingSongPath.clear();
        m_Main->set_song_editor_open(false);
    }
    if (kind == 2 && std::string(m_Main->get_selected_library_path()) == path) {
        m_DocPagePaths.clear();
        m_DocPagesModel->clear();
        m_ActiveDocPageIdx = -1;
        m_Main->set_active_doc_page_idx(-1);
        m_Main->set_doc_title("Documentos — Selecciona un PDF o imagen");
        m_Main->set_doc_status("");
    }

    // Delete file on disk
    std::error_code ec;
    fs::remove(targetFs, ec);
    if (ec) {
        AppendDebugLog("Error al eliminar " + PathToUtf8(targetFs.filename()) + ": " + ec.message());
    }

    // Remove song metadata & playlist links
    if (kind == 0) {
        std::string fname = PathToUtf8(targetFs.filename());
        for (char& ch : fname) { if (ch == ':') ch = '_'; }
        fs::path metaPath = fs::path(ProyecThor::GetAssetsPath()) / "songs_meta" / (fname + ".json");
        fs::remove(metaPath, ec);

        bool plChanged = false;
        for (auto& pl : m_SongPlaylists) {
            auto oldSz = pl.songPaths.size();
            pl.songPaths.erase(std::remove(pl.songPaths.begin(), pl.songPaths.end(), path), pl.songPaths.end());
            if (pl.songPaths.size() != oldSz) plChanged = true;
        }
        if (plChanged) {
            SaveSongPlaylists();
            RefreshSongPlaylistsModel();
        }
    }

    // Remove from all queues and thumbnail cache
    m_Queue.erase(std::remove_if(m_Queue.begin(), m_Queue.end(),
        [&](const ServiceQueueEntry& q) { return q.path == path; }), m_Queue.end());

    for (int i = static_cast<int>(m_VideoQueue.size()) - 1; i >= 0; --i) {
        if (m_VideoQueue[i].path == path) {
            m_VideoQueue.erase(m_VideoQueue.begin() + i);
            if (m_VideoQueueCurrentIdx == i) {
                m_VideoQueueCurrentIdx = -1;
                m_VideoQueuePlaying    = false;
                m_Main->set_video_queue_playing(false);
            } else if (m_VideoQueueCurrentIdx > i) {
                --m_VideoQueueCurrentIdx;
            }
        }
    }
    m_OverlayQueue.erase(std::remove_if(m_OverlayQueue.begin(), m_OverlayQueue.end(),
        [&](const OverlayQueueEntry& oq) { return oq.path == path; }), m_OverlayQueue.end());
    m_ThumbCache.erase(path);

    RefreshAllLibraryViewsAfterMutation(kind, "");
    AppendDebugLog("Archivo eliminado: " + PathToUtf8(targetFs.filename()));
}

void SlintApp::RefreshAllLibraryViewsAfterMutation(int kind, const std::string& selectPathAfter) {
    if (kind == 0)      m_CachedTabEntries.erase(0);
    else if (kind == 1) m_CachedTabEntries.erase(2);
    else if (kind == 2) m_CachedTabEntries.erase(10);

    if (kind == 4) RefreshBgDock();
    if (kind == 5) {
        LoadCustomOverlaysFromDisk();
        RefreshOverlayLayers();
        RefreshOverlayQueueModel();
    }
    if (kind == 3) {
        std::string playingPath;
        if (m_CurrentMusicIdx >= 0 && m_CurrentMusicIdx < static_cast<int>(m_MusicEntries.size())) {
            playingPath = m_MusicEntries[m_CurrentMusicIdx].path;
        }
        ScanMusicTracks();
        m_CurrentMusicIdx = -1;
        if (!playingPath.empty() && m_Main->get_music_playing()) {
            for (int i = 0; i < static_cast<int>(m_MusicEntries.size()); ++i) {
                if (m_MusicEntries[i].path == playingPath) {
                    m_CurrentMusicIdx = i;
                    break;
                }
            }
            RefreshMusicModel();
        }
    }

    if ((kind == 0 && m_ActiveTab == 0) ||
        (kind == 1 && m_ActiveTab == 2) ||
        (kind == 2 && m_ActiveTab == 10)) {
        LoadTabData(m_ActiveTab);
        if (!selectPathAfter.empty()) {
            for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
                if (m_AllEntries[m_FilteredIndices[i]].path == selectPathAfter) {
                    m_Main->set_selected_library_index(i);
                    SelectLibraryItem(i);
                    break;
                }
            }
        }
    }

    RefreshQueueModel();
    RefreshVideoQueueModel();
    RefreshOverlayQueueModel();
    RefreshLibraryHub();
    SyncScreenOutputs();
}

// ── Changelog, Debug & Wi-Fi Firewall Diagnostics ────────────────────────────
void SlintApp::LoadChangelogText() {
    std::ostringstream oss;
    oss << "=== VERSIÓN " << PROYECTHOR_VERSION_STRING << " (MOTOR SLINT + MULTIPLATAFORMA NATIVA) ===\n"
        << "• Nuevo motor nativo Slint 1.19 + C++20 + Async libmpv.\n"
        << "• Soporte nativo completo: Windows x64, Windows ARM64, macOS Apple Silicon (Metal + VideoToolbox), macOS Intel y Linux.\n"
        << "• Búsqueda progresiva en Biblia (Libro → Capítulo → Verso) y búsqueda por verso con resaltado y autor en Canciones.\n"
        << "• Tutorial interactivo de bienvenida y Centro de Soporte y Comunidad con códigos QR (Web, Discord, Instagram, WhatsApp).\n"
        << "• Matriz Multi-Pantalla, Capas Activas, Botón Freeze, Escalado FSR y Redes LAN/Smart TV/Celular independientes.\n\n";

    const auto& reg = ProyecThor::UI::GetPatchNotesRegistry();
    for (size_t i = 0; i < std::min<size_t>(5, reg.size()); ++i) {
        oss << "=== " << reg[i].modalBadge << " ===\n" << reg[i].summary << "\n\n";
    }
    m_Main->set_changelog_text(slint::SharedString(oss.str()));
}

void SlintApp::AppendDebugLog(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    char ts[16];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", &lt);
    m_DebugLogLines.push_back(std::string("[") + ts + "] " + msg);
    if (m_DebugLogLines.size() > 120) m_DebugLogLines.erase(m_DebugLogLines.begin());

    std::ostringstream oss;
    for (size_t i = 0; i < m_DebugLogLines.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << m_DebugLogLines[i];
    }
    m_Main->set_debug_log(slint::SharedString(oss.str()));
}

void SlintApp::SampleAppTelemetry() {
    auto now = std::chrono::steady_clock::now();
    if (m_PerfLastSampleTime.time_since_epoch().count() == 0) {
        m_PerfLastSampleTime = now;
#ifdef _WIN32
        FILETIME ftC, ftE, ftK, ftU;
        if (GetProcessTimes(GetCurrentProcess(), &ftC, &ftE, &ftK, &ftU)) {
            ULARGE_INTEGER k, u;
            k.LowPart = ftK.dwLowDateTime; k.HighPart = ftK.dwHighDateTime;
            u.LowPart = ftU.dwLowDateTime; u.HighPart = ftU.dwHighDateTime;
            m_PerfLastCpuNs = (k.QuadPart + u.QuadPart) * 100ULL;
        }
#elif defined(__APPLE__)
        rusage ru{};
        if (getrusage(RUSAGE_SELF, &ru) == 0) {
            uint64_t userNs = static_cast<uint64_t>(ru.ru_utime.tv_sec) * 1000000000ULL +
                              static_cast<uint64_t>(ru.ru_utime.tv_usec) * 1000ULL;
            uint64_t sysNs  = static_cast<uint64_t>(ru.ru_stime.tv_sec) * 1000000000ULL +
                              static_cast<uint64_t>(ru.ru_stime.tv_usec) * 1000ULL;
            m_PerfLastCpuNs = userNs + sysNs;
        }
#else
        std::ifstream statF("/proc/self/stat");
        std::string line;
        if (std::getline(statF, line)) {
            auto rparen = line.rfind(')');
            if (rparen != std::string::npos) {
                std::istringstream iss(line.substr(rparen + 2));
                std::string tok;
                uint64_t utime = 0, stime = 0;
                for (int idx = 3; idx <= 15 && (iss >> tok); ++idx) {
                    if (idx == 14) utime = std::strtoull(tok.c_str(), nullptr, 10);
                    else if (idx == 15) stime = std::strtoull(tok.c_str(), nullptr, 10);
                }
                long clkTck = sysconf(_SC_CLK_TCK);
                if (clkTck > 0) {
                    m_PerfLastCpuNs = ((utime + stime) * 1000000000ULL) / static_cast<uint64_t>(clkTck);
                }
            }
        }
#endif
        return;
    }

    double elapsed = std::chrono::duration<double>(now - m_PerfLastSampleTime).count();
    if (elapsed < 0.5) return;

    m_PerfFps      = static_cast<double>(m_PerfTickFrames) / elapsed;
    m_PerfVideoFps = static_cast<double>(m_PerfVideoFrames) / elapsed;
    m_PerfTickFrames  = 0;
    m_PerfVideoFrames = 0;
    m_PerfLastSampleTime = now;

    unsigned int cores = std::max(1u, std::thread::hardware_concurrency());

#ifdef _WIN32
    FILETIME ftC, ftE, ftK, ftU;
    if (GetProcessTimes(GetCurrentProcess(), &ftC, &ftE, &ftK, &ftU)) {
        ULARGE_INTEGER k, u;
        k.LowPart = ftK.dwLowDateTime; k.HighPart = ftK.dwHighDateTime;
        u.LowPart = ftU.dwLowDateTime; u.HighPart = ftU.dwHighDateTime;
        uint64_t curNs = (k.QuadPart + u.QuadPart) * 100ULL;
        if (curNs >= m_PerfLastCpuNs) {
            double cpuSec = static_cast<double>(curNs - m_PerfLastCpuNs) / 1e9;
            m_PerfCpuPercent = std::clamp((cpuSec / (elapsed * cores)) * 100.0, 0.0, 100.0);
        }
        m_PerfLastCpuNs = curNs;
    }
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_PerfRamMb     = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        m_PerfPeakRamMb = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#elif defined(__APPLE__)
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        uint64_t userNs = static_cast<uint64_t>(ru.ru_utime.tv_sec) * 1000000000ULL +
                          static_cast<uint64_t>(ru.ru_utime.tv_usec) * 1000ULL;
        uint64_t sysNs  = static_cast<uint64_t>(ru.ru_stime.tv_sec) * 1000000000ULL +
                          static_cast<uint64_t>(ru.ru_stime.tv_usec) * 1000ULL;
        uint64_t curNs  = userNs + sysNs;
        if (curNs >= m_PerfLastCpuNs) {
            double cpuSec = static_cast<double>(curNs - m_PerfLastCpuNs) / 1e9;
            m_PerfCpuPercent = std::clamp((cpuSec / (elapsed * cores)) * 100.0, 0.0, 100.0);
        }
        m_PerfLastCpuNs = curNs;
    }
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        m_PerfRamMb     = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
        m_PerfPeakRamMb = static_cast<double>(info.resident_size_max) / (1024.0 * 1024.0);
    }
#else
    // CPU time from /proc/self/stat
    {
        std::ifstream statF("/proc/self/stat");
        std::string line;
        if (std::getline(statF, line)) {
            auto rparen = line.rfind(')');
            if (rparen != std::string::npos) {
                std::istringstream iss(line.substr(rparen + 2));
                std::string tok;
                uint64_t utime = 0, stime = 0;
                for (int idx = 3; idx <= 15 && (iss >> tok); ++idx) {
                    if (idx == 14) utime = std::strtoull(tok.c_str(), nullptr, 10);
                    else if (idx == 15) stime = std::strtoull(tok.c_str(), nullptr, 10);
                }
                long clkTck = sysconf(_SC_CLK_TCK);
                if (clkTck > 0) {
                    uint64_t curNs = ((utime + stime) * 1000000000ULL) / static_cast<uint64_t>(clkTck);
                    if (curNs >= m_PerfLastCpuNs) {
                        double cpuSec = static_cast<double>(curNs - m_PerfLastCpuNs) / 1e9;
                        m_PerfCpuPercent = std::clamp((cpuSec / (elapsed * cores)) * 100.0, 0.0, 100.0);
                    }
                    m_PerfLastCpuNs = curNs;
                }
            }
        }
    }
    // RAM & Threads from /proc/self/status
    {
        std::ifstream statusF("/proc/self/status");
        std::string line;
        while (std::getline(statusF, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                double kb = std::strtod(line.c_str() + 6, nullptr);
                m_PerfRamMb = kb / 1024.0;
            } else if (line.rfind("VmHWM:", 0) == 0) {
                double kb = std::strtod(line.c_str() + 6, nullptr);
                m_PerfPeakRamMb = kb / 1024.0;
            } else if (line.rfind("Threads:", 0) == 0) {
                m_PerfThreads = static_cast<int>(std::strtol(line.c_str() + 8, nullptr, 10));
            }
        }
    }
#endif

    if (m_ActiveTab == 12 || m_ActiveTab == 7) {
        RefreshDebugInfo();
    }
}

void SlintApp::RefreshDebugInfo() {
    static const char* kScaleNames[] = { "Recortar (Cover)", "Estirar (Fill)", "Original (Contain)" };
    static const char* kUpNames[]    = { "Off (Bilineal)", "AMD FSR 1.0", "FidelityFX CAS+", "NVIDIA NIS", "LanczosHQ 4K" };

    // Show real rendered video FPS when video is active (event-driven 30/60/120 FPS), or idle tick FPS otherwise
    const double primaryFps = (m_HasVideoBg && m_Player.IsPlaying() && !m_Player.IsPaused())
        ? m_PerfVideoFps
        : m_PerfFps;
    const double srcFps = m_Player.GetSourceFps();
    const std::string activeHw = m_Player.GetActiveHwdec();

    std::ostringstream fpsStr, vidSubStr, cpuStr, ramStr, peakStr;
    fpsStr  << std::fixed << std::setprecision(1) << primaryFps;
    cpuStr  << std::fixed << std::setprecision(1) << m_PerfCpuPercent << "%";
    ramStr  << std::fixed << std::setprecision(1) << m_PerfRamMb << " MB";
    peakStr << std::fixed << std::setprecision(1) << m_PerfPeakRamMb << " MB";

    if (m_HasVideoBg && m_Player.IsPlaying()) {
        vidSubStr << "UI " << static_cast<int>(std::round(m_PerfFps)) << " • Límite " << m_TargetFps;
        if (srcFps > 0.1) {
            vidSubStr << " • Clip " << static_cast<int>(std::round(srcFps)) << "fps";
        }
    } else {
        vidSubStr << "UI • Límite " << m_TargetFps;
    }

    m_Main->set_debug_fps_text(slint::SharedString(fpsStr.str()));
    m_Main->set_debug_video_fps_text(slint::SharedString(vidSubStr.str()));
    m_Main->set_debug_cpu_text(slint::SharedString(cpuStr.str()));
    m_Main->set_debug_ram_text(slint::SharedString(ramStr.str()));
    m_Main->set_debug_peak_ram_text(slint::SharedString(
        m_PerfThreads > 0 ? ("Pico " + peakStr.str() + " • " + std::to_string(m_PerfThreads) + " hilos")
                          : ("Pico " + peakStr.str())));

    const char* platTag =
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
        "macOS Apple Silicon (arm64 • Metal)";
#elif defined(__APPLE__)
        "macOS Intel (x86_64 • Metal)";
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
        "Windows ARM64 (Direct3D)";
#elif defined(_WIN32)
        "Windows x64";
#elif defined(__aarch64__)
        "Linux ARM64";
#else
        "Linux x86_64";
#endif
    std::ostringstream st;
    st << "Versión: v" << PROYECTHOR_VERSION_STRING << " (" << platTag << " • Slint 1.19 + Async libmpv)  |  "
       << "FPS: " << fpsStr.str() << " (" << vidSubStr.str() << ")  |  CPU: " << cpuStr.str()
       << "  |  RAM: " << ramStr.str() << " (Pico " << peakStr.str() << ")\n"
       << "GPU: " << m_DetectedGpuName << "  |  HWDec Activo: " << activeHw
       << "  |  NVENC: " << (m_NvencEnabled ? (m_NvencAvailable ? "ON" : "Fallback") : "OFF") << "\n"
       << "Reescalado: " << kUpNames[std::clamp(m_UpscalerMode, 0, 4)]
       << " (" << static_cast<int>(std::round(m_UpscalerSharpness * 100.0f)) << "%)"
       << "  |  Escalado: " << kScaleNames[std::clamp(m_BgScaleMode, 0, 2)]
       << "  |  24→60 FPS: " << (m_SmoothMotion60 ? "ON" : "OFF") << "\n"
       << "Público: " << (m_ProjectorVisible ? "ACTIVO" : "OFF") << "  |  "
       << "Freeze: " << (m_AudienceFrozen ? "CONGELADO" : "NO")
       << "  |  Modo Ultra-Ligero: " << (m_Main->get_performance_mode() ? "ON" : "OFF") << "\n"
       << "Video LAN (8080): " << (IsLanRunning() ? m_LanServer->GetBaseURL() : "OFF")
       << "  |  Chat LAN (8082): " << (m_ChatServer.IsRunning() ? "ON" : "OFF")
       << "  |  Celular (8090): " << (m_MobileServer.IsRunning() ? "ON" : "OFF") << "\n"
       << "Reproductor Video: " << (m_Player.IsPlaying() ? "PLAYING" : "IDLE")
       << "  |  Reproductor Música: " << (m_MusicPlayer.IsPlaying() ? "PLAYING" : "IDLE")
       << "  |  Miniaturas en RAM: " << m_ThumbCache.size();
    m_Main->set_debug_stats(slint::SharedString(st.str()));

    std::ostringstream p;
    p << "AppData: " << ProyecThor::GetAppDataRoot() << "\n"
      << "Songs: " << ProyecThor::SongsPath() << "  |  Bibles: " << ProyecThor::BiblesPath() << "\n"
      << "Videos: " << ProyecThor::VideosPath() << "  |  Documents: " << ProyecThor::DocumentsPath();
    m_Main->set_debug_paths(slint::SharedString(p.str()));
}

void SlintApp::CheckFirewallStatusAsync() {
    std::thread([this]() {
        bool warn = false;
        std::string msg;
#ifdef _WIN32
        msg = "Windows: Si otros dispositivos en Wi-Fi no pueden abrir la URL LAN, pulsa 'Abrir Puertos' para crear la regla en Windows Defender Firewall.";
        warn = false;
#elif defined(__APPLE__)
        msg = "macOS: Si activaste el Firewall de macOS, permite conexiones entrantes para ProyecThor en Configuración del Sistema → Red → Firewall.";
        warn = false;
#else
        std::string ufwState = RunCommandCapture("systemctl is-active ufw 2>/dev/null");
        while (!ufwState.empty() && (ufwState.back() == '\n' || ufwState.back() == '\r')) ufwState.pop_back();
        if (ufwState == "active") {
            warn = true;
            msg = "ATENCIÓN: El firewall UFW está ACTIVO en este Linux y por defecto bloquea el acceso desde otros dispositivos Wi-Fi/Smart TV a los puertos 8080, 8082 y 8090. Pulsa 'Abrir Puertos' para permitirlos.";
        } else {
            msg = "Firewall UFW inactivo — los puertos 8080, 8082 y 8090 están accesibles en tu red local Wi-Fi.";
        }
#endif
        slint::invoke_from_event_loop([this, warn, msg]() {
            m_Main->set_lan_firewall_warning(warn);
            m_Main->set_lan_firewall_status(slint::SharedString(msg));
        });
    }).detach();
}

void SlintApp::OpenFirewallPortsAsync() {
    m_Main->set_lan_firewall_status("Solicitando permiso para abrir puertos 8080, 8082, 8090 (TCP) y 47990 (UDP)...");
    std::thread([this]() {
#ifdef _WIN32
        std::system("netsh advfirewall firewall add rule name=\"ProyecThor LAN\" dir=in action=allow protocol=TCP localport=8080,8082,8090");
        std::system("netsh advfirewall firewall add rule name=\"ProyecThor UDP\" dir=in action=allow protocol=UDP localport=47990");
        int rc = 0;
#elif defined(__APPLE__)
        int rc = std::system("open 'x-apple.systempreferences:com.apple.Network-Settings.extension' >/dev/null 2>&1");
#else
        int rc = std::system("pkexec sh -c \"ufw allow 8080/tcp && ufw allow 8082/tcp && ufw allow 8090/tcp && ufw allow 47990/udp\"");
#endif
        slint::invoke_from_event_loop([this, rc]() {
            if (rc == 0) {
                m_Main->set_lan_firewall_warning(false);
                m_Main->set_lan_firewall_status("¡Puertos 8080, 8082, 8090 (TCP) y 47990 (UDP) abiertos correctamente! Ya puedes acceder desde otros dispositivos en Wi-Fi.");
                AppendDebugLog("Puertos de firewall abiertos (8080, 8082, 8090, 47990).");
            } else {
                m_Main->set_lan_firewall_status("No se pudieron abrir los puertos automáticamente. Ejecuta en tu terminal: sudo ufw allow 8080,8082,8090/tcp && sudo ufw allow 47990/udp");
            }
        });
    }).detach();
}

// ── Multi-Screen / Multi-Room Matrix & Photoshop-Style Real Active Layers ────
void SlintApp::InitMultiScreens() {
    m_Screens.clear();

    ScreenOutputState pub;
    pub.id           = 0;
    pub.name         = "Público Principal";
    pub.roleBadge    = "HDMI / VENTANA";
    pub.isCustomRoom = false;
    m_Screens.push_back(std::move(pub));

    ScreenOutputState lan;
    lan.id           = 1;
    lan.name         = "LAN / Smart TV (8080)";
    lan.roleBadge    = "RED WI-FI / LAN";
    lan.isCustomRoom = false;
    m_Screens.push_back(std::move(lan));

    ScreenOutputState stg;
    stg.id                 = 2;
    stg.name               = "Stage / Escenario";
    stg.roleBadge          = "MONITOR ESCENARIO";
    stg.isCustomRoom       = false;
    stg.textStyle.textSize = 40.0f;
    m_Screens.push_back(std::move(stg));

    ScreenOutputState room2;
    room2.id           = 3;
    room2.name         = "Sala 2 / Lobby";
    room2.roleBadge    = "SALA INDEPENDIENTE";
    room2.isCustomRoom = true;
    m_Screens.push_back(std::move(room2));

    m_OutputTargetMode   = 0;
    m_InspectedScreenIdx = 0;
    m_StyleTargetMode    = 0;
    m_Main->set_output_target_mode(0);
    m_Main->set_inspected_screen_idx(0);
    m_Main->set_inspected_screen_name("Público");
    m_Main->set_style_target_mode(0);
    m_Main->set_style_target_label("Todas las pantallas");
}

// ── Monitores físicos: detección + selección de pantalla de proyección ───────
void SlintApp::RefreshMonitorList() {
    m_Monitors = EnumerateMonitors();

    std::vector<slint::SharedString> labels;
    labels.reserve(m_Monitors.size());
    for (size_t i = 0; i < m_Monitors.size(); ++i) {
        const auto& m = m_Monitors[i];
        std::string label = "Pantalla " + std::to_string(i + 1)
                          + " — " + std::to_string(m.width) + "×" + std::to_string(m.height);
        if (m.isPrimary) label += " (Principal)";
        labels.emplace_back(label);
    }
    auto model = std::make_shared<slint::VectorModel<slint::SharedString>>(labels);
    m_Main->set_monitor_names(model);

    // Por defecto prioriza la pantalla secundaria, salvo que el usuario ya
    // haya elegido una válida.
    if (m_ProjectionMonitorIdx < 0 ||
        m_ProjectionMonitorIdx >= static_cast<int>(m_Monitors.size())) {
        m_ProjectionMonitorIdx = DefaultProjectionMonitorIndex(m_Monitors);
    }
    m_Main->set_projection_monitor_idx(m_ProjectionMonitorIdx);
}

void SlintApp::PositionProjectorOnSelectedMonitor() {
    if (m_ProjectionMonitorIdx < 0 ||
        m_ProjectionMonitorIdx >= static_cast<int>(m_Monitors.size())) {
        return; // sin info de monitores: deja que el compositor decida
    }
    const auto& m = m_Monitors[m_ProjectionMonitorIdx];
    // Coloca la ventana en la esquina del monitor elegido antes de ir a
    // pantalla completa, para que el fullscreen cubra ese monitor y no otro.
    m_Projector->window().set_position(
        slint::PhysicalPosition(slint::Point<int32_t>{ m.x, m.y }));
}

bool SlintApp::IsScreenTargeted(int screenIdx) const {
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return false;
    if (m_OutputTargetMode == 0) return true; // 0 = Todas las pantallas
    return screenIdx == (m_OutputTargetMode - 1);
}

std::string SlintApp::GetActiveClockOverlayString() const {
    if (!m_Screens.empty()) {
        int idx = std::clamp(m_InspectedScreenIdx, 0, static_cast<int>(m_Screens.size()) - 1);
        if (m_Screens[idx].hasCustomOverlay && m_Screens[idx].customOverlay.showClock) {
            return FormatOverlayClockText(m_Screens[idx].customOverlay);
        }
    }
    CustomOverlayConfig def;
    return FormatOverlayClockText(def);
}

void SlintApp::SyncClockOverlay() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &tt);
#else
    localtime_r(&tt, &localTm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &localTm);
    std::string nowStr(buf);

    if (m_OverlayEditorOpen) {
        m_Main->set_ov_ed_clock_preview(slint::SharedString(FormatOverlayClockText(m_EditingOverlay)));
    }

    if (!m_Screens.empty() && !m_AudienceFrozen) {
        const auto& pub = m_Screens[0];
        m_Projector->set_show_custom_overlay(pub.hasCustomOverlay);
        m_Projector->set_ov_show_clock(pub.customOverlay.showClock);
        m_Projector->set_ov_clock_text(slint::SharedString(FormatOverlayClockText(pub.customOverlay)));
    }
    if (m_Screens.size() > 2) {
        m_Stage->set_clock_text(slint::SharedString(nowStr));
        const auto& stg = m_Screens[2];
        bool showStgTimer = stg.hasCustomOverlay && stg.customOverlay.showClock;
        m_Stage->set_timer_text(showStgTimer ? slint::SharedString(FormatOverlayClockText(stg.customOverlay)) : "");
    }
    if (m_InspectedScreenIdx >= 0 && m_InspectedScreenIdx < static_cast<int>(m_Screens.size())) {
        const auto& insp = m_Screens[m_InspectedScreenIdx];
        std::string clk = FormatOverlayClockText(insp.customOverlay);
        m_Main->set_preview_ov_clock_text(slint::SharedString(clk));
        m_Main->set_preview_clock_text(slint::SharedString(clk));
        m_Main->set_preview_show_clock(insp.hasCustomOverlay && insp.customOverlay.showClock);
    }
    SyncLanFromScreenState();
}

void SlintApp::SyncLanFromScreenState() {
    if (m_Screens.size() <= 1) return;
    const auto& lan = m_Screens[1];

    std::string composedText;
    if (!lan.displayText.empty()) {
        composedText = lan.displayText;
    }
    if (!lan.alertText.empty()) {
        if (!composedText.empty()) composedText += "\n";
        composedText += "★ " + lan.alertText + " ★";
    }

    static const int kFontWeights[] = { 400, 700, 900 };
    const auto& st = lan.textStyle;
    std::string resolvedFont = ResolveStyleFontName(st, false);
    if (resolvedFont.empty()) resolvedFont = "Predeterminada";
    std::string fontPath = ResolveFontFilePath(resolvedFont);

    float posX = std::clamp(st.lyricsPosX, 0.05f, 0.95f);
    float posY = std::clamp(st.lyricsPosY, 0.05f, 0.95f);
    float w    = std::clamp(st.lyricsWidth, 0.15f, 1.0f);
    float h    = std::clamp(st.lyricsHeight, 0.08f, 1.0f);
    int   wt   = kFontWeights[std::clamp(st.fontWtIdx, 0, 2)];

    ProyecThor::Core::OverlaySnapshot ovSnap;
    ovSnap.active = lan.hasCustomOverlay;
    if (lan.hasCustomOverlay) {
        const auto& cov = lan.customOverlay;
        ovSnap.showClock      = cov.showClock;
        ovSnap.clockText      = FormatOverlayClockText(cov);
        ovSnap.titleText      = cov.titleText;
        ovSnap.subText        = cov.subText;
        ovSnap.textColor[0]   = cov.textR / 255.0f;
        ovSnap.textColor[1]   = cov.textG / 255.0f;
        ovSnap.textColor[2]   = cov.textB / 255.0f;
        ovSnap.bgColor[0]     = cov.bgR / 255.0f;
        ovSnap.bgColor[1]     = cov.bgG / 255.0f;
        ovSnap.bgColor[2]     = cov.bgB / 255.0f;
        ovSnap.bgOpacity      = cov.bgOpacity;
        ovSnap.showBorder     = cov.showBorder;
        ovSnap.borderColor[0] = cov.borderR / 255.0f;
        ovSnap.borderColor[1] = cov.borderG / 255.0f;
        ovSnap.borderColor[2] = cov.borderB / 255.0f;
        ovSnap.fontSize       = cov.fontSize;
        ovSnap.borderRadius   = cov.borderRadius;
        ovSnap.boxPadding     = cov.boxPadding;
        ovSnap.minBoxWidth    = cov.minBoxWidth;
        ovSnap.posPreset      = cov.posPreset;
        ovSnap.posX           = cov.posX;
        ovSnap.posY           = cov.posY;
    }
    std::string nextPngPath = lan.hasOverlayImage ? lan.overlayPath : "";

    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        if (m_SnapText != composedText || m_SnapRef != lan.displayRef) {
            ++m_SnapTransTrigger;
        }
        if (m_LanFontCacheName != resolvedFont || m_LanFontCachePath != fontPath) {
            m_LanFontCacheName = resolvedFont;
            m_LanFontCachePath = fontPath;
            ++m_LanFontVersion;
        }
        if (m_SnapOverlayPngPath != nextPngPath) {
            m_SnapOverlayPngPath = nextPngPath;
            ++m_LanOverlayVersion;
        }
        m_SnapOverlay       = std::move(ovSnap);
        m_SnapText          = composedText;
        m_SnapRef           = lan.displayRef;
        m_SnapTextSize      = st.textSize;
        m_SnapTextAlign     = st.textAlign;
        m_SnapTextValign    = st.textValign;
        m_SnapTextColor[0]  = st.colorR / 255.0f;
        m_SnapTextColor[1]  = st.colorG / 255.0f;
        m_SnapTextColor[2]  = st.colorB / 255.0f;
        m_SnapTextColor[3]  = 1.0f;
        m_SnapMargins[0]    = std::max(0.0f, (posX - w * 0.5f) * 1920.0f);
        m_SnapMargins[1]    = std::max(0.0f, (posY - h * 0.5f) * 1080.0f);
        m_SnapMargins[2]    = std::max(0.0f, (1.0f - (posX + w * 0.5f)) * 1920.0f);
        m_SnapMargins[3]    = std::max(0.0f, (1.0f - (posY + h * 0.5f)) * 1080.0f);
        m_SnapFontFamily    = resolvedFont;
        m_SnapFontWeight    = wt;
        m_SnapTextShadow    = st.shadow;
        m_SnapTextOutline   = st.outline;
        m_SnapTextGlow      = st.glow;
        m_SnapTextNeon      = st.neon;
        m_SnapText3d        = st.text3d;
        m_SnapTextUnderline = st.underline;
        m_SnapTextBoxBg     = st.boxBg;
        m_SnapTransType     = m_Main->get_transition_type();
        m_SnapTransDur      = m_Main->get_transition_duration_ms() / 1000.0f;
        m_SnapBgColor[0]    = lan.bgSolidRgb[0];
        m_SnapBgColor[1]    = lan.bgSolidRgb[1];
        m_SnapBgColor[2]    = lan.bgSolidRgb[2];
        m_SnapProjecting    = !composedText.empty() || lan.hasBgImage || lan.bgIsVideo || lan.hasCustomOverlay || lan.hasOverlayImage;
        ++m_StreamVersion;
    }
}

void SlintApp::SyncLanStyleProperties() {
    if (m_Screens.size() <= 1) return;
    const auto& lan = m_Screens[1];
    std::string stName = lan.textStyle.name.empty() ? "Predeterminado" : lan.textStyle.name;
    std::string summary = stName + " · " + std::to_string(static_cast<int>(lan.textStyle.textSize)) + "px";
    m_Main->set_lan_own_style(lan.useOwnStyle);
    m_Main->set_lan_assigned_style_idx(lan.assignedStyleIdx);
    m_Main->set_lan_style_summary(slint::SharedString(summary));
}

void SlintApp::SyncInspectedScreenToPreview() {
    if (m_Screens.empty()) return;
    int idx = std::clamp(m_InspectedScreenIdx, 0, static_cast<int>(m_Screens.size()) - 1);
    const auto& sc = m_Screens[idx];
    const auto& st = sc.textStyle;
    static const int kFontWeights[] = { 400, 700, 900 };

    std::string shortName = (idx == 0) ? "Público"
                          : (idx == 1) ? "LAN (8080)"
                          : (idx == 2) ? "Stage"
                          : sc.name;
    m_Main->set_inspected_screen_name(slint::SharedString(shortName));
    m_Main->set_live_is_bible(sc.isBible);
    m_Main->set_live_text(slint::SharedString(sc.displayText));
    m_Main->set_live_ref(slint::SharedString(sc.displayRef));

    std::string styleLabel = st.name.empty() ? "Predeterminado" : st.name;
    m_Main->set_preview_style_name(slint::SharedString(styleLabel));
    m_Main->set_preview_own_style(sc.id > 0 && sc.useOwnStyle);
    m_Main->set_preview_text_size(st.textSize);
    m_Main->set_preview_text_align(st.textAlign);
    m_Main->set_preview_text_valign(st.textValign);
    m_Main->set_preview_text_color(slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB));
    m_Main->set_preview_font_family_name(slint::SharedString(ResolveStyleFontName(st, false)));
    m_Main->set_preview_font_weight_val(kFontWeights[std::clamp(st.fontWtIdx, 0, 2)]);
    m_Main->set_preview_text_shadow(st.shadow);
    m_Main->set_preview_text_outline(st.outline);
    m_Main->set_preview_text_glow(st.glow);
    m_Main->set_preview_text_neon(st.neon);
    m_Main->set_preview_text_3d(st.text3d);
    m_Main->set_preview_text_underline(st.underline);
    m_Main->set_preview_text_box_bg(st.boxBg);
    m_Main->set_preview_text_fit_mode(st.textFitMode);
    m_Main->set_preview_lyrics_pos_x(st.lyricsPosX);
    m_Main->set_preview_lyrics_pos_y(st.lyricsPosY);
    m_Main->set_preview_lyrics_width(st.lyricsWidth);
    m_Main->set_preview_lyrics_height(st.lyricsHeight);
    m_Main->set_preview_index_box_enabled(st.indexEnabled);
    m_Main->set_preview_index_pos_x(st.indexPosX);
    m_Main->set_preview_index_pos_y(st.indexPosY);
    m_Main->set_preview_index_width(st.indexWidth);
    m_Main->set_preview_index_height(st.indexHeight);
    m_Main->set_preview_index_size(st.indexSize);
    m_Main->set_preview_index_align(st.indexAlign);
    m_Main->set_preview_index_color(slint::Color::from_rgb_uint8(st.indexColorR, st.indexColorG, st.indexColorB));
    m_Main->set_preview_index_font_family_name(slint::SharedString(ResolveStyleFontName(st, true)));
    m_Main->set_preview_index_font_weight_val(kFontWeights[std::clamp(st.indexFontWtIdx, 0, 2)]);
    m_Main->set_preview_index_shadow(st.indexShadow);
    m_Main->set_preview_index_outline(st.indexOutline);
    m_Main->set_preview_index_glow(st.indexGlow);
    m_Main->set_preview_index_box_bg(st.indexBoxBg);

    m_Main->set_live_bg_name(slint::SharedString(sc.bgName));
    m_Main->set_live_bg_solid_color(sc.bgSolidColor);
    m_Main->set_bg_is_static_image(sc.bgIsStatic);
    m_Main->set_preview_has_video_bg(sc.bgIsVideo);
    m_Main->set_has_live_preview_image(sc.hasBgImage);
    if (sc.hasBgImage) {
        m_Main->set_live_preview_image(sc.bgImage);
    }

    m_Main->set_live_overlay_name(slint::SharedString(sc.overlayName));
    m_Main->set_has_live_overlay_image(sc.hasOverlayImage);
    if (sc.hasOverlayImage) {
        m_Main->set_live_overlay_image(sc.overlayImage);
    }

    const auto& cov = sc.customOverlay;
    std::string clk = FormatOverlayClockText(cov);
    m_Main->set_preview_show_clock(sc.hasCustomOverlay && cov.showClock);
    m_Main->set_preview_clock_text(slint::SharedString(clk));

    m_Main->set_preview_ov_active(sc.hasCustomOverlay);
    m_Main->set_preview_ov_show_clock(cov.showClock);
    m_Main->set_preview_ov_clock_text(slint::SharedString(clk));
    m_Main->set_preview_ov_title(slint::SharedString(cov.titleText));
    m_Main->set_preview_ov_sub(slint::SharedString(cov.subText));
    m_Main->set_preview_ov_text_color(slint::Color::from_rgb_uint8(cov.textR, cov.textG, cov.textB));
    m_Main->set_preview_ov_bg_color(slint::Color::from_rgb_uint8(cov.bgR, cov.bgG, cov.bgB));
    m_Main->set_preview_ov_bg_opacity(cov.bgOpacity);
    m_Main->set_preview_ov_show_border(cov.showBorder);
    m_Main->set_preview_ov_border_color(slint::Color::from_rgb_uint8(cov.borderR, cov.borderG, cov.borderB));
    m_Main->set_preview_ov_font_size(cov.fontSize);
    m_Main->set_preview_ov_radius(cov.borderRadius);
    m_Main->set_preview_ov_box_padding(cov.boxPadding);
    m_Main->set_preview_ov_min_width(cov.minBoxWidth);
    m_Main->set_preview_ov_pos_preset(cov.posPreset);
    m_Main->set_preview_ov_pos_x(cov.posX);
    m_Main->set_preview_ov_pos_y(cov.posY);

    m_Main->set_active_public_alert(slint::SharedString(sc.alertText));
}

void SlintApp::SyncScreenOutputs() {
    if (m_Screens.empty()) return;

    // 1. Screen 0 -> ProjectorWindow (Público Principal)
    if (!m_AudienceFrozen) {
        const auto& pub = m_Screens[0];
        ApplyStyleToProjector(pub.textStyle);
        m_Projector->set_live_is_bible(pub.isBible);
        m_Projector->set_live_text(slint::SharedString(pub.displayText));
        m_Projector->set_live_ref(slint::SharedString(pub.displayRef));
        m_Projector->set_bg_solid_color(pub.bgSolidColor);
        m_Projector->set_bg_is_static_image(pub.bgIsStatic);
        m_Projector->set_has_bg_image(pub.hasBgImage);
        if (pub.hasBgImage) {
            m_Projector->set_bg_image(pub.bgImage);
        }
        m_Projector->set_has_overlay_image(pub.hasOverlayImage);
        if (pub.hasOverlayImage) {
            m_Projector->set_overlay_image(pub.overlayImage);
        }
        const auto& cov = pub.customOverlay;
        m_Projector->set_show_custom_overlay(pub.hasCustomOverlay);
        m_Projector->set_ov_show_clock(cov.showClock);
        m_Projector->set_ov_clock_text(slint::SharedString(FormatOverlayClockText(cov)));
        m_Projector->set_ov_title(slint::SharedString(cov.titleText));
        m_Projector->set_ov_sub(slint::SharedString(cov.subText));
        m_Projector->set_ov_text_color(slint::Color::from_rgb_uint8(cov.textR, cov.textG, cov.textB));
        m_Projector->set_ov_bg_color(slint::Color::from_rgb_uint8(cov.bgR, cov.bgG, cov.bgB));
        m_Projector->set_ov_bg_opacity(cov.bgOpacity);
        m_Projector->set_ov_show_border(cov.showBorder);
        m_Projector->set_ov_border_color(slint::Color::from_rgb_uint8(cov.borderR, cov.borderG, cov.borderB));
        m_Projector->set_ov_font_size(cov.fontSize);
        m_Projector->set_ov_radius(cov.borderRadius);
        m_Projector->set_ov_box_padding(cov.boxPadding);
        m_Projector->set_ov_min_width(cov.minBoxWidth);
        m_Projector->set_ov_pos_preset(cov.posPreset);
        m_Projector->set_ov_pos_x(cov.posX);
        m_Projector->set_ov_pos_y(cov.posY);
        m_Projector->set_alert_text(slint::SharedString(pub.alertText));
    }

    // 2. Screen 1 -> NetworkStreamServer (LAN / Smart TV 8080)
    SyncLanFromScreenState();

    // 3. Screen 2 -> StageWindow (Escenario)
    if (m_Screens.size() > 2) {
        static const int kFontWeights[] = { 400, 700, 900 };
        const auto& stg = m_Screens[2];
        const auto& st  = stg.textStyle;
        m_Stage->set_live_text(slint::SharedString(stg.displayText));
        m_Stage->set_live_ref(slint::SharedString(stg.displayRef));
        m_Stage->set_stage_alert(slint::SharedString(stg.alertText));
        m_Stage->set_text_size(st.textSize);
        m_Stage->set_text_align(st.textAlign);
        m_Stage->set_text_color(slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB));
        m_Stage->set_font_family_name(slint::SharedString(ResolveStyleFontName(st, false)));
        m_Stage->set_font_weight_val(kFontWeights[std::clamp(st.fontWtIdx, 0, 2)]);
        bool showStgTimer = stg.hasCustomOverlay && stg.customOverlay.showClock;
        m_Stage->set_timer_text(showStgTimer ? slint::SharedString(FormatOverlayClockText(stg.customOverlay)) : "");
    }

    // 4. Inspected Screen -> Vista en Vivo + LAN Style Status + Capas Activas + Matriz Multi-Pantalla
    SyncLanStyleProperties();
    SyncInspectedScreenToPreview();
    RefreshActiveLayersModel();
    RefreshScreenMatrixModel();
}

void SlintApp::RefreshActiveLayersModel() {
    m_ActiveLayersModel->clear();
    if (m_Screens.empty()) return;
    int idx = std::clamp(m_InspectedScreenIdx, 0, static_cast<int>(m_Screens.size()) - 1);
    const auto& sc = m_Screens[idx];

    auto singleLine = [](const std::string& s) {
        std::string out;
        out.reserve(std::min<size_t>(s.size(), 64));
        for (char c : s) {
            if (c == '\n' || c == '\r') {
                if (!out.empty() && out.back() != ' ') out += " / ";
            } else {
                out += c;
            }
            if (out.size() >= 64) break;
        }
        return out;
    };

    // Layer 1 (Top): Custom Overlay (Clock / Countdown / Count-up / Lower-Third)
    if (sc.hasCustomOverlay) {
        ActiveLayerItem item;
        item.id       = "clock";
        item.kind     = sc.customOverlay.showClock ? "TIEMPO" : "PLECA";
        std::string clk = FormatOverlayClockText(sc.customOverlay);
        std::string lbl = sc.customOverlay.name.empty() ? "Overlay" : sc.customOverlay.name;
        if (!clk.empty()) lbl += ": " + clk;
        else if (!sc.customOverlay.titleText.empty()) lbl += " — " + sc.customOverlay.titleText;
        item.title    = slint::SharedString(lbl);
        item.subtitle = slint::SharedString("Overlay Personalizado • " + sc.name);
        item.accent   = slint::Color::from_rgb_uint8(56, 189, 248);
        item.has_thumbnail = false;
        m_ActiveLayersModel->push_back(item);
    }

    // Layer 2: Alert / Letrero Animado
    if (!sc.alertText.empty()) {
        ActiveLayerItem item;
        item.id       = "alert";
        item.kind     = "AVISO";
        item.title    = slint::SharedString(singleLine(sc.alertText));
        item.subtitle = slint::SharedString("Letrero en Pantalla • " + sc.name);
        item.accent   = slint::Color::from_rgb_uint8(244, 63, 94);
        item.has_thumbnail = false;
        m_ActiveLayersModel->push_back(item);
    }

    // Layer 4: Overlay PNG Transparente
    if (sc.hasOverlayImage) {
        ActiveLayerItem item;
        item.id       = "overlay";
        item.kind     = "OVERLAY";
        item.title    = slint::SharedString(sc.overlayName);
        item.subtitle = slint::SharedString("Gráfico PNG Transparente • " + sc.name);
        item.accent   = slint::Color::from_rgb_uint8(45, 212, 191);
        item.has_thumbnail = true;
        item.thumbnail     = sc.hasOverlayThumb ? sc.overlayThumb : sc.overlayImage;
        m_ActiveLayersModel->push_back(item);
    }

    // Layer 5: Lyrics / Bible Verse
    if (!sc.displayText.empty()) {
        ActiveLayerItem item;
        item.id       = "text";
        item.kind     = slint::SharedString(sc.isBible ? "BIBLIA" : "LETRAS");
        item.title    = slint::SharedString(singleLine(sc.displayText));
        item.subtitle = slint::SharedString(sc.displayRef.empty() ? ("Texto proyectado • " + sc.name) : (sc.displayRef + " • " + sc.name));
        item.accent   = slint::Color::from_rgb_uint8(34, 197, 94);
        item.has_thumbnail = false;
        m_ActiveLayersModel->push_back(item);
    }

    // Layer 6: Background (Video / Static Image / Custom Solid Color)
    if (sc.bgIsVideo || sc.bgIsStatic || sc.bgName != "Sin fondo") {
        ActiveLayerItem item;
        item.id       = "bg";
        item.kind     = slint::SharedString(sc.bgIsVideo ? "VIDEO" : (sc.bgIsStatic ? "IMAGEN" : "COLOR"));
        item.title    = slint::SharedString(sc.bgName);
        item.subtitle = slint::SharedString((sc.bgIsVideo ? "Fondo Video en Vivo • " : "Fondo Activo • ") + sc.name);
        item.accent   = slint::Color::from_rgb_uint8(245, 158, 11);
        item.has_thumbnail = sc.hasBgThumb || sc.hasBgImage;
        if (sc.hasBgThumb)      item.thumbnail = sc.bgThumb;
        else if (sc.hasBgImage) item.thumbnail = sc.bgImage;
        m_ActiveLayersModel->push_back(item);
    }

    // Layer 7: Independent Music Player (Global Audio Layer)
    if (m_Main->get_music_playing()) {
        ActiveLayerItem item;
        item.id       = "music";
        item.kind     = "MÚSICA";
        item.title    = m_Main->get_music_now_playing();
        item.subtitle = "Reproductor de Audio Independiente";
        item.accent   = slint::Color::from_rgb_uint8(236, 72, 153);
        item.has_thumbnail = m_Main->get_has_music_now_cover();
        if (item.has_thumbnail) item.thumbnail = m_Main->get_music_now_cover();
        m_ActiveLayersModel->push_back(item);
    }
}

void SlintApp::RefreshScreenMatrixModel() {
    m_ScreenMatrixModel->clear();
    static const int kFontWeights[] = { 400, 700, 900 };

    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        const auto& sc = m_Screens[i];
        const auto& st = sc.textStyle;
        const auto& cov = sc.customOverlay;
        std::string clk = FormatOverlayClockText(cov);
        ScreenMatrixItem item;
        item.id             = i;
        item.name           = slint::SharedString(sc.name);
        item.role_badge     = slint::SharedString(sc.roleBadge);
        item.is_live        = (i == 0) ? m_ProjectorVisible
                            : (i == 1) ? IsLanRunning()
                            : (i == 2) ? m_StageVisible
                            : (!sc.displayText.empty() || sc.hasBgImage || sc.hasCustomOverlay || sc.hasOverlayImage || !sc.alertText.empty());
        item.is_targeted    = IsScreenTargeted(i);
        item.is_inspected   = (i == m_InspectedScreenIdx);
        item.is_custom_room = sc.isCustomRoom;
        item.bg_color       = sc.bgSolidColor;
        item.has_bg_image   = sc.hasBgImage;
        if (sc.hasBgImage) item.bg_image = sc.bgImage;
        item.bg_name        = slint::SharedString(sc.bgName);
        item.has_overlay_image = sc.hasOverlayImage;
        if (sc.hasOverlayImage) item.overlay_image = sc.overlayImage;
        item.overlay_name   = slint::SharedString(sc.overlayName);
        item.live_text      = slint::SharedString(sc.displayText);
        item.live_ref       = slint::SharedString(sc.displayRef);
        item.live_is_bible  = sc.isBible;
        item.show_clock     = sc.hasCustomOverlay;
        item.clock_text     = slint::SharedString(!clk.empty() ? clk : cov.name);
        item.alert_text     = slint::SharedString(sc.alertText);
        item.ov_show_clock  = cov.showClock;
        item.ov_title       = slint::SharedString(cov.titleText);
        item.ov_sub         = slint::SharedString(cov.subText);
        item.ov_text_color  = slint::Color::from_rgb_uint8(cov.textR, cov.textG, cov.textB);
        item.ov_bg_color    = slint::Color::from_rgb_uint8(cov.bgR, cov.bgG, cov.bgB);
        item.ov_bg_opacity  = cov.bgOpacity;
        item.ov_show_border = cov.showBorder;
        item.ov_border_color = slint::Color::from_rgb_uint8(cov.borderR, cov.borderG, cov.borderB);
        item.ov_font_size   = cov.fontSize;
        item.ov_radius      = cov.borderRadius;
        item.ov_box_padding = cov.boxPadding;
        item.ov_min_width   = cov.minBoxWidth;
        item.ov_pos_preset  = cov.posPreset;
        item.ov_pos_x       = cov.posX;
        item.ov_pos_y       = cov.posY;

        std::string stName = st.name.empty() ? "Predeterminado" : st.name;
        item.style_name         = slint::SharedString(stName);
        item.use_own_style      = sc.useOwnStyle;
        item.assigned_style_idx = sc.assignedStyleIdx;
        item.text_size        = st.textSize;
        item.text_align       = st.textAlign;
        item.text_valign      = st.textValign;
        item.text_color       = slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB);
        item.font_family_name = slint::SharedString(ResolveStyleFontName(st, false));
        item.font_weight_val  = kFontWeights[std::clamp(st.fontWtIdx, 0, 2)];
        item.text_shadow      = st.shadow;
        item.text_outline     = st.outline;
        item.text_glow        = st.glow;
        item.text_neon        = st.neon;
        item.text_3d          = st.text3d;
        item.text_underline   = st.underline;
        item.text_box_bg      = st.boxBg;
        item.lyrics_pos_x     = st.lyricsPosX;
        item.lyrics_pos_y     = st.lyricsPosY;
        item.lyrics_width     = st.lyricsWidth;
        item.lyrics_height    = st.lyricsHeight;

        int activeCount = 0;
        if (sc.hasCustomOverlay)   ++activeCount;
        if (!sc.displayText.empty()) ++activeCount;
        if (sc.bgName != "Sin fondo") ++activeCount;
        if (sc.hasOverlayImage)    ++activeCount;
        if (!sc.alertText.empty()) ++activeCount;
        item.queue_summary = slint::SharedString(
            std::to_string(activeCount) + " capa(s) • Estilo: " + stName + " (" +
            std::to_string(static_cast<int>(st.textSize)) + "px)");
        m_ScreenMatrixModel->push_back(item);
    }
}

void SlintApp::RemoveLayerOnScreen(int screenIdx, const std::string& layerId) {
    if (layerId == "music") {
        m_MusicPlayer.Stop();
        m_CurrentMusicIdx = -1;
        m_Main->set_music_playing(false);
        m_Main->set_music_paused(false);
        m_Main->set_music_position(0.0f);
        m_Main->set_music_time_label("00:00 / 00:00");
        m_Main->set_music_now_playing("Sin pista en reproducción");
        m_Main->set_has_music_now_cover(false);
        RefreshMusicModel();
        RefreshActiveLayersModel();
        return;
    }
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    auto& sc = m_Screens[screenIdx];

    if (layerId == "clock" || layerId == "pleca") {
        sc.hasCustomOverlay = false;
        RefreshOverlayLayers();
        RefreshOverlayQueueModel();
    } else if (layerId == "alert") {
        sc.alertText.clear();
        if (screenIdx == 2) {
            m_Main->set_active_stage_alert("");
            m_Stage->set_stage_alert("");
        }
    } else if (layerId == "overlay") {
        sc.overlayName = "Sin overlay";
        sc.overlayPath.clear();
        sc.hasOverlayImage = false;
        sc.hasOverlayThumb = false;
        if (screenIdx == 0) m_ActiveOverlayPath.clear();
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
    } else if (layerId == "text") {
        sc.rawText.clear();
        sc.rawRef.clear();
        sc.displayText.clear();
        sc.displayRef.clear();
        sc.isBible = false;
        if (screenIdx == 0) {
            m_RawLiveText.clear();
            m_RawLiveRef.clear();
            m_LiveIsBible = false;
            m_Main->set_active_slide_index(-1);
        }
    } else if (layerId == "bg") {
        sc.bgName       = "Sin fondo";
        sc.bgPath.clear();
        sc.hasBgImage   = false;
        sc.bgIsStatic   = false;
        sc.bgIsVideo    = false;
        sc.hasBgThumb   = false;
        sc.bgSolidColor = slint::Color::from_rgb_uint8(3, 3, 4);
        sc.bgSolidRgb[0] = 0.0f;
        sc.bgSolidRgb[1] = 0.0f;
        sc.bgSolidRgb[2] = 0.0f;
        if (screenIdx == 1) ClearLanFrame();
    }

    bool anyVideoStillUsed = false;
    for (const auto& s : m_Screens) {
        if (s.bgIsVideo) { anyVideoStillUsed = true; break; }
    }
    if (!anyVideoStillUsed && m_HasVideoBg) {
        m_Player.Stop();
        m_CurrentBgPath.clear();
        m_HasStaticImage = false;
        m_HasVideoBg = false;
        m_Main->set_media_position(0.0f);
        m_Main->set_media_time_label("00:00 / 00:00");
    }
}

void SlintApp::RemoveActiveLayer(const std::string& layerId) {
    if (layerId == "music") {
        RemoveLayerOnScreen(0, "music");
        return;
    }
    // If inspecting a specific screen (or targeting a specific screen), remove from that screen;
    // if in "Todas" mode and inspecting Público (0), remove from all screens.
    if (m_OutputTargetMode == 0 && m_InspectedScreenIdx == 0) {
        for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
            RemoveLayerOnScreen(i, layerId);
        }
    } else {
        RemoveLayerOnScreen(m_InspectedScreenIdx, layerId);
    }
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::ToggleLayerOnScreen(int screenIdx, const std::string& layerId) {
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    auto& sc = m_Screens[screenIdx];

    if (layerId == "clock") {
        if (sc.hasCustomOverlay) {
            sc.hasCustomOverlay = false;
        } else {
            sc.customOverlay = !m_CustomOverlays.empty() ? m_CustomOverlays.front() : CustomOverlayConfig{};
            sc.hasCustomOverlay = true;
        }
        RefreshOverlayLayers();
        RefreshOverlayQueueModel();
    } else if (layerId == "text") {
        if (!sc.displayText.empty()) {
            RemoveLayerOnScreen(screenIdx, "text");
        } else {
            int curSlide = m_Main->get_active_slide_index();
            if (curSlide >= 0 && curSlide < static_cast<int>(m_SlidesModel->row_count())) {
                if (auto s = m_SlidesModel->row_data(curSlide)) {
                    SetScreenLiveText(sc, std::string(s->body), std::string(s->ref_text), (m_ActiveTab == 1));
                }
            } else if (!m_Screens[0].displayText.empty()) {
                SetScreenLiveText(sc, m_Screens[0].rawText, m_Screens[0].rawRef, m_Screens[0].isBible);
            }
        }
    } else if (layerId == "bg") {
        if (sc.bgName != "Sin fondo") {
            RemoveLayerOnScreen(screenIdx, "bg");
        } else if (m_Screens[0].bgName != "Sin fondo") {
            sc.bgName     = m_Screens[0].bgName;
            sc.bgPath     = m_Screens[0].bgPath;
            sc.hasBgImage = m_Screens[0].hasBgImage;
            sc.bgIsStatic = m_Screens[0].bgIsStatic;
            sc.bgIsVideo  = m_Screens[0].bgIsVideo;
            sc.bgImage    = m_Screens[0].bgImage;
            sc.hasBgThumb = m_Screens[0].hasBgThumb;
            sc.bgThumb    = m_Screens[0].bgThumb;
            sc.bgSolidColor = m_Screens[0].bgSolidColor;
            for (int c = 0; c < 3; ++c) sc.bgSolidRgb[c] = m_Screens[0].bgSolidRgb[c];
            if (screenIdx == 1 && sc.bgIsStatic && sc.hasBgImage && IsLanRunning()) {
                UpdateLanStaticFrame(sc.bgImage);
            }
        }
    } else if (layerId == "alert") {
        if (!sc.alertText.empty()) {
            RemoveLayerOnScreen(screenIdx, "alert");
        } else {
            std::string inp = std::string(m_Main->get_alert_input());
            if (inp.empty() && !m_Screens[0].alertText.empty()) inp = m_Screens[0].alertText;
            if (inp.empty()) inp = "Aviso en " + sc.name;
            sc.alertText = inp;
            m_HasActivePublicAlert = true;
        }
    }
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::SendCurrentSelectionToScreen(int screenIdx) {
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    int prevTarget = m_OutputTargetMode;
    m_OutputTargetMode = screenIdx + 1; // Route exclusively to screenIdx

    int curSlide = m_Main->get_active_slide_index();
    if ((m_ActiveTab == 0 || m_ActiveTab == 1) && curSlide >= 0 && curSlide < static_cast<int>(m_SlidesModel->row_count())) {
        ProjectSlide(curSlide);
    } else if ((m_ActiveTab == 2 || m_ActiveTab == 3) && m_HasSelectedPreviewEntry) {
        ProjectSelectedMedia();
    } else if (m_ActiveTab == 10 && m_ActiveDocPageIdx >= 0) {
        ProjectDocPage(m_ActiveDocPageIdx);
    } else if (!m_Screens[0].displayText.empty() || m_Screens[0].bgName != "Sin fondo") {
        ClonePublicToScreen(screenIdx);
    }

    m_OutputTargetMode = prevTarget;
    m_InspectedScreenIdx = screenIdx;
    m_Main->set_inspected_screen_idx(m_InspectedScreenIdx);
    SyncScreenOutputs();
}

void SlintApp::ClonePublicToScreen(int screenIdx) {
    if (screenIdx <= 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    const auto& pub = m_Screens[0];
    auto& dst       = m_Screens[screenIdx];

    // Preserve dst's own textStyle / assignedStyleIdx and re-apply its uppercase rules
    SetScreenLiveText(dst, pub.rawText, pub.rawRef, pub.isBible);
    dst.bgName          = pub.bgName;
    dst.bgPath          = pub.bgPath;
    dst.hasBgImage      = pub.hasBgImage;
    dst.bgIsStatic      = pub.bgIsStatic;
    dst.bgIsVideo       = pub.bgIsVideo;
    dst.bgImage         = pub.bgImage;
    dst.hasBgThumb      = pub.hasBgThumb;
    dst.bgThumb         = pub.bgThumb;
    dst.bgSolidColor    = pub.bgSolidColor;
    for (int c = 0; c < 3; ++c) dst.bgSolidRgb[c] = pub.bgSolidRgb[c];
    dst.overlayName      = pub.overlayName;
    dst.overlayPath      = pub.overlayPath;
    dst.hasOverlayImage  = pub.hasOverlayImage;
    dst.overlayImage     = pub.overlayImage;
    dst.hasOverlayThumb  = pub.hasOverlayThumb;
    dst.overlayThumb     = pub.overlayThumb;
    dst.hasCustomOverlay = pub.hasCustomOverlay;
    dst.customOverlay    = pub.customOverlay;
    dst.alertText        = pub.alertText;

    if (screenIdx == 1 && dst.bgIsStatic && dst.hasBgImage && IsLanRunning()) {
        UpdateLanStaticFrame(dst.bgImage);
    }
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::ClearAllOnScreen(int screenIdx) {
    if (screenIdx < 0 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    RemoveLayerOnScreen(screenIdx, "clock");
    RemoveLayerOnScreen(screenIdx, "alert");
    RemoveLayerOnScreen(screenIdx, "pleca");
    RemoveLayerOnScreen(screenIdx, "overlay");
    RemoveLayerOnScreen(screenIdx, "text");
    RemoveLayerOnScreen(screenIdx, "bg");
    SyncClockOverlay();
    SyncScreenOutputs();
}

void SlintApp::CreateExtraScreen(const std::string& rawName) {
    std::string clean = rawName;
    while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
    while (!clean.empty() && clean.back() == ' ')  clean.pop_back();
    if (clean.empty()) {
        clean = "Sala " + std::to_string(m_Screens.size());
    }

    ScreenOutputState room;
    room.id           = static_cast<int>(m_Screens.size());
    room.name         = clean;
    room.roleBadge    = "SALA INDEPENDIENTE";
    room.isCustomRoom = true;
    if (!m_Screens.empty()) {
        room.textStyle        = m_Screens[0].textStyle;
        room.assignedStyleIdx = m_Screens[0].assignedStyleIdx;
        room.useOwnStyle      = false;
    }
    m_Screens.push_back(std::move(room));

    m_InspectedScreenIdx = static_cast<int>(m_Screens.size()) - 1;
    m_OutputTargetMode   = m_InspectedScreenIdx + 1;
    m_Main->set_inspected_screen_idx(m_InspectedScreenIdx);
    m_Main->set_output_target_mode(m_OutputTargetMode);
    SyncScreenOutputs();
    RefreshSavedStylesModel();
    SavePersistedState();
    AppendDebugLog("Nueva sala creada: " + clean);
}

void SlintApp::DeleteExtraScreen(int screenIdx) {
    if (screenIdx < 3 || screenIdx >= static_cast<int>(m_Screens.size())) return;
    m_Screens.erase(m_Screens.begin() + screenIdx);
    for (int i = 0; i < static_cast<int>(m_Screens.size()); ++i) {
        m_Screens[i].id = i;
    }
    if (m_InspectedScreenIdx >= static_cast<int>(m_Screens.size())) {
        m_InspectedScreenIdx = 0;
        m_Main->set_inspected_screen_idx(0);
    }
    if (m_OutputTargetMode > static_cast<int>(m_Screens.size())) {
        m_OutputTargetMode = 0;
        m_Main->set_output_target_mode(0);
    }
    if (m_StyleTargetMode > static_cast<int>(m_Screens.size())) {
        SelectStyleTarget(0);
    }
    SyncScreenOutputs();
    RefreshSavedStylesModel();
    SavePersistedState();
}

} // namespace ProyecThor::SlintUI

namespace ProyecThor::UI::BibleBooks {

const char* GetCanonicalBookName(int bookNum) {
    static const char* kBookNames[] = {
        "Génesis","Éxodo","Levítico","Números","Deuteronomio",
        "Josué","Jueces","Rut","1 Samuel","2 Samuel","1 Reyes","2 Reyes",
        "1 Crónicas","2 Crónicas","Esdras","Nehemías","Ester","Job","Salmos",
        "Proverbios","Eclesiastés","Cantares","Isaías","Jeremías","Lamentaciones",
        "Ezequiel","Daniel","Oseas","Joel","Amós","Abdías","Jonás","Miqueas",
        "Nahúm","Habacuc","Sofonías","Hageo","Zacarías","Malaquías",
        "Mateo","Marcos","Lucas","Juan","Hechos","Romanos","1 Corintios",
        "2 Corintios","Gálatas","Efesios","Filipenses","Colosenses","1 Tesalonicenses",
        "2 Tesalonicenses","1 Timoteo","2 Timoteo","Tito","Filemón","Hebreos",
        "Santiago","1 Pedro","2 Pedro","1 Juan","2 Juan","3 Juan","Judas","Apocalipsis"
    };
    if (bookNum < 1 || bookNum > 66) return nullptr;
    return kBookNames[bookNum - 1];
}

} // namespace ProyecThor::UI::BibleBooks
