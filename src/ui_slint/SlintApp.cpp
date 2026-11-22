#include "SlintApp.h"
#include "AppPaths.h"
#include "BibleXmlIO.h"
#include "OSCSender.h"
#include "Version.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
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
    , m_QueueModel(std::make_shared<slint::VectorModel<QueueItem>>())
    , m_PadsModel(std::make_shared<slint::VectorModel<PadItem>>())
    , m_CaptureModel(std::make_shared<slint::VectorModel<CaptureDeviceItem>>())
    , m_SavedStylesModel(std::make_shared<slint::VectorModel<slint::SharedString>>())
    , m_BgDockModel(std::make_shared<slint::VectorModel<LibraryItem>>())
{
    for (int i = 0; i < 8; ++i) {
        m_Pads[i].name = "Pad " + std::to_string(i + 1);
    }

    m_Splash->set_app_version(slint::SharedString(PROYECTHOR_VERSION_STRING));
    m_Main->set_app_version(slint::SharedString(PROYECTHOR_VERSION_STRING));
    m_Main->set_library_items(m_LibraryModel);
    m_Main->set_slides(m_SlidesModel);
    m_Main->set_queue_items(m_QueueModel);
    m_Main->set_pads(m_PadsModel);
    m_Main->set_capture_devices(m_CaptureModel);
    m_Main->set_saved_style_names(m_SavedStylesModel);
    m_Main->set_bg_dock_items(m_BgDockModel);

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
        return slint::CloseRequestResponse::HideWindow;
    });
    m_Stage->window().on_close_requested([this]() {
        m_StageVisible = false;
        m_Main->set_stage_live(false);
        return slint::CloseRequestResponse::HideWindow;
    });

    BindCallbacks();
    LoadPersistedState();
    ApplyThemePreset(m_ActiveThemeIdx);
    RefreshPadsModel();
    RefreshSavedStylesModel();
    RefreshBgDock();
    SyncProjectorStyle();
    SyncTilesState();

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

    // Configure LAN Stream Server
    m_LanServer.SetChatStore(&m_ChatStore);
    m_LanServer.SetSnapshotProvider([this]() {
        ProyecThor::Core::StreamSnapshot s;
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        s.isProjecting  = m_SnapProjecting;
        s.currentText   = m_SnapText;
        s.showText      = !m_SnapText.empty();
        s.textSize      = m_SnapTextSize;
        s.textAlignment = m_SnapTextAlign;
        s.vAlignment    = m_SnapTextValign;
        s.version       = m_StreamVersion;
        return s;
    });
    (void)weakMain;

    // 30 FPS tick timer for libmpv video frames, playback progress, clock & countdown
    m_TickTimer = std::make_unique<slint::Timer>(
        std::chrono::milliseconds(33),
        [this]() { OnTick(); }
    );
}

SlintApp::~SlintApp() {
    SavePersistedState();
    m_LanServer.Stop();
    m_MobileServer.Stop();
    m_SplashTimer.reset();
    m_TickTimer.reset();
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

    std::string bgPath = FindFirstExisting({
        "splash_bg1.jpg",
        "assets/splash_bg1.jpg",
        "../assets/splash_bg1.jpg",
        "bg_splash3.jpg",
        "assets/bg_splash3.jpg"
    });
    if (!bgPath.empty()) {
        auto bg = slint::Image::load_from_path(slint::SharedString(bgPath));
        m_Splash->set_bg_image(bg);
        m_Splash->set_has_bg(true);
    }
}

