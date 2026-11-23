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
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace ProyecThor::SlintUI {

namespace {

bool HasExtension(const fs::path& p, const std::vector<std::string>& exts) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

std::string RunCommandCapture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        out += buf;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
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
    , m_MusicPlayer(true)
    , m_HoverPlayer(true)
{
    for (int i = 0; i < 8; ++i) {
        m_Pads[i].name = "Pad " + std::to_string(i + 1);
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
    m_Main->set_hub_recent_songs(m_HubSongsModel);
    m_Main->set_hub_recent_media(m_HubMediaModel);
    m_Main->set_hub_recent_music(m_HubMusicModel);
    m_Main->set_hub_recent_docs(m_HubDocsModel);
    m_Main->set_doc_pages(m_DocPagesModel);
    m_Main->set_queue_items(m_QueueModel);
    m_Main->set_overlay_queue_items(m_OverlayQueueModel);
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

    BindCallbacks();
    ScanSystemFonts();
    LoadPersistedState();
    UpdatePlayerRenderSize();
    m_Player.SetScaleMode(m_BgScaleMode);
    LoadThemeFilesFromDisk();
    LoadSongPlaylists();
    ApplyThemePreset(m_ActiveThemeIdx);
    RefreshPadsModel();
    RefreshSavedStylesModel();
    RefreshMusicScanFoldersModel();
    RefreshBgDock();
    RefreshOverlayLayers();
    LoadChangelogText();
    CheckFirewallStatusAsync();
    SyncProjectorStyle();
    SyncTilesState();
    ApplyShaders();
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

    // 30 FPS tick timer for libmpv video frames, playback progress, clock & countdown
    m_TickTimer = std::make_unique<slint::Timer>(
        std::chrono::milliseconds(33),
        [this]() { OnTick(); }
    );
}

SlintApp::~SlintApp() {
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
            (fs::path(ProyecThor::GetAssetsPath()) / f).string(),
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
        m_ActiveTab = tab;
        m_SearchQuery.clear();
        m_Main->set_search_query("");
        m_Main->set_song_editor_open(false);
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
        } else if (tab == 12) {
            RefreshDebugInfo();
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
    m_Main->on_bible_filter_changed([this](int) { RefreshBibleBooks(); });
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
            std::string disp = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
            std::string dispRef = m_Main->get_index_uppercase() ? ToUpperUtf8(m_RawLiveRef) : m_RawLiveRef;
            m_Main->set_live_is_bible(true);
            m_Projector->set_live_is_bible(true);
            m_Main->set_live_text(slint::SharedString(disp));
            m_Main->set_live_ref(slint::SharedString(dispRef));
            m_Projector->set_live_text(slint::SharedString(disp));
            m_Projector->set_live_ref(slint::SharedString(dispRef));
            m_Stage->set_live_text(slint::SharedString(disp));
            m_Stage->set_live_ref(slint::SharedString(dispRef));
            TriggerSlideTransition();
        }
    });
    m_Main->on_bible_favorite_clicked([this](int idx) {
        if (auto item = m_BibleFavoritesModel->row_data(idx)) {
            m_RawLiveText = std::string(item->body);
            m_RawLiveRef  = std::string(item->ref_text);
            m_LiveIsBible = true;
            std::string disp = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
            std::string dispRef = m_Main->get_index_uppercase() ? ToUpperUtf8(m_RawLiveRef) : m_RawLiveRef;
            m_Main->set_live_is_bible(true);
            m_Projector->set_live_is_bible(true);
            m_Main->set_live_text(slint::SharedString(disp));
            m_Main->set_live_ref(slint::SharedString(dispRef));
            m_Projector->set_live_text(slint::SharedString(disp));
            m_Projector->set_live_ref(slint::SharedString(dispRef));
            m_Stage->set_live_text(slint::SharedString(disp));
            m_Stage->set_live_ref(slint::SharedString(dispRef));
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
        m_Main->set_has_music_now_cover(false);
        RefreshMusicModel();
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
        m_HasActivePublicAlert = !std::string(msg).empty();
        m_Main->set_active_public_alert(msg);
        m_AlertTickerPos  = 0.0f;
        m_AlertPulsePhase = 0.0f;
        if (!m_AudienceFrozen) {
            m_Projector->set_alert_text(msg);
            SyncAlertConfig();
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
        m_Stage->set_stage_alert(msg);
    });
    m_Main->on_alert_config_changed([this]() { SyncAlertConfig(); });
    m_Main->on_clear_alerts([this]() {
        m_HasActivePublicAlert = false;
        m_Main->set_active_public_alert("");
        m_Main->set_active_stage_alert("");
        if (!m_AudienceFrozen) m_Projector->set_alert_text("");
        m_Stage->set_stage_alert("");
    });

    // Clock & Timer
    m_Main->on_timer_set_minutes([this](int mins) {
        m_TimerInitialSec   = std::max(1, mins) * 60;
        m_TimerRemainingSec = m_TimerInitialSec;
        m_Main->set_timer_display_text(slint::SharedString(FormatCountdownSec(m_TimerRemainingSec)));
    });
    m_Main->on_timer_toggle([this]() {
        m_TimerRunning = !m_TimerRunning;
        m_Main->set_timer_running(m_TimerRunning);
    });
    m_Main->on_timer_reset([this]() {
        m_TimerRunning = false;
        m_TimerRemainingSec = m_TimerInitialSec;
        m_Main->set_timer_running(false);
        m_Main->set_timer_display_text(slint::SharedString(FormatCountdownSec(m_TimerRemainingSec)));
    });
    m_Main->on_clock_overlay_toggled([this](bool show) {
        m_Projector->set_show_clock_overlay(show);
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
    m_Main->on_custom_overlay_toggled([this](slint::SharedString t, slint::SharedString s, bool show) {
        m_Projector->set_live_overlay_title(t);
        m_Projector->set_live_overlay_sub(s);
        m_Projector->set_show_custom_overlay(show);
    });
    m_Main->on_open_legacy_imgui([this]() { LaunchLegacyImGui(); });
    m_Main->on_fsr_changed([this](bool) {
        ApplyShaders();
        SavePersistedState();
    });
    m_Main->on_performance_mode_changed([this](bool) {
        UpdatePlayerRenderSize();
        SavePersistedState();
    });
    m_Main->on_fullscreen_changed([this](bool fs) {
        if (m_ProjectorVisible) {
            m_Projector->window().set_fullscreen(fs);
        }
        SavePersistedState();
    });
    m_Main->on_check_github_updates([this]() { CheckGitHubUpdatesAsync(); });
}

void SlintApp::LoadTabData(int tab) {
    m_AllEntries.clear();
    m_SlidesModel->clear();
    m_SlideRowsModel->clear();
    m_Main->set_selected_library_index(-1);
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
        if (!needle.empty()) {
            if (ToLowerUtf8(e.title).find(needle) == std::string::npos &&
                ToLowerUtf8(e.subtitle).find(needle) == std::string::npos) {
                continue;
            }
        }
        m_FilteredIndices.push_back(i);
        LibraryItem item;
        item.title    = slint::SharedString(e.title);
        item.subtitle = slint::SharedString(e.subtitle);
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

    if (m_ActiveTab == 0) {
        m_Main->set_song_editor_open(false);
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_song_author(slint::SharedString(entry.author));
        auto stanzas = ParseSongStanzas(entry.path);
        m_SlidesModel->clear();
        m_Main->set_active_slide_index(-1);
        for (size_t i = 0; i < stanzas.size(); ++i) {
            SlideItem s;
            s.badge    = slint::SharedString("Estrofa " + std::to_string(i + 1));
            s.body     = slint::SharedString(stanzas[i]);
            s.ref_text = slint::SharedString(entry.title);
            m_SlidesModel->push_back(s);
        }
        RebuildSlideGridRows();
    } else if (m_ActiveTab == 10) {
        OpenDocumentAsync(entry.path);
    } else if (m_ActiveTab == 2) {
        // Multimedia: Load into Preview ONLY (does NOT interrupt Live output until user clicks "Enviar a Pantalla" or "+ Cola Videos")
        m_SelectedPreviewEntry    = entry;
        m_HasSelectedPreviewEntry = true;
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_media_info(slint::SharedString(entry.subtitle + " • " + fs::path(entry.path).filename().string()));

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
        m_Main->set_selected_media_info(slint::SharedString(entry.subtitle + " • " + fs::path(entry.path).filename().string()));

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
    m_RawLiveRef  = m_LiveIsBible ? std::string(slide->ref_text) : "";

    std::string displayStr = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
    std::string displayRef = (m_LiveIsBible && m_Main->get_index_uppercase()) ? ToUpperUtf8(m_RawLiveRef) : m_RawLiveRef;
    slint::SharedString dispText(displayStr);
    slint::SharedString dispRef(displayRef);

    m_Main->set_live_is_bible(m_LiveIsBible);
    m_Main->set_live_text(dispText);
    m_Main->set_live_ref(dispRef);
    if (!m_AudienceFrozen) {
        m_Projector->set_live_is_bible(m_LiveIsBible);
        m_Projector->set_live_text(dispText);
        m_Projector->set_live_ref(dispRef);
    }
    m_Stage->set_live_text(dispText);
    m_Stage->set_live_ref(m_LiveIsBible ? dispRef : slide->ref_text);
    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapText = displayStr;
        m_SnapRef  = displayRef;
        ++m_SnapTransTrigger;
        ++m_StreamVersion;
    }
    if (m_ActiveTab == 1) {
        PushBibleHistory(std::string(slide->badge), std::string(slide->body), std::string(slide->ref_text));
    }
    SyncStageNextSlide();
    SyncTilesState();
    TriggerSlideTransition();
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
    m_RawLiveText.clear();
    m_RawLiveRef.clear();
    m_LiveIsBible = false;
    m_Main->set_live_is_bible(false);
    m_Main->set_live_text("");
    m_Main->set_live_ref("");
    m_Main->set_active_slide_index(-1);
    if (!m_AudienceFrozen) {
        m_Projector->set_live_is_bible(false);
        m_Projector->set_live_text("");
        m_Projector->set_live_ref("");
    }
    m_Stage->set_live_text("");
    m_Stage->set_live_ref("");
    m_Stage->set_next_text("");
    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapText.clear();
        m_SnapRef.clear();
        ++m_StreamVersion;
    }
}

void SlintApp::ClearBackground() {
    m_Player.Stop();
    m_CurrentBgPath.clear();
    m_HasStaticImage = false;
    m_HasVideoBg = false;
    ClearLanFrame();
    m_Main->set_bg_is_static_image(false);
    m_Main->set_has_live_preview_image(false);
    m_Main->set_live_bg_name("Sin fondo");
    if (!m_AudienceFrozen) {
        m_Projector->set_bg_is_static_image(false);
        m_Projector->set_has_bg_image(false);
    }
    m_Main->set_media_position(0.0f);
    m_Main->set_media_time_label("00:00 / 00:00");
}

void SlintApp::ClearOverlay() {
    m_ActiveOverlayPath.clear();
    m_Main->set_has_live_overlay_image(false);
    m_Main->set_live_overlay_name("Sin overlay");
    if (!m_AudienceFrozen) {
        m_Projector->set_has_overlay_image(false);
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
}

void SlintApp::ClearAll() {
    ClearText();
    ClearBackground();
    ClearOverlay();
}

void SlintApp::ScanSystemFonts() {
    m_AllSystemFonts.clear();
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

    std::string raw = RunCommandCapture("fc-list : family 2>/dev/null");
    std::istringstream iss(raw);
    std::string line;
    std::vector<std::string> unique;
    while (std::getline(iss, line)) {
        // fc-list can output comma-separated aliases per line; take primary family
        auto comma = line.find(',');
        if (comma != std::string::npos) line = line.substr(0, comma);
        std::string fam = trimLine(line);
        if (fam.empty()) continue;
        std::string low = ToLowerUtf8(fam);
        if (low.find(".pcf") != std::string::npos || low.find("cursor") != std::string::npos ||
            low.find("icomoon") != std::string::npos || low.find("fontawesome") != std::string::npos ||
            low.find("emoji") != std::string::npos || low.find("dingbat") != std::string::npos) {
            continue;
        }
        unique.push_back(std::move(fam));
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

void SlintApp::SyncProjectorStyle() {
    static const int kFontWeights[] = { 400, 700, 900 };
    int wtIdx     = std::clamp(m_Main->get_font_weight_idx(), 0, 2);
    int idxWtIdx  = std::clamp(m_Main->get_index_font_weight_idx(), 0, 2);

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

    if (!m_AudienceFrozen) {
        m_Projector->set_text_size(m_Main->get_text_size());
        m_Projector->set_text_align(m_Main->get_text_align());
        m_Projector->set_text_valign(m_Main->get_text_valign());
        m_Projector->set_text_shadow(m_Main->get_text_shadow());
        m_Projector->set_text_outline(m_Main->get_text_outline());
        m_Projector->set_text_glow(m_Main->get_text_glow());
        m_Projector->set_text_neon(m_Main->get_text_neon());
        m_Projector->set_text_3d(m_Main->get_text_3d());
        m_Projector->set_text_underline(m_Main->get_text_underline());
        m_Projector->set_text_box_bg(m_Main->get_text_box_bg());
        m_Projector->set_text_color(m_Main->get_text_color());
        m_Projector->set_font_family_name(slint::SharedString(mainFont));
        m_Projector->set_font_weight_val(kFontWeights[wtIdx]);
        m_Projector->set_text_margin_h(m_Main->get_text_margin_h());
        m_Projector->set_lyrics_pos_x(m_Main->get_lyrics_pos_x());
        m_Projector->set_lyrics_pos_y(m_Main->get_lyrics_pos_y());
        m_Projector->set_lyrics_width(m_Main->get_lyrics_width());
        m_Projector->set_lyrics_height(m_Main->get_lyrics_height());

        // Independent Bible Index Box style (100% decoupled from Lyrics)
        m_Projector->set_index_box_enabled(m_Main->get_index_box_enabled());
        m_Projector->set_index_pos_x(m_Main->get_index_pos_x());
        m_Projector->set_index_pos_y(m_Main->get_index_pos_y());
        m_Projector->set_index_width(m_Main->get_index_width());
        m_Projector->set_index_height(m_Main->get_index_height());
        m_Projector->set_index_size(m_Main->get_index_size());
        m_Projector->set_index_align(m_Main->get_index_align());
        m_Projector->set_index_color(m_Main->get_index_color());
        m_Projector->set_index_font_family_name(slint::SharedString(idxFont));
        m_Projector->set_index_font_weight_val(kFontWeights[idxWtIdx]);
        m_Projector->set_index_shadow(m_Main->get_index_shadow());
        m_Projector->set_index_outline(m_Main->get_index_outline());
        m_Projector->set_index_glow(m_Main->get_index_glow());
        m_Projector->set_index_box_bg(m_Main->get_index_box_bg());

        m_Projector->set_transition_type(m_Main->get_transition_type());
        m_Projector->set_transition_dur(static_cast<int64_t>(m_Main->get_transition_duration_ms()));
    }

    // Re-apply uppercase toggles independently to current live text & Bible ref
    if (!m_RawLiveText.empty()) {
        std::string disp = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
        slint::SharedString s(disp);
        m_Main->set_live_text(s);
        if (!m_AudienceFrozen) m_Projector->set_live_text(s);
        m_Stage->set_live_text(s);
    }
    if (m_LiveIsBible && !m_RawLiveRef.empty()) {
        std::string dispRef = m_Main->get_index_uppercase() ? ToUpperUtf8(m_RawLiveRef) : m_RawLiveRef;
        slint::SharedString sr(dispRef);
        m_Main->set_live_ref(sr);
        if (!m_AudienceFrozen) m_Projector->set_live_ref(sr);
    }

    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapTextSize   = m_Main->get_text_size();
        m_SnapTextAlign  = m_Main->get_text_align();
        m_SnapTextValign = m_Main->get_text_valign();
        m_SnapTextColor[0] = std::clamp(m_Main->get_text_color_r() / 255.0f, 0.0f, 1.0f);
        m_SnapTextColor[1] = std::clamp(m_Main->get_text_color_g() / 255.0f, 0.0f, 1.0f);
        m_SnapTextColor[2] = std::clamp(m_Main->get_text_color_b() / 255.0f, 0.0f, 1.0f);
        m_SnapTextColor[3] = 1.0f;
        float posX = std::clamp(m_Main->get_lyrics_pos_x(), 0.05f, 0.95f);
        float posY = std::clamp(m_Main->get_lyrics_pos_y(), 0.05f, 0.95f);
        float w    = std::clamp(m_Main->get_lyrics_width(), 0.15f, 1.0f);
        float h    = std::clamp(m_Main->get_lyrics_height(), 0.08f, 1.0f);
        m_SnapMargins[0] = std::max(0.0f, (posX - w * 0.5f) * 1920.0f);
        m_SnapMargins[1] = std::max(0.0f, (posY - h * 0.5f) * 1080.0f);
        m_SnapMargins[2] = std::max(0.0f, (1.0f - (posX + w * 0.5f)) * 1920.0f);
        m_SnapMargins[3] = std::max(0.0f, (1.0f - (posY + h * 0.5f)) * 1080.0f);
        m_SnapTransType  = m_Main->get_transition_type();
        m_SnapTransDur   = m_Main->get_transition_duration_ms() / 1000.0f;
        ++m_StreamVersion;
    }
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
    SetIndexColorRGB(st.indexColorR, st.indexColorG, st.indexColorB);
    SetTextColorRGB(st.colorR, st.colorG, st.colorB);
}

void SlintApp::OpenNewStyleEditor() {
    m_StyleBeforeEdit = CaptureCurrentStyleFromUI("");
    m_EditingStyleIdx = -1;
    m_Main->set_editing_style_name("Mi Estilo " + slint::SharedString(std::to_string(m_SavedStyles.size() + 1)));
    m_Main->set_style_editor_box(0);
    m_Main->set_style_editor_open(true);
}

void SlintApp::OpenEditStyleEditor(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    m_StyleBeforeEdit = CaptureCurrentStyleFromUI("");
    m_EditingStyleIdx = idx;
    ApplyStyleStructToUI(m_SavedStyles[idx]);
    m_Main->set_editing_style_name(slint::SharedString(m_SavedStyles[idx].name));
    m_Main->set_style_editor_box(0);
    m_Main->set_style_editor_open(true);
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
    RefreshSavedStylesModel();
    SyncProjectorStyle();
    SavePersistedState();
}

void SlintApp::CancelStyleEditor() {
    ApplyStyleStructToUI(m_StyleBeforeEdit);
    m_Main->set_style_editor_open(false);
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
    ApplyStyleStructToUI(m_SavedStyles[idx]);
    SavePersistedState();
}

void SlintApp::DeleteCustomStyle(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    std::error_code ec;
    fs::remove(fs::path(ProyecThor::GetAppDataRoot()) / "themes" / (SanitizeFilename(m_SavedStyles[idx].name) + ".theme"), ec);
    m_SavedStyles.erase(m_SavedStyles.begin() + idx);
    if (m_Main->get_active_saved_style_idx() == idx) m_Main->set_active_saved_style_idx(-1);
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::RefreshSavedStylesModel() {
    m_SavedStyleCardsModel->clear();
    static const char* kFamNames[] = { "Default", "Sans", "Serif", "Mono" };
    for (const auto& st : m_SavedStyles) {
        StyleCardItem card;
        card.name       = slint::SharedString(st.name);
        card.text_color = slint::Color::from_rgb_uint8(st.colorR, st.colorG, st.colorB);
        card.box_bg     = st.boxBg;

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
}

void SlintApp::LoadThemeFilesFromDisk() {
    fs::path dir = fs::path(ProyecThor::GetAppDataRoot()) / "themes";
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".theme") continue;
        std::string themeName = entry.path().stem().string();
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
            if (m_Main->get_text_uppercase()) body = ToUpperUtf8(body);
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
        case 1: SetTextColorRGB(254, 243, 199); break;
        case 2: SetTextColorRGB(212, 212, 216); break;
        case 3: SetTextColorRGB(245, 196, 81);  break;
        case 4: SetTextColorRGB(250, 204, 21);  break;
        default: SetTextColorRGB(255, 255, 255); break;
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
        m_Projector->window().set_fullscreen(wantFs);
        m_Projector->invoke_focus_window();
        // Wayland / Hyprland: re-apply fullscreen & focus once xdg_toplevel surface maps
        slint::ComponentWeakHandle<ProjectorWindow> weakProj(m_Projector);
        slint::Timer::single_shot(std::chrono::milliseconds(60), [weakProj, wantFs]() {
            if (auto locked = weakProj.lock()) {
                if (wantFs) (*locked)->window().set_fullscreen(true);
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
        m_StaticSrcW    = w;
        m_StaticSrcH    = h;
        m_StaticSrcRgba.assign(data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        stbi_image_free(data);
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
    const bool lanNeedsVideo = IsLanRunning() && !m_LanConfig.transparentMode && m_LanConfig.sendBackground;
    int outW = 1280, outH = 720;
    GetOutputResolution(outW, outH);

    if (m_ProjectorVisible) {
        int maxW = perf ? 960 : 1280;
        int w = std::min(outW, maxW);
        int h = std::max(360, static_cast<int>(std::round(static_cast<double>(w) * outH / std::max(1, outW))));
        w &= ~1; h &= ~1;
        m_Player.SetRenderSize(w, h);
    } else if (lanNeedsVideo) {
        int w = std::clamp(m_LanConfig.frameWidth, 640, perf ? 960 : 1280);
        int h = std::clamp(m_LanConfig.frameHeight, 360, perf ? 540 : 720);
        m_Player.SetRenderSize(w, h);
    } else {
        int w = 640;
        int h = std::max(360, static_cast<int>(std::round(640.0 * outH / std::max(1, outW))));
        h &= ~1;
        m_Player.SetRenderSize(w, h);
    }
}

void SlintApp::ApplyOutputResolution(int preset) {
    m_OutputResPreset = std::clamp(preset, 0, 3);
    m_Main->set_output_res_preset(m_OutputResPreset);
    UpdatePlayerRenderSize();
    ReapplyCurrentStaticImage();
    SavePersistedState();
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
    SyncProjectorStyle();
    ApplyShaders();
    ApplyScalingMode(m_BgScaleMode);
    SyncAlertConfig();
    SyncTilesState();

    m_Projector->set_live_is_bible(m_LiveIsBible);
    m_Projector->set_live_text(m_Main->get_live_text());
    m_Projector->set_live_ref(m_Main->get_live_ref());
    m_Projector->set_bg_solid_color(m_Main->get_live_bg_solid_color());
    m_Projector->set_has_bg_image(m_Main->get_has_live_preview_image());
    if (m_Main->get_has_live_preview_image()) {
        m_Projector->set_bg_image(m_Main->get_live_preview_image());
    }
    m_Projector->set_bg_is_static_image(m_HasStaticImage);
    m_Projector->set_has_overlay_image(m_Main->get_has_live_overlay_image());
    if (m_Main->get_has_live_overlay_image()) {
        m_Projector->set_overlay_image(m_Main->get_live_overlay_image());
    }
    m_Projector->set_alert_text(m_Main->get_active_public_alert());
    m_Projector->set_show_clock_overlay(m_Main->get_show_clock_on_public());
    m_Projector->set_live_overlay_title(m_Main->get_custom_overlay_title());
    m_Projector->set_live_overlay_sub(m_Main->get_custom_overlay_sub());
    m_Projector->set_show_custom_overlay(m_Main->get_custom_overlay_active());
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
    outTitle  = p.stem().string();
    outAuthor.clear();

    // 1. Check ImGui songs_meta/<filename>.json
    std::string fname = p.filename().string();
    for (char& ch : fname) { if (ch == ':') ch = '_'; }
    fs::path metaPath = fs::path(ProyecThor::GetAssetsPath()) / "songs_meta" / (fname + ".json");
    std::ifstream mf(metaPath);
    if (mf.is_open()) {
        try {
            nlohmann::json j;
            mf >> j;
            std::string mt = j.value("title", "");
            std::string ma = j.value("artistAuthor", "");
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
}

void SlintApp::WriteSongMeta(const std::string& songPath, const std::string& title, const std::string& author) {
    fs::path p(songPath);
    std::string fname = p.filename().string();
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
    WriteSongMeta(targetPath.string(), title.empty() ? safeName : title, author);

    m_CachedTabEntries.erase(0);
    m_Main->set_song_editor_open(false);
    LoadTabData(0);

    // Select newly saved song
    for (int i = 0; i < static_cast<int>(m_FilteredIndices.size()); ++i) {
        if (m_AllEntries[m_FilteredIndices[i]].path == targetPath.string()) {
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
        q.title = fs::path(sp).stem().string();
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
    const auto& o = m_OverlayQueue[index];
    if (m_ActiveOverlayPath == o.path && m_Main->get_has_live_overlay_image()) {
        ClearOverlay();
        return;
    }
    m_ActiveOverlayPath = o.path;
    auto img = slint::Image::load_from_path(slint::SharedString(o.path));
    m_Main->set_live_overlay_image(img);
    m_Main->set_has_live_overlay_image(true);
    m_Main->set_live_overlay_name(slint::SharedString(o.title));
    if (!m_AudienceFrozen) {
        m_Projector->set_overlay_image(img);
        m_Projector->set_has_overlay_image(true);
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
}

void SlintApp::RefreshOverlayQueueModel() {
    m_OverlayQueueModel->clear();
    bool live = m_Main->get_has_live_overlay_image();
    for (const auto& o : m_OverlayQueue) {
        QueueItem item;
        item.title = slint::SharedString(o.title);
        item.kind  = slint::SharedString((live && m_ActiveOverlayPath == o.path) ? "● ON" : "PNG");
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
        p.name = "Pad " + std::to_string(index + 1) + " • " + fs::path(p.bgPath).stem().string().substr(0, 14);
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

    m_Main->set_live_text(slint::SharedString(p.text));
    m_Main->set_live_ref(slint::SharedString(p.ref));
    m_Projector->set_live_text(slint::SharedString(p.text));
    m_Projector->set_live_ref(slint::SharedString(p.ref));
    m_Stage->set_live_text(slint::SharedString(p.text));
    m_Stage->set_live_ref(slint::SharedString(p.ref));

    if (!p.bgPath.empty()) {
        m_CurrentBgPath = p.bgPath;
        m_Main->set_live_bg_name(slint::SharedString(fs::path(p.bgPath).stem().string()));
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
                sum = "Fondo: " + fs::path(m_Pads[i].bgPath).filename().string();
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
void SlintApp::LoadPersistedState() {
    fs::path path = fs::path(ProyecThor::GetAppDataRoot()) / "settings_slint.json";
    std::ifstream f(path);
    if (!f.is_open()) return;

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

        if (j.contains("savedStyles") && j["savedStyles"].is_array()) {
            m_SavedStyles.clear();
            for (const auto& js : j["savedStyles"]) {
                SavedTextStyle st;
                st.name         = js.value("name", "");
                st.textSize     = js.value("textSize", 60.0f);
                st.marginH      = js.value("marginH", 64.0f);
                st.lyricsPosX   = js.value("lyricsPosX", 0.5f);
                st.lyricsPosY   = js.value("lyricsPosY", 0.5f);
                st.lyricsWidth  = js.value("lyricsWidth", 0.90f);
                st.lyricsHeight = js.value("lyricsHeight", 0.55f);
                st.indexEnabled    = js.value("indexEnabled", true);
                st.indexPosX       = js.value("indexPosX", 0.5f);
                st.indexPosY       = js.value("indexPosY", 0.86f);
                st.indexWidth      = js.value("indexWidth", 0.50f);
                st.indexHeight     = js.value("indexHeight", 0.14f);
                st.indexSize       = js.value("indexSize", 30.0f);
                st.indexAlign      = js.value("indexAlign", 1);
                st.indexFontFamIdx = js.value("indexFontFamIdx", 0);
                st.indexFontName   = js.value("indexFontName", std::string(""));
                st.indexFontWtIdx  = js.value("indexFontWtIdx", 1);
                st.indexColorR     = static_cast<uint8_t>(js.value("indexColorR", 251));
                st.indexColorG     = static_cast<uint8_t>(js.value("indexColorG", 191));
                st.indexColorB     = static_cast<uint8_t>(js.value("indexColorB", 36));
                st.indexShadow     = js.value("indexShadow", true);
                st.indexOutline    = js.value("indexOutline", false);
                st.indexGlow       = js.value("indexGlow", false);
                st.indexBoxBg      = js.value("indexBoxBg", true);
                st.indexUppercase  = js.value("indexUppercase", false);
                st.textAlign    = js.value("textAlign", 1);
                st.textValign   = js.value("textValign", 1);
                st.colorR       = static_cast<uint8_t>(js.value("colorR", 255));
                st.colorG       = static_cast<uint8_t>(js.value("colorG", 255));
                st.colorB       = static_cast<uint8_t>(js.value("colorB", 255));
                st.fontFamIdx   = js.value("fontFamIdx", 0);
                st.fontName     = js.value("fontName", std::string(""));
                st.fontWtIdx    = js.value("fontWtIdx", 1);
                st.shadow       = js.value("shadow", true);
                st.outline      = js.value("outline", false);
                st.glow         = js.value("glow", false);
                st.neon         = js.value("neon", false);
                st.text3d       = js.value("text3d", false);
                st.underline    = js.value("underline", false);
                st.uppercase    = js.value("uppercase", false);
                st.boxBg        = js.value("boxBg", false);
                if (!st.name.empty()) m_SavedStyles.push_back(st);
            }
        }
        int colIdx = j.value("textColorIdx", 0);
        m_Main->set_text_color_idx(colIdx);
        m_Main->set_text_color(TextColorFromIndex(colIdx));
        m_Main->set_fullscreen_output(j.value("fullscreen", true));
        m_Main->set_fsr_enabled(j.value("fsr", true));
        m_BgScaleMode = std::clamp(j.value("bgScaleMode", 0), 0, 2);
        m_OutputResPreset = std::clamp(j.value("outputResPreset", 1), 0, 3);
        m_Main->set_bg_scale_mode(m_BgScaleMode);
        m_Main->set_output_res_preset(m_OutputResPreset);
        m_Projector->set_bg_scale_mode(m_BgScaleMode);
        m_Player.SetScaleMode(m_BgScaleMode);
        m_Main->set_slide_columns(std::clamp(j.value("slideColumns", 3), 1, 4));
        m_Main->set_slide_card_size(std::clamp(j.value("slideCardSize", 1), 0, 2));
        bool perf = j.value("performanceMode", false);
        m_Main->set_performance_mode(perf);
        UpdatePlayerRenderSize();
        m_ActiveThemeIdx = j.value("themeIdx", 0);
        m_MusicScanFolders.clear();
        if (j.contains("musicScanFolders") && j["musicScanFolders"].is_array()) {
            for (const auto& dirVal : j["musicScanFolders"]) {
                if (dirVal.is_string()) {
                    std::string p = dirVal.get<std::string>();
                    if (!p.empty()) m_MusicScanFolders.push_back(p);
                }
            }
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
        stylesArr.push_back({
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
            {"uppercase", st.uppercase}, {"boxBg", st.boxBg}
        });
    }
    j["savedStyles"] = stylesArr;
    j["textColorIdx"]    = m_Main->get_text_color_idx();
    j["fullscreen"]      = m_Main->get_fullscreen_output();
    j["fsr"]             = m_Main->get_fsr_enabled();
    j["bgScaleMode"]     = m_BgScaleMode;
    j["outputResPreset"] = m_OutputResPreset;
    j["slideColumns"]    = m_Main->get_slide_columns();
    j["slideCardSize"]   = m_Main->get_slide_card_size();
    j["performanceMode"] = m_Main->get_performance_mode();
    j["themeIdx"]        = m_ActiveThemeIdx;
    j["musicScanFolders"] = m_MusicScanFolders;
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
    // 1. Poll libmpv decoded video frame
    if (m_HasVideoBg) {
        slint::Image frame;
        if (m_Player.PollFrame(frame)) {
            m_Main->set_live_preview_image(frame);
            m_Main->set_has_live_preview_image(true);
            if (!m_AudienceFrozen) {
                m_Projector->set_bg_image(frame);
                m_Projector->set_has_bg_image(true);
            }
            if ((m_ActiveTab == 2 || m_ActiveTab == 3) && !m_HoverVideoActive) {
                m_Main->set_selected_media_preview(frame);
                m_Main->set_has_selected_media_preview(true);
            }
        }
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

    // 1c. Animate Public Alert Banner (Marquee Letrero & Pulse at 30 FPS — zero alloc when inactive)
    if (m_HasActivePublicAlert) {
        int anim = m_Main->get_alert_anim_mode();
        if (anim == 1) {
            m_AlertTickerPos += 0.0042f;
            if (m_AlertTickerPos > 1.0f) m_AlertTickerPos = 0.0f;
            m_Main->set_alert_ticker_pos(m_AlertTickerPos);
            if (!m_AudienceFrozen) m_Projector->set_alert_ticker_pos(m_AlertTickerPos);
        } else if (anim == 3) {
            m_AlertPulsePhase += 0.14f;
            float alpha = 0.72f + 0.28f * std::sin(m_AlertPulsePhase);
            m_Main->set_alert_pulse_alpha(alpha);
            if (!m_AudienceFrozen) m_Projector->set_alert_pulse_alpha(alpha);
        }
    }

    // 1d. Sync Independent Music Player (Tab 9 — respects ScrubBar drag + throttled writes)
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

    // 3. Clock & Countdown (update every ~1 second = 30 ticks)
    if (++m_TickCounter >= 30) {
        m_TickCounter = 0;

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
        slint::SharedString nowStr(buf);

        m_Main->set_clock_now_text(nowStr);
        if (m_StageVisible) {
            m_Stage->set_clock_text(nowStr);
        }

        if (m_TimerRunning && m_TimerRemainingSec > 0) {
            --m_TimerRemainingSec;
            if (m_TimerRemainingSec == 0) {
                m_TimerRunning = false;
                m_Main->set_timer_running(false);
            }
        }
        std::string timerStr = FormatCountdownSec(m_TimerRemainingSec);
        m_Main->set_timer_display_text(slint::SharedString(timerStr));

        if (m_StageVisible) {
            m_Stage->set_timer_text(m_TimerRunning ? slint::SharedString(timerStr) : "");
        }

        if (m_Main->get_show_clock_on_public()) {
            m_Projector->set_clock_overlay_text(
                m_Main->get_timer_mode_countdown() ? slint::SharedString(timerStr) : nowStr);
        }

        if (m_ChatServer.IsRunning()) {
            RefreshChatLog();
        }
    }
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
            std::string stem = entry.path().stem().string();
            if (std::find(seen.begin(), seen.end(), stem) != seen.end()) continue;
            seen.push_back(stem);

            std::string title, author;
            ReadSongMeta(entry.path().string(), title, author);

            MediaEntry m;
            m.title    = title;
            m.author   = author;
            m.subtitle = author.empty() ? entry.path().parent_path().filename().string() : author;
            m.path     = entry.path().string();
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
            std::string p = entry.path().string();
            if (std::find(m_BiblePaths.begin(), m_BiblePaths.end(), p) != m_BiblePaths.end()) continue;
            m_BiblePaths.push_back(p);
        }
    }

    std::sort(m_BiblePaths.begin(), m_BiblePaths.end());

    // Populate dropdown menu with all installed Bible versions
    m_BibleVersionNamesModel->clear();
    for (const auto& p : m_BiblePaths) {
        std::string label = fs::path(p).stem().string();
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

    std::string shortName = fs::path(m_BiblePaths[versionIdx]).stem().string();
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

void SlintApp::RefreshBibleBooks() {
    m_BibleBooksModel->clear();
    m_BibleFilteredBookIndices.clear();
    int filter = m_Main->get_bible_testament_filter(); // 0=Todos, 1=AT, 2=NT

    for (int i = 0; i < static_cast<int>(m_Bible.books.size()); ++i) {
        const auto& b = m_Bible.books[i];
        bool isNT = (b.canonicalNumber >= 40) || (b.canonicalNumber == 0 && i >= 39);
        if (filter == 1 && isNT)  continue;
        if (filter == 2 && !isNT) continue;

        m_BibleFilteredBookIndices.push_back(i);
        BibleBookItem item;
        item.name     = slint::SharedString(b.name);
        item.chapters = static_cast<int>(b.chapters.size());
        item.is_nt    = isNT;
        m_BibleBooksModel->push_back(item);
    }
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

void SlintApp::SearchBible(const std::string& rawQuery) {
    std::string q = ToLowerUtf8(rawQuery);
    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back() == ' ')  q.pop_back();
    if (q.empty() || m_Bible.books.empty()) return;

    // 1. Try citation parse: e.g. "juan 3:16", "gn 1 1", "sal 23", "1 cor 13:4"
    std::string bookPart;
    int chapNum = -1, verseNum = -1;
    {
        // Split trailing numbers (chapter and optional verse)
        size_t pos = q.size();
        std::vector<int> nums;
        std::string tmp = q;
        for (char& ch : tmp) {
            if (ch == ':' || ch == ',' || ch == '.') ch = ' ';
        }
        std::istringstream iss(tmp);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);

        while (!tokens.empty() && std::all_of(tokens.back().begin(), tokens.back().end(),
                                              [](unsigned char c) { return std::isdigit(c); })) {
            try { nums.insert(nums.begin(), std::stoi(tokens.back())); } catch (...) {}
            tokens.pop_back();
            if (nums.size() == 2) break;
        }
        if (!tokens.empty() && !nums.empty()) {
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i > 0) bookPart += " ";
                bookPart += tokens[i];
            }
            chapNum  = nums[0];
            verseNum = (nums.size() >= 2) ? nums[1] : -1;
        }
    }

    if (!bookPart.empty() && chapNum >= 1) {
        for (int bi = 0; bi < static_cast<int>(m_Bible.books.size()); ++bi) {
            std::string bLow = ToLowerUtf8(m_Bible.books[bi].name);
            if (bLow.find(bookPart) == 0 || bLow.find(bookPart) != std::string::npos) {
                m_Main->set_bible_submode(0);
                m_Main->set_bible_testament_filter(0);
                RefreshBibleBooks();
                m_Main->set_selected_bible_book_idx(bi);
                SelectBibleBook(bi);
                int clampedChap = std::clamp(chapNum, 1, static_cast<int>(m_Bible.books[bi].chapters.size()));
                m_Main->set_selected_chapter(clampedChap);
                LoadBibleChapter(bi, clampedChap);
                if (verseNum >= 1) {
                    int vIdx = std::clamp(verseNum - 1, 0, std::max(0, static_cast<int>(m_SlidesModel->row_count()) - 1));
                    m_Main->set_active_slide_index(vIdx);
                    ProjectSlide(vIdx);
                }
                return;
            }
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
        m_Main->set_live_overlay_image(img);
        m_Main->set_has_live_overlay_image(true);
        m_Main->set_live_overlay_name(slint::SharedString(entry.title));
        if (!m_AudienceFrozen) {
            m_Projector->set_overlay_image(img);
            m_Projector->set_has_overlay_image(true);
        }
        RefreshOverlayQueueModel();
        RefreshOverlayLayers();
    } else if (entry.isImage) {
        m_VideoQueuePlaying = false;
        m_Main->set_video_queue_playing(false);
        m_Player.Stop();
        m_HasVideoBg = false;
        m_CurrentBgPath = entry.path;
        m_Main->set_live_bg_name(slint::SharedString(entry.title));
        slint::Image img;
        RenderScaledStaticImage(entry.path, img);
        m_Main->set_live_preview_image(img);
        m_Main->set_has_live_preview_image(true);
        m_Main->set_bg_is_static_image(true);
        if (!m_AudienceFrozen) {
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
            m_Projector->set_bg_is_static_image(true);
        }
        m_HasStaticImage = true;
        if (IsLanRunning()) UpdateLanStaticFrame(img);
    } else {
        bool isBg = (m_ActiveTab == 3);
        if (!isBg) {
            m_VideoQueuePlaying = false;
            m_Main->set_video_queue_playing(false);
        }
        m_HasStaticImage = false;
        m_HasVideoBg     = entry.isVideo;
        m_CurrentBgPath  = entry.path;
        m_Main->set_bg_is_static_image(false);
        if (!m_AudienceFrozen) m_Projector->set_bg_is_static_image(false);
        m_Main->set_live_bg_name(slint::SharedString(entry.title));
        m_Main->set_media_loop(isBg);
        m_Player.Play(entry.path, isBg, isBg);
        ApplyShaders();
        m_Main->set_media_paused(false);
        m_Main->set_media_muted(isBg);
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
    m_Main->set_live_bg_name(slint::SharedString("Cola: " + item.title));

    if (item.isImage) {
        m_Player.Stop();
        m_HasVideoBg     = false;
        m_HasStaticImage = true;
        slint::Image img;
        RenderScaledStaticImage(item.path, img);
        m_Main->set_live_preview_image(img);
        m_Main->set_has_live_preview_image(true);
        m_Projector->set_bg_image(img);
        m_Projector->set_has_bg_image(true);
        if (IsLanRunning()) UpdateLanStaticFrame(img);
    } else {
        m_HasStaticImage = false;
        m_HasVideoBg     = true;
        m_Main->set_media_loop(false);
        m_Main->set_media_muted(false);
        m_Main->set_media_paused(false);
        m_Player.Play(item.path, false, false);
    }
    RefreshVideoQueueModel();
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
    for (const auto& v : chap.verses) {
        SlideItem s;
        s.badge    = slint::SharedString(book.name + " " + std::to_string(chap.number) + ":" + std::to_string(v.number));
        s.body     = slint::SharedString(v.text);
        s.ref_text = s.badge;
        m_SlidesModel->push_back(s);
    }
}

void SlintApp::ScanMedia(bool backgroundsOnly) {
    std::vector<fs::path> dirs;
    std::error_code dirEc;
    if (backgroundsOnly) {
        // Carpeta EXCLUSIVA de Fondos (igual que LayersBgTab::BgRootDir en ImGui)
        fs::path bgDir = fs::path(ProyecThor::GetAssetsPath()) / "backgrounds";
        fs::create_directories(bgDir, dirEc);
        dirs = { bgDir };
    } else {
        // Carpetas EXCLUSIVAS de Multimedia (SOLO Videos e Imágenes — Música tiene su propio panel!)
        dirs = {
            fs::path(ProyecThor::VideosPath()),
            fs::path(ProyecThor::ImagesPath())
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

            std::string fullPath = entry.path().string();
            if (std::find(seen.begin(), seen.end(), fullPath) != seen.end()) continue;
            seen.push_back(fullPath);

            MediaEntry m;
            m.title    = entry.path().stem().string();
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
            m.title     = entry.path().stem().string();
            m.subtitle  = "Overlay PNG";
            m.path      = entry.path().string();
            m.isOverlay = true;
            m_AllEntries.push_back(std::move(m));
        }
    }
}

void SlintApp::CheckGitHubUpdatesAsync() {
    m_Main->set_update_status("Consultando GitHub Releases (TheVixcho/ProyecThor)...");
    slint::ComponentWeakHandle<MainWindow> weakMain(m_Main);

    std::thread([weakMain]() {
        std::string cmd =
            "curl -fsSL -H \"User-Agent: ProyecThor\" "
            "\"https://api.github.com/repos/TheVixcho/ProyecThor/releases/latest\" 2>/dev/null";
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
    th.set_bg_root(col(p.base[0], p.base[1], p.base[2]));
    th.set_bg_surface_0(col(p.s0[0], p.s0[1], p.s0[2]));
    th.set_bg_surface_1(col(p.s1[0], p.s1[1], p.s1[2]));
    th.set_bg_surface_2(col(p.s2[0], p.s2[1], p.s2[2]));
    th.set_bg_surface_hover(col(p.s3[0], p.s3[1], p.s3[2]));
    th.set_bg_glass(col(p.s0[0], p.s0[1], p.s0[2], 0.95f));
    th.set_bg_input(col(p.base[0], p.base[1], p.base[2]));
    th.set_accent(col(p.acc[0], p.acc[1], p.acc[2]));
    th.set_accent_strong(col(p.accL[0], p.accL[1], p.accL[2]));
    th.set_accent_soft(col(p.acc[0], p.acc[1], p.acc[2], 0.18f));
    th.set_border_accent(col(p.acc[0], p.acc[1], p.acc[2], 0.55f));
    th.set_text_primary(col(p.txt[0], p.txt[1], p.txt[2]));
    th.set_text_secondary(col(p.txtD[0], p.txtD[1], p.txtD[2]));

    if (p.lightTheme) {
        th.set_border_subtle(col(0, 0, 0, 0.10f));
        th.set_border_medium(col(0, 0, 0, 0.22f));
        th.set_text_muted(col(0.35f, 0.35f, 0.38f));
        th.set_accent_fg(col(1, 1, 1));
    } else {
        th.set_border_subtle(col(1, 1, 1, 0.08f));
        th.set_border_medium(col(1, 1, 1, 0.16f));
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
        char cmd[] = "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
                     "$f = New-Object System.Windows.Forms.OpenFileDialog; "
                     "$f.Title = 'Importar archivo a ProyecThor'; "
                     "$f.Filter = 'Todos los soportados|*.txt;*.song;*.xml;*.pdf;*.mp4;*.mkv;*.mov;*.webm;*.avi;*.png;*.jpg;*.jpeg;*.webp;*.mp3;*.wav;*.flac;*.ogg;*.m4a|Todos|*.*'; "
                     "if($f.ShowDialog() -eq 'OK'){ Write-Output $f.FileName }\"";
        selectedPath = RunCommandCapture(cmd);
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

        fs::path src(selectedPath);
        std::error_code ec;
        if (!fs::exists(src, ec) || !fs::is_regular_file(src, ec)) return;

        fs::path destDir;
        int targetTab = currentTab;
        if (HasExtension(src, { ".pdf" }) || currentTab == 10) {
            destDir = ProyecThor::DocumentsPath();
            targetTab = 10;
        } else if (HasExtension(src, { ".txt", ".song" })) {
            destDir = ProyecThor::SongsPath();
            targetTab = 0;
        } else if (HasExtension(src, { ".xml" })) {
            destDir = ProyecThor::BiblesPath();
            targetTab = 1;
        } else if (currentTab == 3 && currentBgSub == 1 && HasExtension(src, { ".png", ".webp" })) {
            destDir = ProyecThor::OverlaysPath();
            targetTab = 3;
        } else if (HasExtension(src, { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" })) {
            destDir = (currentTab == 3)
                ? (fs::path(ProyecThor::GetAssetsPath()) / "backgrounds" / m_BgCurrentRelDir)
                : fs::path(ProyecThor::VideosPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".png", ".jpg", ".jpeg", ".webp", ".bmp" })) {
            destDir = (currentTab == 3)
                ? (fs::path(ProyecThor::GetAssetsPath()) / "backgrounds" / m_BgCurrentRelDir)
                : fs::path(ProyecThor::ImagesPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".opus" })) {
            destDir = fs::path(ProyecThor::GetAssetsPath()) / "audio";
            targetTab = 9;
        } else {
            destDir = ProyecThor::DocumentsPath();
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
    if (!fs::exists(path, ec)) return false;

    if (!isVideo) {
        // Resize large static images (e.g. 6000x6000) to lightweight 320x180 thumbnail so RAM stays tiny!
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (data && w > 0 && h > 0) {
            if (w > 320 || h > 180) {
                double sc = std::min(320.0 / w, 180.0 / h);
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

    // Cache video thumbnail via ffmpeg into assets/thumbnails/slint_<hash>.jpg
    fs::path thumbDir = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails";
    fs::create_directories(thumbDir, ec);
    size_t h = std::hash<std::string>{}(path);
    fs::path thumbPath = thumbDir / ("slint_" + std::to_string(h) + ".jpg");

    if (!fs::exists(thumbPath, ec)) {
        std::string cmd = "ffmpeg -y -ss 00:00:01 -i \"" + path +
                          "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + thumbPath.string() +
                          "\" >/dev/null 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0 || !fs::exists(thumbPath, ec)) {
            // Fallback at 00:00:00 for very short loops
            cmd = "ffmpeg -y -ss 00:00:00 -i \"" + path +
                  "\" -vframes 1 -vf scale=320:-1 -q:v 5 \"" + thumbPath.string() +
                  "\" >/dev/null 2>&1";
            std::system(cmd.c_str());
        }
    }

    if (fs::exists(thumbPath, ec) && fs::file_size(thumbPath, ec) > 0) {
        outImg = slint::Image::load_from_path(slint::SharedString(thumbPath.string()));
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
        fi.name     = slint::SharedString(sd.filename().string());
        fi.rel_path = slint::SharedString(fs::relative(sd, rootDir, ec).generic_string());
        fi.count    = count;
        m_BgFoldersModel->push_back(fi);
    }

    for (const auto& fp : files) {
        bool isVid = HasExtension(fp, videoExts);
        bool isImg = HasExtension(fp, imageExts);
        std::string fullPath = fp.string();
        std::string stem     = fp.stem().string();

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
    m_Main->set_live_bg_name(slint::SharedString(entry.title));
    if (entry.isImage) {
        m_Player.Stop();
        m_HasVideoBg = false;
        m_HasStaticImage = true;
        slint::Image img;
        RenderScaledStaticImage(entry.path, img);
        m_Main->set_live_preview_image(img);
        m_Main->set_has_live_preview_image(true);
        m_Main->set_bg_is_static_image(true);
        if (!m_AudienceFrozen) {
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
            m_Projector->set_bg_is_static_image(true);
        }
        if (IsLanRunning()) UpdateLanStaticFrame(img);
    } else {
        m_HasStaticImage = false;
        m_HasVideoBg = true;
        m_Main->set_bg_is_static_image(false);
        if (!m_AudienceFrozen) m_Projector->set_bg_is_static_image(false);
        m_Main->set_media_loop(true);
        m_Main->set_media_muted(true);
        m_Main->set_media_paused(false);
        m_Player.Play(entry.path, true, true);
        ApplyShaders();
    }
}

void SlintApp::RefreshOverlayLayers() {
    m_OverlayLayerEntries.clear();
    m_OverlayLayersModel->clear();

    std::vector<fs::path> dirs = {
        fs::path(ProyecThor::OverlaysPath()),
        fs::path("assets/overlays")
    };

    bool live = m_Main->get_has_live_overlay_image();
    std::vector<std::string> seen;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!HasExtension(entry.path(), { ".png", ".webp" })) continue;

            std::string fullPath = entry.path().string();
            if (std::find(seen.begin(), seen.end(), fullPath) != seen.end()) continue;
            seen.push_back(fullPath);

            MediaEntry m;
            m.title     = entry.path().stem().string();
            m.subtitle  = (live && m_ActiveOverlayPath == fullPath) ? "● ACTIVO" : "Overlay PNG";
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
    if (m_ActiveOverlayPath == entry.path && m_Main->get_has_live_overlay_image()) {
        ClearOverlay();
        return;
    }
    m_ActiveOverlayPath = entry.path;
    auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
    m_Main->set_live_overlay_image(img);
    m_Main->set_has_live_overlay_image(true);
    m_Main->set_live_overlay_name(slint::SharedString(entry.title));
    if (!m_AudienceFrozen) {
        m_Projector->set_overlay_image(img);
        m_Projector->set_has_overlay_image(true);
    }
    RefreshOverlayQueueModel();
    RefreshOverlayLayers();
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
    o.title = entry.title;
    o.path  = entry.path;
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
    int i = std::clamp(colorIdx, 0, 5);
    m_Player.Stop();
    m_HasVideoBg = false;
    m_HasStaticImage = false;
    m_CurrentBgPath.clear();
    auto c = slint::Color::from_rgb_uint8(kSolids[i].r, kSolids[i].g, kSolids[i].b);
    m_Main->set_has_live_preview_image(false);
    m_Main->set_bg_is_static_image(false);
    m_Main->set_live_bg_solid_color(c);
    m_Main->set_live_bg_name(slint::SharedString(std::string("Color: ") + kSolids[i].name));
    if (!m_AudienceFrozen) {
        m_Projector->set_has_bg_image(false);
        m_Projector->set_bg_is_static_image(false);
        m_Projector->set_bg_solid_color(c);
    }
    ClearLanFrame();
    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapBgColor[0] = kSolids[i].r / 255.0f;
        m_SnapBgColor[1] = kSolids[i].g / 255.0f;
        m_SnapBgColor[2] = kSolids[i].b / 255.0f;
        ++m_StreamVersion;
    }
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
            std::string devName = node.filename().string(); // e.g. video0
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
    m_Main->set_live_bg_name(slint::SharedString("Captura: " + dev.name));
    m_Main->set_capture_status(slint::SharedString("Capturando en vivo: " + dev.name + " (" + dev.uri + ")"));
    m_Main->set_media_paused(false);
    m_Main->set_media_muted(true);
    m_Player.PlayLiveStream(dev.uri, false, true, true);
}

// ── Network Hub: Full ImGui StreamingPanel + FrameEncodeWorker LAN Engine ────
void SlintApp::CopyToClipboard(const std::string& text) {
    if (text.empty()) return;
#ifdef _WIN32
    FILE* p = _popen("clip", "w");
    if (p) { std::fwrite(text.data(), 1, text.size(), p); _pclose(p); }
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
        s.isProjecting       = m_SnapProjecting || !m_SnapText.empty() || hasFrame || m_HasVideoBg || m_HasStaticImage;
        s.currentText        = m_SnapText;
        s.showText           = !m_SnapText.empty();
        s.textSize           = m_SnapTextSize;
        s.textAlignment      = m_SnapTextAlign;
        s.vAlignment         = m_SnapTextValign;
        s.autoScale          = false;
        s.fontFamily         = "Predeterminada";
        s.refW               = 1920;
        s.refH               = 1080;
        for (int i = 0; i < 4; ++i) s.textColor[i] = m_SnapTextColor[i];
        for (int i = 0; i < 3; ++i) s.bgColor[i]   = m_SnapBgColor[i];
        for (int i = 0; i < 4; ++i) s.margins[i]   = m_SnapMargins[i];
        s.isBgVideo          = m_HasVideoBg;
        s.hasFrame           = hasFrame;
        s.transitionTrigger  = m_SnapTransTrigger;
        s.transitionType     = m_SnapTransType;
        s.transitionDuration = m_SnapTransDur;
        s.version            = m_StreamVersion;
        s.fontVersion        = 1;
        return s;
    });

    srv.SetFrameProvider([this]() -> std::vector<uint8_t> {
        std::lock_guard<std::mutex> lk(m_LanFrameMutex);
        return m_LanLatestJpeg;
    });

    srv.SetFontPathProvider([]() -> std::string { return ""; });
}

void SlintApp::UpdateLanFrameCapture() {
    if (!IsLanRunning() || m_LanConfig.transparentMode || !m_LanConfig.sendBackground) return;
    if (!m_HasVideoBg) return;

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
            std::string p = entry.path().string();
            if (std::find(seen.begin(), seen.end(), p) != seen.end()) continue;
            seen.push_back(p);

            MediaEntry m;
            m.title    = entry.path().stem().string();
            m.subtitle = entry.path().extension().string() + " • " + entry.path().parent_path().filename().string();
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
        std::string cmd = "ffmpeg -y -i \"" + audioPath + "\" -an -vcodec mjpeg -vframes 1 -vf scale=240:-1 \"" + coverPath.string() + "\" 2>nul";
#else
        std::string cmd = "ffmpeg -y -i \"" + audioPath + "\" -an -vcodec mjpeg -vframes 1 -vf scale=240:-1 \"" + coverPath.string() + "\" >/dev/null 2>&1";
#endif
        std::system(cmd.c_str());
        if (!fs::exists(coverPath, ec) || fs::file_size(coverPath, ec) == 0) {
            fs::remove(coverPath, ec);
            // Fallback: check cover.jpg / folder.jpg in the same directory
            fs::path dir = fs::path(audioPath).parent_path();
            for (const char* name : { "cover.jpg", "cover.png", "folder.jpg", "folder.png" }) {
                if (fs::exists(dir / name, ec)) {
                    outImg = slint::Image::load_from_path(slint::SharedString((dir / name).string()));
                    m_ThumbCache.emplace(cacheKey, outImg);
                    return true;
                }
            }
            std::ofstream m(noCoverMarker);
            return false;
        }
    }

    if (fs::exists(coverPath, ec) && fs::file_size(coverPath, ec) > 0) {
        outImg = slint::Image::load_from_path(slint::SharedString(coverPath.string()));
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
    slint::Image cov;
    if (ExtractMusicCoverArt(trk.path, cov)) {
        m_Main->set_music_now_cover(cov);
        m_Main->set_has_music_now_cover(true);
    } else {
        m_Main->set_has_music_now_cover(false);
    }
    RefreshMusicModel();
    AppendDebugLog("Reproduciendo música: " + trk.title);
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
                    std::string s = e.path().filename().string();
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
        m.title      = entry.path().stem().string();
        m.subtitle   = HasExtension(entry.path(), { ".pdf" }) ? "Documento PDF" : "Diapositiva Imagen";
        m.path       = entry.path().string();
        m.isDocument = true;
        m.isImage    = !HasExtension(entry.path(), { ".pdf" });
        m_AllEntries.push_back(std::move(m));
    }
    std::sort(m_AllEntries.begin(), m_AllEntries.end(),
              [](const MediaEntry& a, const MediaEntry& b) { return a.title < b.title; });
}

void SlintApp::OpenDocumentAsync(const std::string& path) {
    m_DocPagePaths.clear();
    m_DocPagesModel->clear();
    m_ActiveDocPageIdx = -1;
    m_Main->set_active_doc_page_idx(-1);
    m_Main->set_doc_title(slint::SharedString(fs::path(path).stem().string()));

    if (!HasExtension(path, { ".pdf" })) {
        m_DocPagePaths.push_back(path);
        DocPageItem pg;
        pg.number      = 1;
        pg.preview     = slint::Image::load_from_path(slint::SharedString(path));
        pg.has_preview = true;
        m_DocPagesModel->push_back(pg);
        m_Main->set_doc_status("1 página lista. Haz clic para proyectar.");
        return;
    }

    m_Main->set_doc_status("Renderizando páginas PDF con pdftoppm...");
    std::thread([this, path]() {
        fs::path outDir = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails" / "docs" /
                          std::to_string(std::hash<std::string>{}(path));
        std::error_code ec;
        fs::create_directories(outDir, ec);

        std::vector<std::string> pages;
        for (const auto& e : fs::directory_iterator(outDir, ec)) {
            if (e.is_regular_file(ec) && HasExtension(e.path(), { ".png" })) {
                pages.push_back(e.path().string());
            }
        }
        if (pages.empty()) {
            std::string prefix = (outDir / "page").string();
#ifdef _WIN32
            std::string cmd = "pdftoppm -png -r 140 \"" + path + "\" \"" + prefix + "\" 2>nul";
#else
            std::string cmd = "pdftoppm -png -r 140 \"" + path + "\" \"" + prefix + "\" >/dev/null 2>&1";
#endif
            std::system(cmd.c_str());
            for (const auto& e : fs::directory_iterator(outDir, ec)) {
                if (e.is_regular_file(ec) && HasExtension(e.path(), { ".png" })) {
                    pages.push_back(e.path().string());
                }
            }
        }
        std::sort(pages.begin(), pages.end());

        slint::invoke_from_event_loop([this, pages = std::move(pages)]() mutable {
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
            }
        });
    }).detach();
}

void SlintApp::ProjectDocPage(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= static_cast<int>(m_DocPagePaths.size())) return;
    m_ActiveDocPageIdx = pageIdx;
    const std::string& p = m_DocPagePaths[pageIdx];

    m_Player.Stop();
    m_HasVideoBg     = false;
    m_HasStaticImage = true;
    m_CurrentBgPath  = p;
    slint::Image img;
    RenderScaledStaticImage(p, img);

    m_Main->set_live_bg_name(slint::SharedString("Doc Pág. " + std::to_string(pageIdx + 1)));
    m_Main->set_bg_is_static_image(true);
    m_Main->set_live_preview_image(img);
    m_Main->set_has_live_preview_image(true);
    if (!m_AudienceFrozen) {
        m_Projector->set_bg_is_static_image(true);
        m_Projector->set_bg_image(img);
        m_Projector->set_has_bg_image(true);
    }
    if (IsLanRunning()) UpdateLanStaticFrame(img);
}

// ── Changelog, Debug & Wi-Fi Firewall Diagnostics ────────────────────────────
void SlintApp::LoadChangelogText() {
    std::ostringstream oss;
    oss << "=== VERSIÓN " << PROYECTHOR_VERSION_STRING << " BETA (SLINT UI + LIBMPV) ===\n"
        << "• Nueva interfaz moderna Slint 1.19 + Dark Carbon + 14 temas completos.\n"
        << "• Panel Capas permanente a la derecha (Fondos con hover-preview, Overlays PNG y Notas).\n"
        << "• Botón Freeze (Congelar salida secundaria) y Escalado / Nitidez FSR.\n"
        << "• Música independiente (sin cortar video), Documentos PDF, Ayuda y Consola Debug.\n"
        << "• Centro de Redes unificado con Video LAN (8080), Smart TV DLNA, Chat LAN (8082) y Celular (8090) independientes.\n\n";

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

void SlintApp::RefreshDebugInfo() {
    static const char* kScaleNames[] = { "Recortar (Cover)", "Estirar (Fill)", "Original (Contain)" };
    std::ostringstream st;
    st << "Versión: v" << PROYECTHOR_VERSION_STRING << " (Slint 1.19 + libmpv)  |  "
       << "Público: " << (m_ProjectorVisible ? "ACTIVO" : "OFF") << "  |  "
       << "Freeze: " << (m_AudienceFrozen ? "CONGELADO" : "NO") << "\n"
       << "Escalado: " << kScaleNames[std::clamp(m_BgScaleMode, 0, 2)]
       << "  |  FSR Nitidez: " << (m_Main->get_fsr_enabled() ? "ON" : "OFF") << "\n"
       << "Video LAN (8080): " << (IsLanRunning() ? m_LanServer->GetBaseURL() : "OFF")
       << "  |  Chat LAN (8082): " << (m_ChatServer.IsRunning() ? "ON" : "OFF")
       << "  |  Celular (8090): " << (m_MobileServer.IsRunning() ? "ON" : "OFF") << "\n"
       << "Reproductor Video: " << (m_Player.IsPlaying() ? "PLAYING" : "IDLE")
       << "  |  Reproductor Música: " << (m_MusicPlayer.IsPlaying() ? "PLAYING" : "IDLE")
       << "  |  Tabs Cacheadas: " << m_CachedTabEntries.size();
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
