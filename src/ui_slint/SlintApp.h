#pragma once

#include <memory>
#include <string>
#include <vector>

#include "main_window.h"
#include "SlintVideoPlayer.h"
#include "SlintMobileServer.h"
#include "NetworkStreamServer.h"
#include "ChatMessageStore.h"
#include "BibleTypes.h"

namespace ProyecThor::SlintUI {

class SlintApp {
public:
    SlintApp();
    ~SlintApp();

    int Run();

private:
    struct MediaEntry {
        std::string title;
        std::string subtitle;
        std::string path;
        bool        isVideo   = false;
        bool        isAudio   = false;
        bool        isImage   = false;
        bool        isOverlay = false;
    };

    struct ServiceQueueEntry {
        int         tab = 0; // 0=Song, 1=Bible, 2=Media, 3=Bg
        std::string title;
        std::string kind;
        std::string path;
        int         bibleBookIdx = -1;
        int         bibleChapter = 1;
    };

    struct SavedPad {
        bool        assigned = false;
        std::string name;
        std::string text;
        std::string ref;
        std::string bgPath;
        bool        bgIsImage = false;
        bool        bgIsVideo = false;
        float       textSize  = 60.0f;
        int         textAlign = 1;
        int         textValign = 1;
        int         textColorIdx = 0;
    };

    struct SavedTextStyle {
        std::string name;
        float       textSize    = 60.0f;
        float       marginH     = 64.0f;
        float       lyricsPosX  = 0.5f;
        float       lyricsPosY  = 0.5f;
        float       lyricsWidth = 0.90f;
        bool        indexEnabled = false;
        float       indexPosX   = 0.5f;
        float       indexPosY   = 0.86f;
        float       indexSize   = 30.0f;
        int         textAlign   = 1;
        int         textValign  = 1;
        int         colorIdx    = 0;
        int         fontFamIdx  = 0;
        int         fontWtIdx   = 1;
        bool        shadow      = true;
        bool        outline     = false;
        bool        glow        = false;
        bool        neon        = false;
        bool        text3d      = false;
        bool        underline   = false;
        bool        uppercase   = false;
        bool        boxBg       = false;
    };

    void InitAssetsAndSplash();
    void FinishSplash();
    void BindCallbacks();
    void LoadTabData(int tab);
    void RefreshLibraryModel();
    void SelectLibraryItem(int index);
    void ProjectSlide(int index);
    void ClearText();
    void ClearBackground();
    void ClearOverlay();
    void ClearAll();

    void SyncProjectorStyle();
    void SyncStageNextSlide();
    void ToggleAudienceWindow();
    void ToggleStageWindow();
    void ApplyStylePreset(int presetIdx);
    void SelectTextColor(int colorIdx);
    void SaveCustomStyle(const std::string& name);
    void LoadCustomStyle(int idx);
    void RefreshSavedStylesModel();
    void ApplyShaders();
    void ResetShaders();
    void ApplyEqualizer();
    void ApplyEqPreset(int presetIdx);
    void TriggerSlideTransition();
    void SyncTilesState();
    static std::string ToUpperUtf8(const std::string& s);

    // Song Editor
    void OpenNewSongEditor();
    void OpenEditSongEditor();
    void SaveEditedSong(const std::string& title, const std::string& body);

    // Queue
    void AddCurrentToQueue();
    void ActivateQueueItem(int index);
    void RefreshQueueModel();

    // Pads
    void SaveToPad(int index);
    void TriggerPad(int index);
    void RefreshPadsModel();

    // Themes (14 ImGui presets), Import, Capture/URL & Network Hub
    void ApplyThemePreset(int themeIdx);
    void ImportFileAsync(bool forceBackgroundFolder = false);
    void RefreshCaptureDevices();
    void StartCaptureDevice(int index);
    void SendUrlToScreen(const std::string& url, bool loop, bool muted);
    void RefreshBgDock();
    void SelectDockBackground(int index);
    void SelectSolidBgColor(int colorIdx);
    void LaunchLegacyImGui();
    static bool LoadOrCreateThumbnail(const std::string& path, bool isVideo, slint::Image& outImg);
    void ToggleLanStream();
    void ApplyLanConfig();
    void ToggleMobileServer();
    void RegenerateMobilePin();
    void RefreshChatLog();
    void SendOscCommand(const std::string& host, const std::string& portStr,
                        const std::string& address, const std::string& args);
    static std::string GenerateRandomPin6();