int SlintApp::Run() {
    // Show animated splash screen first, load libraries step-by-step, then open MainWindow
    m_SplashStep = 0;
    m_Splash->set_progress(0.18f);
    m_Splash->set_status_text("Inicializando motor libmpv (GPU)...");
    m_Splash->show();

    m_SplashTimer = std::make_unique<slint::Timer>(
        std::chrono::milliseconds(260),
        [this]() {
            ++m_SplashStep;
            if (m_SplashStep == 1) {
                m_Splash->set_progress(0.48f);
                m_Splash->set_status_text("Cargando bibliotecas de canciones y Biblias...");
                LoadTabData(0);
            } else if (m_SplashStep == 2) {
                m_Splash->set_progress(0.82f);
                m_Splash->set_status_text("Preparando interfaz Dark Carbon (Slint)...");
            } else if (m_SplashStep == 3) {
                m_Splash->set_progress(1.0f);
                m_Splash->set_status_text("Listo.");
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
        m_ActiveTab = tab;
        m_SearchQuery.clear();
        m_Main->set_search_query("");
        m_Main->set_song_editor_open(false);
        if (tab < 4) {
            LoadTabData(tab);
        } else if (tab == 8) {
            RefreshCaptureDevices();
        }
    });

    m_Main->on_bg_subtab_changed([this](int sub) {
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

    // Song Editor
    m_Main->on_new_song_clicked([this]() { OpenNewSongEditor(); });
    m_Main->on_edit_song_clicked([this]() { OpenEditSongEditor(); });
    m_Main->on_save_song_clicked([this](slint::SharedString t, slint::SharedString b) {
        SaveEditedSong(std::string(t), std::string(b));
    });
    m_Main->on_cancel_song_editor([this]() {
        m_Main->set_song_editor_open(false);
    });

    // Service Queue
    m_Main->on_add_current_to_queue([this]() { AddCurrentToQueue(); });
    m_Main->on_queue_item_clicked([this](int idx) { ActivateQueueItem(idx); });
    m_Main->on_clear_queue([this]() {
        m_Queue.clear();
        RefreshQueueModel();
    });

    // Bible
    m_Main->on_cycle_bible_version([this]() {
        if (m_BiblePaths.empty()) return;
        m_CurrentBibleVersionIdx = (m_CurrentBibleVersionIdx + 1) % static_cast<int>(m_BiblePaths.size());
        LoadSelectedBible(m_CurrentBibleVersionIdx);
    });

    m_Main->on_bible_chapter_changed([this](int chap) {
        if (m_CurrentBibleBookIdx >= 0) {
            LoadBibleChapter(m_CurrentBibleBookIdx, chap);
        }
    });

    // Live Output
    m_Main->on_toggle_audience([this]() { ToggleAudienceWindow(); });
    m_Main->on_toggle_stage([this]() { ToggleStageWindow(); });
    m_Main->on_clear_text([this]() { ClearText(); });
    m_Main->on_clear_background([this]() { ClearBackground(); });
    m_Main->on_clear_overlay([this]() { ClearOverlay(); });
    m_Main->on_clear_all([this]() { ClearAll(); });

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
    });

    // Design, Shaders, Transitions, Tiles & Alerts
    m_Main->on_style_changed([this]() {
        SyncProjectorStyle();
        SavePersistedState();
    });
    m_Main->on_apply_style_preset([this](int idx) { ApplyStylePreset(idx); });
    m_Main->on_save_custom_style([this](slint::SharedString name) { SaveCustomStyle(std::string(name)); });
    m_Main->on_load_custom_style([this](int idx) { LoadCustomStyle(idx); });
    m_Main->on_delete_custom_style([this](int idx) {
        if (idx >= 0 && idx < static_cast<int>(m_SavedStyles.size())) {
            m_SavedStyles.erase(m_SavedStyles.begin() + idx);
            RefreshSavedStylesModel();
            SavePersistedState();
        }
    });
    m_Main->on_text_color_selected([this](int idx) { SelectTextColor(idx); });
    m_Main->on_shaders_changed([this]() { ApplyShaders(); });
    m_Main->on_reset_shaders([this]() { ResetShaders(); });
    m_Main->on_eq_changed([this]() { ApplyEqualizer(); });
    m_Main->on_apply_eq_preset([this](int idx) { ApplyEqPreset(idx); });
    m_Main->on_transition_changed([this]() {
        SyncProjectorStyle();
        SavePersistedState();
    });
    m_Main->on_test_transition([this]() { TriggerSlideTransition(); });
    m_Main->on_tiles_changed([this]() {
        SyncTilesState();
        SavePersistedState();
    });

    m_Main->on_send_public_alert([this](slint::SharedString msg) {
        m_Main->set_active_public_alert(msg);
        m_Projector->set_alert_text(msg);
    });
    m_Main->on_send_stage_alert([this](slint::SharedString msg) {
        m_Main->set_active_stage_alert(msg);
        m_Stage->set_stage_alert(msg);
    });
    m_Main->on_clear_alerts([this]() {
        m_Main->set_active_public_alert("");
        m_Main->set_active_stage_alert("");
        m_Projector->set_alert_text("");
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

    // Capture & URL to Screen
    m_Main->on_send_url_to_screen([this](slint::SharedString url, bool loop, bool muted) {
        SendUrlToScreen(std::string(url), loop, muted);
    });
    m_Main->on_refresh_capture_devices([this]() { RefreshCaptureDevices(); });
    m_Main->on_start_capture_device([this](int idx) { StartCaptureDevice(idx); });
    m_Main->on_stop_capture_or_url([this]() {
        m_Main->set_selected_capture_idx(-1);
        ClearBackground();
        m_Main->set_capture_status("Señal detenida.");
    });

    // Network Hub
    m_Main->on_toggle_lan_stream([this]() { ToggleLanStream(); });
    m_Main->on_lan_config_changed([this]() { ApplyLanConfig(); });
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
    m_Main->on_import_file_clicked([this]() { ImportFileAsync(false); });
    m_Main->on_import_background_clicked([this]() { ImportFileAsync(true); });
    m_Main->on_dock_bg_selected([this](int idx) { SelectDockBackground(idx); });
    m_Main->on_dock_solid_color_selected([this](int idx) { SelectSolidBgColor(idx); });
    m_Main->on_custom_overlay_toggled([this](slint::SharedString t, slint::SharedString s, bool show) {
        m_Projector->set_live_overlay_title(t);
        m_Projector->set_live_overlay_sub(s);
        m_Projector->set_show_custom_overlay(show);
    });
    m_Main->on_open_legacy_imgui([this]() { LaunchLegacyImGui(); });
    m_Main->on_fsr_changed([this](bool) { SavePersistedState(); });
    m_Main->on_performance_mode_changed([this](bool lowRes) {
        m_Player.SetLowResMode(lowRes);
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
    m_Main->set_selected_library_index(-1);
    m_Main->set_active_slide_index(-1);
    m_Main->set_chapter_count(1);

    if (tab == 0) {
        m_Main->set_selected_item_title("Canciones — Selecciona una canción");
        ScanSongs();
    } else if (tab == 1) {
        m_Main->set_selected_item_title("Biblia — Selecciona un libro");
        ScanBibles();
    } else if (tab == 2) {
        m_Main->set_selected_item_title("Multimedia — Videos, Imágenes y Audio");
        ScanMedia(false);
    } else if (tab == 3) {
        if (m_BgSubtab == 0) {
            m_Main->set_selected_item_title("Fondos en Bucle");
            ScanMedia(true);
        } else {
            m_Main->set_selected_item_title("Overlays PNG Transparentes");
            ScanOverlays();
        }
    }

    RefreshLibraryModel();
}

void SlintApp::RefreshLibraryModel() {
    m_LibraryModel->clear();
    m_FilteredIndices.clear();

    std::string needle = ToLowerUtf8(m_SearchQuery);
    for (int i = 0; i < static_cast<int>(m_AllEntries.size()); ++i) {
        const auto& e = m_AllEntries[i];
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
    } else if (m_ActiveTab == 1) {
        if (realIdx >= 0 && realIdx < static_cast<int>(m_Bible.books.size())) {
            m_CurrentBibleBookIdx = realIdx;
            int chapCount = static_cast<int>(m_Bible.books[realIdx].chapters.size());
            m_Main->set_chapter_count(std::max(1, chapCount));
            m_Main->set_selected_chapter(1);
            LoadBibleChapter(realIdx, 1);
        }
    } else if (m_ActiveTab == 2 || m_ActiveTab == 3) {
        m_Main->set_selected_item_title(slint::SharedString(entry.title));
        m_Main->set_selected_media_info(slint::SharedString(entry.subtitle + " • " + fs::path(entry.path).filename().string()));

        slint::Image thumb;
        if (LoadOrCreateThumbnail(entry.path, entry.isVideo, thumb)) {
            m_Main->set_selected_media_preview(thumb);
            m_Main->set_has_selected_media_preview(true);
        } else {
            m_Main->set_has_selected_media_preview(false);
        }

        if (entry.isOverlay) {
            auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
            m_Main->set_live_overlay_image(img);
            m_Main->set_has_live_overlay_image(true);
            m_Main->set_live_overlay_name(slint::SharedString(entry.title));
            m_Projector->set_overlay_image(img);
            m_Projector->set_has_overlay_image(true);
        } else if (entry.isImage) {
            m_Player.Stop();
            m_HasVideoBg = false;
            m_CurrentBgPath = entry.path;
            m_Main->set_live_bg_name(slint::SharedString(entry.title));
            auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
            m_Main->set_live_preview_image(img);
            m_Main->set_has_live_preview_image(true);
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
            m_HasStaticImage = true;
        } else {
            bool isBg = (m_ActiveTab == 3);
            m_HasStaticImage = false;
            m_HasVideoBg = entry.isVideo;
            m_CurrentBgPath = entry.path;
            m_Main->set_live_bg_name(slint::SharedString(entry.title));
            m_Main->set_media_loop(isBg);
            m_Player.Play(entry.path, isBg, isBg);
            m_Main->set_media_paused(false);
            m_Main->set_media_muted(isBg);
        }

        m_SlidesModel->clear();
        m_Main->set_active_slide_index(-1);
    }
}

void SlintApp::ProjectSlide(int index) {
    if (index < 0 || index >= static_cast<int>(m_SlidesModel->row_count())) return;
    if (m_ActiveTab >= 2) return;

    auto slide = m_SlidesModel->row_data(index);
    if (!slide.has_value()) return;

    m_RawLiveText = std::string(slide->body);
    std::string displayStr = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
    slint::SharedString dispText(displayStr);

    m_Main->set_live_text(dispText);
    m_Main->set_live_ref(slide->ref_text);
    m_Projector->set_live_text(dispText);
    m_Projector->set_live_ref(slide->ref_text);
    m_Stage->set_live_text(dispText);
    m_Stage->set_live_ref(slide->ref_text);
    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapText = displayStr;
        m_SnapRef  = std::string(slide->ref_text);
        ++m_StreamVersion;
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
    m_Main->set_live_text("");
    m_Main->set_live_ref("");
    m_Main->set_active_slide_index(-1);
    m_Projector->set_live_text("");
    m_Projector->set_live_ref("");
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
    m_Main->set_has_live_preview_image(false);
    m_Main->set_live_bg_name("Sin fondo");
    m_Projector->set_has_bg_image(false);
    m_Main->set_media_position(0.0f);
    m_Main->set_media_time_label("00:00 / 00:00");
}

void SlintApp::ClearOverlay() {
    m_Main->set_has_live_overlay_image(false);
    m_Main->set_live_overlay_name("Sin overlay");
    m_Projector->set_has_overlay_image(false);
}

void SlintApp::ClearAll() {
    ClearText();
    ClearBackground();
    ClearOverlay();
}

void SlintApp::SyncProjectorStyle() {
    static const char* kFontFamilies[] = { "", "Sans", "Serif", "Monospace" };
    static const int   kFontWeights[]  = { 400, 700, 900 };
    int famIdx = std::clamp(m_Main->get_font_family_idx(), 0, 3);
    int wtIdx  = std::clamp(m_Main->get_font_weight_idx(), 0, 2);

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
    m_Projector->set_font_family_name(slint::SharedString(kFontFamilies[famIdx]));
    m_Projector->set_font_weight_val(kFontWeights[wtIdx]);
    m_Projector->set_text_margin_h(m_Main->get_text_margin_h());
    m_Projector->set_lyrics_pos_x(m_Main->get_lyrics_pos_x());
    m_Projector->set_lyrics_pos_y(m_Main->get_lyrics_pos_y());
    m_Projector->set_lyrics_width(m_Main->get_lyrics_width());
    m_Projector->set_index_box_enabled(m_Main->get_index_box_enabled());
    m_Projector->set_index_pos_x(m_Main->get_index_pos_x());
    m_Projector->set_index_pos_y(m_Main->get_index_pos_y());
    m_Projector->set_index_size(m_Main->get_index_size());
    m_Projector->set_transition_type(m_Main->get_transition_type());
    m_Projector->set_transition_dur(static_cast<int64_t>(m_Main->get_transition_duration_ms()));

    // Re-apply uppercase toggle to current live text immediately
    if (!m_RawLiveText.empty()) {
        std::string disp = m_Main->get_text_uppercase() ? ToUpperUtf8(m_RawLiveText) : m_RawLiveText;
        slint::SharedString s(disp);
        m_Main->set_live_text(s);
        m_Projector->set_live_text(s);
        m_Stage->set_live_text(s);
    }

    {
        std::lock_guard<std::mutex> lk(m_LiveSnapMutex);
        m_SnapTextSize   = m_Main->get_text_size();
        m_SnapTextAlign  = m_Main->get_text_align();
        m_SnapTextValign = m_Main->get_text_valign();
        ++m_StreamVersion;
    }
}

void SlintApp::SaveCustomStyle(const std::string& name) {
    if (name.empty()) return;
    SavedTextStyle st;
    st.name         = name;
    st.textSize     = m_Main->get_text_size();
    st.marginH      = m_Main->get_text_margin_h();
    st.lyricsPosX   = m_Main->get_lyrics_pos_x();
    st.lyricsPosY   = m_Main->get_lyrics_pos_y();
    st.lyricsWidth  = m_Main->get_lyrics_width();
    st.indexEnabled = m_Main->get_index_box_enabled();
    st.indexPosX    = m_Main->get_index_pos_x();
    st.indexPosY    = m_Main->get_index_pos_y();
    st.indexSize    = m_Main->get_index_size();
    st.textAlign    = m_Main->get_text_align();
    st.textValign   = m_Main->get_text_valign();
    st.colorIdx     = m_Main->get_text_color_idx();
    st.fontFamIdx   = m_Main->get_font_family_idx();
    st.fontWtIdx    = m_Main->get_font_weight_idx();
    st.shadow       = m_Main->get_text_shadow();
    st.outline      = m_Main->get_text_outline();
    st.glow         = m_Main->get_text_glow();
    st.neon         = m_Main->get_text_neon();
    st.text3d       = m_Main->get_text_3d();
    st.underline    = m_Main->get_text_underline();
    st.uppercase    = m_Main->get_text_uppercase();
    st.boxBg        = m_Main->get_text_box_bg();

    bool replaced = false;
    for (auto& existing : m_SavedStyles) {
        if (existing.name == name) {
            existing = st;
            replaced = true;
            break;
        }
    }
    if (!replaced) m_SavedStyles.push_back(st);
    RefreshSavedStylesModel();
    SavePersistedState();
}

void SlintApp::LoadCustomStyle(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_SavedStyles.size())) return;
    const auto& st = m_SavedStyles[idx];
    m_Main->set_text_size(st.textSize);
    m_Main->set_text_margin_h(st.marginH);
    m_Main->set_lyrics_pos_x(st.lyricsPosX);
    m_Main->set_lyrics_pos_y(st.lyricsPosY);
    m_Main->set_lyrics_width(st.lyricsWidth);
    m_Main->set_index_box_enabled(st.indexEnabled);
    m_Main->set_index_pos_x(st.indexPosX);
    m_Main->set_index_pos_y(st.indexPosY);
    m_Main->set_index_size(st.indexSize);
    m_Main->set_text_align(st.textAlign);
    m_Main->set_text_valign(st.textValign);
    m_Main->set_font_family_idx(st.fontFamIdx);
    m_Main->set_font_weight_idx(st.fontWtIdx);
    m_Main->set_text_shadow(st.shadow);
    m_Main->set_text_outline(st.outline);
    m_Main->set_text_glow(st.glow);
    m_Main->set_text_neon(st.neon);
    m_Main->set_text_3d(st.text3d);
    m_Main->set_text_underline(st.underline);
    m_Main->set_text_uppercase(st.uppercase);
    m_Main->set_text_box_bg(st.boxBg);
    SelectTextColor(st.colorIdx);
}

void SlintApp::RefreshSavedStylesModel() {
    m_SavedStylesModel->clear();
    for (const auto& st : m_SavedStyles) {
        m_SavedStylesModel->push_back(slint::SharedString(st.name));
    }
}

void SlintApp::ApplyShaders() {
    m_Player.SetVideoFilters(
        static_cast<int>(m_Main->get_shader_brightness()),
        static_cast<int>(m_Main->get_shader_contrast()),
        static_cast<int>(m_Main->get_shader_saturation()),
        static_cast<int>(m_Main->get_shader_gamma()),
        m_Main->get_shader_blur(),
        m_Main->get_shader_preset());
    m_Projector->set_vignette_enabled(m_Main->get_shader_vignette());
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

void SlintApp::ApplyStylePreset(int presetIdx) {
    if (presetIdx == 0) { // Clásico Centrado
        m_Main->set_text_size(64.0f);
        m_Main->set_text_align(1);
        m_Main->set_text_valign(1);
        m_Main->set_text_shadow(true);
        m_Main->set_text_box_bg(false);
        SelectTextColor(0);
    } else if (presetIdx == 1) { // Subtítulo Inferior
        m_Main->set_text_size(44.0f);
        m_Main->set_text_align(1);
        m_Main->set_text_valign(2);
        m_Main->set_text_shadow(true);
        m_Main->set_text_box_bg(true);
        SelectTextColor(0);
    } else if (presetIdx == 2) { // Biblia Elegante
        m_Main->set_text_size(52.0f);
        m_Main->set_text_align(0);
        m_Main->set_text_valign(1);
        m_Main->set_text_shadow(true);
        m_Main->set_text_box_bg(true);
        SelectTextColor(1);
    } else if (presetIdx == 3) { // Neón / Glow
        m_Main->set_text_size(70.0f);
        m_Main->set_text_align(1);
        m_Main->set_text_valign(1);
        m_Main->set_text_shadow(true);
        m_Main->set_text_outline(true);
        m_Main->set_text_glow(true);
        m_Main->set_text_box_bg(false);
        SelectTextColor(0);
    }
    SyncProjectorStyle();
    SavePersistedState();
}

void SlintApp::SelectTextColor(int colorIdx) {
    m_Main->set_text_color_idx(colorIdx);
    m_Main->set_text_color(TextColorFromIndex(colorIdx));
    SyncProjectorStyle();
    SavePersistedState();
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
        SyncProjectorStyle();
        m_Projector->show();
        m_Projector->window().set_fullscreen(m_Main->get_fullscreen_output());
    } else {
        m_Projector->hide();
    }
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

// ── Song Editor ──────────────────────────────────────────────────────────────
void SlintApp::OpenNewSongEditor() {
    m_EditingSongPath.clear();
    m_Main->set_editor_song_title("");
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
    m_Main->set_editor_song_body(slint::SharedString(ss.str()));
    m_Main->set_song_editor_open(true);
}

void SlintApp::SaveEditedSong(const std::string& title, const std::string& body) {
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
            auto img = slint::Image::load_from_path(slint::SharedString(p.bgPath));
            m_Main->set_live_preview_image(img);
            m_Main->set_has_live_preview_image(true);
            m_Projector->set_bg_image(img);
            m_Projector->set_has_bg_image(true);
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
        m_Main->set_font_family_idx(j.value("fontFamIdx", 0));
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
                st.name       = js.value("name", "");
                st.textSize   = js.value("textSize", 60.0f);
                st.marginH    = js.value("marginH", 64.0f);
                st.textAlign  = js.value("textAlign", 1);
                st.textValign = js.value("textValign", 1);
                st.colorIdx   = js.value("colorIdx", 0);
                st.fontFamIdx = js.value("fontFamIdx", 0);
                st.fontWtIdx  = js.value("fontWtIdx", 1);
                st.shadow     = js.value("shadow", true);
                st.outline    = js.value("outline", false);
                st.glow       = js.value("glow", false);
                st.uppercase  = js.value("uppercase", false);
                st.boxBg      = js.value("boxBg", false);
                if (!st.name.empty()) m_SavedStyles.push_back(st);
            }
        }
        int colIdx = j.value("textColorIdx", 0);
        m_Main->set_text_color_idx(colIdx);
        m_Main->set_text_color(TextColorFromIndex(colIdx));
        m_Main->set_fullscreen_output(j.value("fullscreen", true));
        m_Main->set_fsr_enabled(j.value("fsr", true));
        bool perf = j.value("performanceMode", false);
        m_Main->set_performance_mode(perf);
        m_Player.SetLowResMode(perf);
        m_ActiveThemeIdx = j.value("themeIdx", 0);
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
            {"textAlign", st.textAlign}, {"textValign", st.textValign}, {"colorIdx", st.colorIdx},
            {"fontFamIdx", st.fontFamIdx}, {"fontWtIdx", st.fontWtIdx},
            {"shadow", st.shadow}, {"outline", st.outline}, {"glow", st.glow},
            {"uppercase", st.uppercase}, {"boxBg", st.boxBg}
        });
    }
    j["savedStyles"] = stylesArr;
    j["textColorIdx"]    = m_Main->get_text_color_idx();
    j["fullscreen"]      = m_Main->get_fullscreen_output();
    j["fsr"]             = m_Main->get_fsr_enabled();
    j["performanceMode"] = m_Main->get_performance_mode();
    j["themeIdx"]        = m_ActiveThemeIdx;
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
            m_Projector->set_bg_image(frame);
            m_Projector->set_has_bg_image(true);
            if (m_ActiveTab == 2 || m_ActiveTab == 3) {
                m_Main->set_selected_media_preview(frame);
                m_Main->set_has_selected_media_preview(true);
            }
        }
    }

    // 2. Sync transport position & time label
    if (m_Player.IsPlaying()) {
        m_Main->set_media_position(m_Player.GetPosition());
        std::string label = FormatTimeMs(m_Player.GetTimeMs()) + " / " + FormatTimeMs(m_Player.GetLengthMs());
        m_Main->set_media_time_label(slint::SharedString(label));
        m_Main->set_media_paused(m_Player.IsPaused());
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

        if (m_LanServer.IsRunning()) {
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
            std::string title = entry.path().stem().string();
            if (std::find(seen.begin(), seen.end(), title) != seen.end()) continue;
            seen.push_back(title);

            MediaEntry m;
            m.title    = title;
            m.subtitle = entry.path().parent_path().filename().string();
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

    std::string shortName = fs::path(m_BiblePaths[versionIdx]).stem().string();
    auto pos = shortName.find("Bible");
    if (pos != std::string::npos) shortName = shortName.substr(0, pos);
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
    RefreshLibraryModel();
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
        // Carpetas EXCLUSIVAS de Multimedia (Videos, Imágenes y Audio — separadas de Fondos)
        dirs = {
            fs::path(ProyecThor::VideosPath()),
            fs::path(ProyecThor::ImagesPath()),
            fs::path(ProyecThor::GetAssetsPath()) / "audio"
        };
    }

    const std::vector<std::string> videoExts = { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" };
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };
    const std::vector<std::string> audioExts = { ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".opus" };

    std::vector<std::string> seen;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            bool isVid = HasExtension(entry.path(), videoExts);
            bool isImg = HasExtension(entry.path(), imageExts);
            bool isAud = !backgroundsOnly && HasExtension(entry.path(), audioExts);
            if (!isVid && !isImg && !isAud) continue;

            std::string fullPath = entry.path().string();
            if (std::find(seen.begin(), seen.end(), fullPath) != seen.end()) continue;
            seen.push_back(fullPath);

            MediaEntry m;
            m.title    = entry.path().stem().string();
            m.subtitle = isVid ? "Video" : (isImg ? "Imagen" : "Audio");
            m.path     = fullPath;
            m.isVideo  = isVid;
            m.isImage  = isImg;
            m.isAudio  = isAud;
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
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        bool blank = line.find_first_not_of(" \t") == std::string::npos;
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
void SlintApp::ImportFileAsync(bool forceBackgroundFolder) {
    int currentTab = forceBackgroundFolder ? 3 : m_ActiveTab;
    int currentBgSub = forceBackgroundFolder ? 0 : m_BgSubtab;

    std::thread([this, currentTab, currentBgSub]() {
        std::string selectedPath;
#ifdef _WIN32
        char cmd[] = "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
                     "$f = New-Object System.Windows.Forms.OpenFileDialog; "
                     "$f.Title = 'Importar archivo a ProyecThor'; "
                     "$f.Filter = 'Todos los soportados|*.txt;*.song;*.xml;*.mp4;*.mkv;*.mov;*.webm;*.avi;*.png;*.jpg;*.jpeg;*.webp;*.mp3;*.wav;*.flac;*.ogg|Todos|*.*'; "
                     "if($f.ShowDialog() -eq 'OK'){ Write-Output $f.FileName }\"";
        selectedPath = RunCommandCapture(cmd);
#else
        // Try zenity first, fallback to kdialog
        selectedPath = RunCommandCapture(
            "zenity --file-selection --title=\"Importar archivo a ProyecThor\" "
            "--file-filter=\"Archivos soportados | *.txt *.song *.xml *.mp4 *.mkv *.mov *.webm *.avi *.png *.jpg *.jpeg *.webp *.mp3 *.wav *.flac *.ogg\" "
            "--file-filter=\"Todos los archivos | *\" 2>/dev/null");
        if (selectedPath.empty()) {
            selectedPath = RunCommandCapture(
                "kdialog --getopenfilename ~ \"*.txt *.song *.xml *.mp4 *.mkv *.mov *.webm *.png *.jpg *.jpeg *.webp *.mp3 *.wav *.flac\" 2>/dev/null");
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
        if (HasExtension(src, { ".txt", ".song" })) {
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
                ? (fs::path(ProyecThor::GetAssetsPath()) / "backgrounds")
                : fs::path(ProyecThor::VideosPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".png", ".jpg", ".jpeg", ".webp", ".bmp" })) {
            destDir = (currentTab == 3)
                ? (fs::path(ProyecThor::GetAssetsPath()) / "backgrounds")
                : fs::path(ProyecThor::ImagesPath());
            targetTab = (currentTab == 3) ? 3 : 2;
        } else if (HasExtension(src, { ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".opus" })) {
            destDir = fs::path(ProyecThor::GetAssetsPath()) / "audio";
            targetTab = 2;
        } else {
            destDir = ProyecThor::DocumentsPath();
        }

        fs::create_directories(destDir, ec);
        fs::path destFile = destDir / src.filename();
        fs::copy_file(src, destFile, fs::copy_options::overwrite_existing, ec);

        slint::invoke_from_event_loop([this, targetTab]() {
            RefreshBgDock();
            if (targetTab < 4) {
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
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;

    if (!isVideo) {
        outImg = slint::Image::load_from_path(slint::SharedString(path));
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
        return true;
    }
    return false;
}

void SlintApp::RefreshBgDock() {
    m_BgDockEntries.clear();
    m_BgDockModel->clear();

    // Carpeta EXCLUSIVA de Fondos (separada de Multimedia/Videos!)
    fs::path bgDir = fs::path(ProyecThor::GetAssetsPath()) / "backgrounds";
    std::error_code ec;
    fs::create_directories(bgDir, ec);

    const std::vector<std::string> videoExts = { ".mp4", ".mkv", ".mov", ".webm", ".avi", ".wmv" };
    const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".webp", ".bmp" };

    if (!fs::exists(bgDir, ec) || !fs::is_directory(bgDir, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(bgDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        bool isVid = HasExtension(entry.path(), videoExts);
        bool isImg = HasExtension(entry.path(), imageExts);
        if (!isVid && !isImg) continue;

        std::string fullPath = entry.path().string();
        std::string stem     = entry.path().stem().string();

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
        auto img = slint::Image::load_from_path(slint::SharedString(entry.path));
        m_Main->set_live_preview_image(img);
        m_Main->set_has_live_preview_image(true);
        m_Projector->set_bg_image(img);
        m_Projector->set_has_bg_image(true);
    } else {
        m_HasStaticImage = false;
        m_HasVideoBg = true;
        m_Main->set_media_loop(true);
        m_Main->set_media_muted(true);
        m_Main->set_media_paused(false);
        m_Player.Play(entry.path, true, true);
    }
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
    m_Main->set_live_bg_solid_color(c);
    m_Main->set_live_bg_name(slint::SharedString(std::string("Color: ") + kSolids[i].name));
    m_Projector->set_has_bg_image(false);
    m_Projector->set_bg_solid_color(c);
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

void SlintApp::SendUrlToScreen(const std::string& url, bool loop, bool muted) {
    if (url.empty()) return;
    m_HasStaticImage = false;
    m_HasVideoBg     = true;
    m_CurrentBgPath  = url;
    m_Main->set_selected_capture_idx(-1);
    m_Main->set_live_bg_name(slint::SharedString("URL: " + url));
    m_Main->set_capture_status(slint::SharedString("Enviando URL a pantalla vía libmpv: " + url));
    m_Main->set_media_loop(loop);
    m_Main->set_media_muted(muted);
    m_Main->set_media_paused(false);
    m_Player.PlayLiveStream(url, loop, muted, false);
}

// ── Network Hub: LAN Stream, Mobile Server, Chat & OSC ───────────────────────
void SlintApp::ToggleLanStream() {
    if (m_LanServer.IsRunning()) {
        m_LanServer.Stop();
        m_Main->set_lan_running(false);
        m_Main->set_lan_url("Inactivo — pulsa Iniciar para transmitir en tu red local");
        m_Main->set_has_lan_qr(false);
    } else {
        ApplyLanConfig();
        if (m_LanServer.Start(8080)) {
            std::string url = m_LanServer.GetBaseURL();
            m_Main->set_lan_running(true);
            m_Main->set_lan_url(slint::SharedString(url + "   (Chat: " + url + "/chat)"));
            m_Main->set_lan_qr_image(GenerateQrImage(url));
            m_Main->set_has_lan_qr(true);
            RefreshChatLog();
        } else {
            m_Main->set_lan_url("Error al iniciar servidor en puerto 8080 (¿puerto ocupado?).");
        }
    }
}

void SlintApp::ApplyLanConfig() {
    ProyecThor::Core::StreamConfig cfg = m_LanServer.GetConfig();
    cfg.transparentMode = m_Main->get_lan_transparent();
    cfg.sendBackground  = m_Main->get_lan_send_bg();
    cfg.sendText        = true;
    m_LanServer.SetConfig(cfg);
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
        m_Main->set_chat_log("Chat activo en " + slint::SharedString(m_LanServer.GetBaseURL()) + "/chat — sin mensajes aún.");
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