    // Persistence
    void LoadPersistedState();
    void SavePersistedState();

    // Updater & Tick
    void CheckGitHubUpdatesAsync();
    void OnTick();

    // Library loaders
    void ScanSongs();
    void ScanBibles();
    void LoadSelectedBible(int versionIdx);
    void LoadBibleChapter(int bookIdx, int chapterOneBased);
    void ScanMedia(bool backgroundsOnly);
    void ScanOverlays();

    static std::vector<std::string> ParseSongStanzas(const std::string& path);
    static std::string FormatTimeMs(int64_t ms);
    static std::string FormatCountdownSec(int totalSec);
    static std::string ToLowerUtf8(const std::string& s);
    static slint::Color TextColorFromIndex(int idx);

    slint::ComponentHandle<SplashWindow>    m_Splash;
    slint::ComponentHandle<MainWindow>      m_Main;
    slint::ComponentHandle<ProjectorWindow> m_Projector;
    slint::ComponentHandle<StageWindow>     m_Stage;

    bool m_ProjectorVisible = false;
    bool m_StageVisible     = false;

    std::shared_ptr<slint::VectorModel<LibraryItem>> m_LibraryModel;
    std::shared_ptr<slint::VectorModel<SlideItem>>   m_SlidesModel;
    std::shared_ptr<slint::VectorModel<QueueItem>>         m_QueueModel;
    std::shared_ptr<slint::VectorModel<PadItem>>           m_PadsModel;
    std::shared_ptr<slint::VectorModel<CaptureDeviceItem>>   m_CaptureModel;
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_SavedStylesModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>         m_BgDockModel;

    std::vector<MediaEntry>     m_BgDockEntries;
    std::vector<SavedTextStyle> m_SavedStyles;
    std::string                 m_RawLiveText;

    struct CaptureDev {
        std::string name;
        std::string uri;
    };
    std::vector<CaptureDev> m_CaptureDevices;

    int         m_ActiveTab = 0;
    int         m_BgSubtab  = 0;
    std::string m_SearchQuery;
    std::string m_EditingSongPath;

    std::vector<MediaEntry>        m_AllEntries;
    std::vector<int>               m_FilteredIndices;
    std::vector<ServiceQueueEntry> m_Queue;
    SavedPad                       m_Pads[8];

    // Bible state
    std::vector<std::string>  m_BiblePaths;
    int                       m_CurrentBibleVersionIdx = 0;
    ProyecThor::UI::BibleData m_Bible;
    int                       m_CurrentBibleBookIdx = -1;
    int                       m_CurrentBibleChapter = 1;

    // Video / Image playback
    SlintVideoPlayer m_Player;
    std::string      m_CurrentBgPath;
    bool             m_HasStaticImage = false;
    bool             m_HasVideoBg     = false;

    // Network Hub (LAN Stream, Mobile Server, Chat)
    ProyecThor::Core::NetworkStreamServer m_LanServer;
    ProyecThor::Core::ChatMessageStore    m_ChatStore;
    SlintMobileServer                     m_MobileServer;
    uint64_t                              m_StreamVersion = 1;
    std::mutex                            m_LiveSnapMutex;
    std::string                           m_SnapText;
    std::string                           m_SnapRef;
    bool                                  m_SnapProjecting = false;
    float                                 m_SnapTextSize   = 60.0f;
    int                                   m_SnapTextAlign  = 1;
    int                                   m_SnapTextValign = 1;
    uint64_t                              m_LastChatMsgId  = 0;
    int                                   m_ActiveThemeIdx = 0;

    // Countdown timer state
    int    m_TimerInitialSec   = 300;
    int    m_TimerRemainingSec = 300;
    bool   m_TimerRunning      = false;
    int    m_TickCounter       = 0;

    // Splash state
    int    m_SplashStep        = 0;
    std::unique_ptr<slint::Timer> m_SplashTimer;
    std::unique_ptr<slint::Timer> m_TickTimer;
};

} // namespace ProyecThor::SlintUI
