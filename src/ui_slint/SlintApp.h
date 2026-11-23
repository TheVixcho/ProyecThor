#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "main_window.h"
#include "SlintVideoPlayer.h"
#include "SlintMobileServer.h"
#include "NetworkStreamServer.h"
#include "FrameEncodeWorker.h"
#include "SmartTvCaster.h"
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
        std::string author;
        std::string path;
        bool        isVideo    = false;
        bool        isAudio    = false;
        bool        isImage    = false;
        bool        isOverlay  = false;
        bool        isDocument = false;
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
        float       lyricsHeight = 0.55f;
        bool        indexEnabled = true;
        float       indexPosX   = 0.5f;
        float       indexPosY   = 0.86f;
        float       indexWidth  = 0.50f;
        float       indexHeight = 0.14f;
        float       indexSize   = 30.0f;
        int         indexAlign  = 1;
        int         indexFontFamIdx = 0;
        std::string indexFontName;
        int         indexFontWtIdx  = 1;
        uint8_t     indexColorR = 251;
        uint8_t     indexColorG = 191;
        uint8_t     indexColorB = 36;
        bool        indexShadow = true;
        bool        indexOutline = false;
        bool        indexGlow   = false;
        bool        indexBoxBg  = true;
        bool        indexUppercase = false;
        int         textAlign   = 1;
        int         textValign  = 1;
        uint8_t     colorR      = 255;
        uint8_t     colorG      = 255;
        uint8_t     colorB      = 255;
        int         fontFamIdx  = 0;
        std::string fontName;
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

    struct VideoQueueEntry {
        std::string title;
        std::string path;
        std::string kind;
        bool        isImage = false;
    };

    struct SongPlaylist {
        std::string              name;
        std::vector<std::string> songPaths;
    };

    struct OverlayQueueEntry {
        std::string title;
        std::string path;
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
    void ScanSystemFonts();
    void RefreshFontPickerModel(const std::string& filter);
    std::string ResolveFontPreset(int presetIdx) const;
    void SyncStageNextSlide();
    void ToggleAudienceWindow();
    void ToggleStageWindow();
    void ToggleFreeze();
    void ResyncProjectorFromLive();
    void ApplyScalingMode(int mode);
    void ApplyOutputResolution(int preset);
    void GetOutputResolution(int& outW, int& outH) const;
    bool RenderScaledStaticImage(const std::string& path, slint::Image& outImg);
    void ReapplyCurrentStaticImage();
    void UpdatePlayerRenderSize();
    void ScanAllLibrariesManual();
    bool ExtractMusicCoverArt(const std::string& audioPath, slint::Image& outImg);
    void CreateBgSubfolder(const std::string& folderName);
    void RebuildSlideGridRows();
    void SelectTextColor(int colorIdx);
    void SetTextColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void SelectIndexColor(int colorIdx);
    void SetIndexColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void SyncAlertConfig();
    void OpenNewStyleEditor();
    void OpenEditStyleEditor(int idx);
    void SaveStyleEditor(const std::string& name);
    void CancelStyleEditor();
    void DuplicateCustomStyle(int idx);
    void LoadCustomStyle(int idx);
    void DeleteCustomStyle(int idx);
    void RefreshSavedStylesModel();
    void LoadThemeFilesFromDisk();
    void SaveThemeFileToDisk(const SavedTextStyle& st);
    SavedTextStyle CaptureCurrentStyleFromUI(const std::string& name) const;
    void ApplyStyleStructToUI(const SavedTextStyle& st);

    // Bible Fullscreen helpers
    void RefreshBibleBooks();
    void SelectBibleBook(int filteredBookIdx);
    void SearchBible(const std::string& query);
    void AddCurrentVerseToFavorites();
    void PushBibleHistory(const std::string& badge, const std::string& body, const std::string& ref);

    // Media Preview & Video Queue (MonitorQueueEngine parity)
    void ProjectSelectedMedia();
    void AddSelectedToVideoQueue();
    void ToggleVideoQueuePlay();
    void PrevVideoQueueItem();
    void NextVideoQueueItem();
    void PlayVideoQueueIndex(int idx);
    void MoveVideoQueueItem(int from, int to);
    void RemoveVideoQueueItem(int idx);
    void ClearVideoQueue();
    void RefreshVideoQueueModel();
    void ApplyShaders();
    void ResetShaders();
    void ApplyEqualizer();
    void ApplyEqPreset(int presetIdx);
    void TriggerSlideTransition();
    void SelectTransitionPreset(int presetIdx);
    void ReplayTransitionPreview();
    void SyncTilesState();
    static std::string ToUpperUtf8(const std::string& s);

    // Song Editor & Playlists
    void OpenNewSongEditor();
    void OpenEditSongEditor();
    void SaveEditedSong(const std::string& title, const std::string& author, const std::string& body);
    static void ReadSongMeta(const std::string& songPath, std::string& outTitle, std::string& outAuthor);
    static void WriteSongMeta(const std::string& songPath, const std::string& title, const std::string& author);

    // Music Player (Tab 9 - independent audio grid + user scan folders)
    void ScanMusicTracks();
    void PlayMusicTrack(int idx);
    void RefreshMusicModel();
    void RebuildMusicGridRows();
    void BrowseMusicFolderAsync();
    void AddMusicScanFolder(const std::string& folderPath);
    void RemoveMusicScanFolder(int idx);
    void RefreshMusicScanFoldersModel();

    // Library Hub (Tab 13 - main unified landing view)
    void RefreshLibraryHub();

    // Documents (Tab 10 - PDF/Image pages)
    void ScanDocuments();
    void OpenDocumentAsync(const std::string& path);
    void ProjectDocPage(int pageIdx);

    // Changelog, Debug & Firewall diagnostics
    void LoadChangelogText();
    void AppendDebugLog(const std::string& msg);
    void RefreshDebugInfo();
    void CheckFirewallStatusAsync();
    void OpenFirewallPortsAsync();
    void LoadSongPlaylists();
    void SaveSongPlaylists();
    void CreateSongPlaylist(const std::string& name);
    void SelectSongPlaylist(int idx);
    void TogglePlaylistFilter();
    void AddSelectedSongToPlaylist();
    void SendActivePlaylistToQueue();
    void DeleteSongPlaylist(int idx);
    void RefreshSongPlaylistsModel();

    // Service Queue (Letras, Videos, Overlays)
    void AddCurrentToQueue();
    void ActivateQueueItem(int index);
    void RefreshQueueModel();
    void AddSelectedOverlayToQueue();
    void ToggleOverlayQueueItem(int index);
    void RefreshOverlayQueueModel();

    // Hover Video Preview (silent low-res libmpv instance)
    void StartHoverVideoPreview(const std::string& path);
    void StopHoverVideoPreview();

    // Pads
    void SaveToPad(int index);
    void TriggerPad(int index);
    void RefreshPadsModel();

    // Themes (14 ImGui presets), Import, Capture/URL & Network Hub
    void ApplyThemePreset(int themeIdx);
    void ImportFileAsync(bool forceBackgroundFolder = false, bool forceOverlayFolder = false, int explicitTargetTab = -1);
    void RefreshCaptureDevices();
    void StartCaptureDevice(int index);
    void RefreshBgDock();
    void SelectDockBackground(int index);
    void RefreshOverlayLayers();
    void SelectOverlayLayer(int index);
    void AddOverlayLayerToQueue(int index);
    void SelectSolidBgColor(int colorIdx);
    void LaunchLegacyImGui();
    bool LoadOrCreateThumbnail(const std::string& path, bool isVideo, slint::Image& outImg);
    void WireLanServerProviders(ProyecThor::Core::NetworkStreamServer& srv);
    void ToggleLanStream();
    void ApplyLanConfig();
    void UpdateLanFrameCapture();
    void UpdateLanStaticFrame(const slint::Image& img);
    void ClearLanFrame();
    void CopyToClipboard(const std::string& text);
    bool IsLanRunning() const { return m_LanServer && m_LanServer->IsRunning(); }
    void ScanSmartTvsAsync();
    void CastToSmartTv(int index);
    void CastToSmartTvIp(const std::string& ip);
    void StopSmartTvCast(int index);
    void RefreshSmartTvModel();
    void ToggleChatServer();
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
    bool m_AudienceFrozen   = false;
    int  m_BgScaleMode      = 0;
    int  m_OutputResPreset  = 1; // 0=1280x720, 1=1920x1080, 2=1440x900 (16:10), 3=1280x960 (4:3)
    std::string          m_StaticSrcPath;
    std::vector<uint8_t> m_StaticSrcRgba;
    int                  m_StaticSrcW = 0;
    int                  m_StaticSrcH = 0;
    std::string          m_BgCurrentRelDir; // relative path inside assets/backgrounds/

    std::shared_ptr<slint::VectorModel<LibraryItem>>   m_LibraryModel;
    std::shared_ptr<slint::VectorModel<SlideItem>>     m_SlidesModel;
    std::shared_ptr<slint::VectorModel<SlideGridRow>>  m_SlideRowsModel;
    std::shared_ptr<slint::VectorModel<MusicTrackItem>> m_MusicTracksModel;
    std::shared_ptr<slint::VectorModel<MusicGridRow>>   m_MusicGridRowsModel;
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_MusicScanFoldersModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>    m_HubSongsModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>    m_HubMediaModel;
    std::shared_ptr<slint::VectorModel<MusicTrackItem>> m_HubMusicModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>    m_HubDocsModel;
    std::shared_ptr<slint::VectorModel<DocPageItem>>   m_DocPagesModel;
    std::shared_ptr<slint::VectorModel<QueueItem>>         m_QueueModel;
    std::shared_ptr<slint::VectorModel<PadItem>>           m_PadsModel;
    std::shared_ptr<slint::VectorModel<CaptureDeviceItem>>   m_CaptureModel;
    std::shared_ptr<slint::VectorModel<SmartTvItem>>         m_SmartTvModel;
    std::shared_ptr<slint::VectorModel<StyleCardItem>>       m_SavedStyleCardsModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>         m_BgDockModel;
    std::shared_ptr<slint::VectorModel<BgFolderItem>>        m_BgFoldersModel;
    std::shared_ptr<slint::VectorModel<LibraryItem>>         m_OverlayLayersModel;
    std::shared_ptr<slint::VectorModel<VideoQueueItem>>      m_VideoQueueModel;
    std::shared_ptr<slint::VectorModel<BibleBookItem>>       m_BibleBooksModel;
    std::shared_ptr<slint::VectorModel<int>>                 m_BibleChaptersModel;
    std::shared_ptr<slint::VectorModel<SlideItem>>           m_BibleHistoryModel;
    std::shared_ptr<slint::VectorModel<SlideItem>>           m_BibleFavoritesModel;
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_BibleVersionNamesModel;
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_SongPlaylistNamesModel;
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_SystemFontsModel;
    std::shared_ptr<slint::VectorModel<QueueItem>>           m_OverlayQueueModel;

    std::vector<std::string>       m_AllSystemFonts;
    std::string                    m_DefaultSansFont;
    std::string                    m_DefaultSerifFont;
    std::string                    m_DefaultMonoFont;

    std::vector<SongPlaylist>      m_SongPlaylists;
    int                            m_ActiveSongPlaylistIdx = -1;
    bool                           m_PlaylistFilterActive  = false;
    std::vector<OverlayQueueEntry> m_OverlayQueue;
    std::string                    m_ActiveOverlayPath;

    std::vector<MediaEntry>      m_BgDockEntries;
    std::vector<MediaEntry>      m_OverlayLayerEntries;
    std::vector<SavedTextStyle>  m_SavedStyles;
    SavedTextStyle               m_StyleBeforeEdit;
    int                          m_EditingStyleIdx = -1; // -1 = new style

    // Media preview selection & Video Queue
    MediaEntry                   m_SelectedPreviewEntry;
    bool                         m_HasSelectedPreviewEntry = false;
    std::vector<VideoQueueEntry> m_VideoQueue;
    int                          m_VideoQueueCurrentIdx = -1;
    bool                         m_VideoQueuePlaying    = false;

    // Bible fullscreen filtered books mapping
    std::vector<int>             m_BibleFilteredBookIndices;
    std::string                  m_RawLiveText;
    std::string                  m_RawLiveRef;
    bool                         m_LiveIsBible = false;
    float                        m_AlertTickerPos  = 0.0f;
    float                        m_AlertPulsePhase = 0.0f;

    struct CaptureDev {
        std::string name;
        std::string uri;
    };
    std::vector<CaptureDev> m_CaptureDevices;

    std::vector<ProyecThor::Core::SmartTvDevice> m_SmartTvDevices;
    int                                          m_ActiveSmartTvIdx = -1;

    int         m_ActiveTab = 13; // Opens in Biblioteca Hub
    int         m_BgSubtab  = 0;
    std::string m_SearchQuery;
    std::string m_EditingSongPath;

    std::vector<MediaEntry>        m_AllEntries;
    std::unordered_map<int, std::vector<MediaEntry>> m_CachedTabEntries;
    std::vector<int>               m_FilteredIndices;
    std::vector<ServiceQueueEntry> m_Queue;
    SavedPad                       m_Pads[8];

    // Independent Music player (Tab 9)
    SlintVideoPlayer         m_MusicPlayer;
    std::vector<MediaEntry>  m_MusicEntries;
    std::vector<std::string> m_MusicScanFolders;
    int                      m_CurrentMusicIdx = -1;
    bool                     m_MusicScanned    = false;

    // Documents (Tab 10)
    std::vector<std::string> m_DocPagePaths;
    int                      m_ActiveDocPageIdx = -1;

    // Debug log
    std::vector<std::string> m_DebugLogLines;

    // Bible state
    std::vector<std::string>  m_BiblePaths;
    int                       m_CurrentBibleVersionIdx = 0;
    ProyecThor::UI::BibleData m_Bible;
    int                       m_CurrentBibleBookIdx = -1;
    int                       m_CurrentBibleChapter = 1;

    // Video / Image playback + Hover preview player
    SlintVideoPlayer m_Player;
    SlintVideoPlayer m_HoverPlayer;
    bool             m_HoverVideoActive = false;
    std::string      m_HoverVideoPath;
    std::string      m_CurrentBgPath;
    bool             m_HasStaticImage = false;
    bool             m_HasVideoBg     = false;

    // Network Hub (ImGui StreamingPanel + FrameEncodeWorker parity; Chat LAN & Mobile independent)
    std::unique_ptr<ProyecThor::Core::NetworkStreamServer> m_LanServer;
    ProyecThor::Core::StreamConfig                         m_LanConfig;
    ProyecThor::Core::FrameEncodeWorker                    m_LanEncodeWorker;
    ProyecThor::Core::NetworkStreamServer                  m_ChatServer;
    ProyecThor::Core::ChatMessageStore                     m_ChatStore;
    SlintMobileServer                                      m_MobileServer;
    uint64_t                                               m_StreamVersion    = 1;
    uint64_t                                               m_SnapTransTrigger = 0;
    std::mutex                                             m_LiveSnapMutex;
    std::string                                            m_SnapText;
    std::string                                            m_SnapRef;
    bool                                                   m_SnapProjecting = false;
    float                                                  m_SnapTextSize   = 60.0f;
    int                                                    m_SnapTextAlign  = 1;
    int                                                    m_SnapTextValign = 1;
    float                                                  m_SnapTextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float                                                  m_SnapBgColor[3]   = { 0.0f, 0.0f, 0.0f };
    float                                                  m_SnapMargins[4]   = { 96.0f, 243.0f, 96.0f, 243.0f };
    int                                                    m_SnapTransType    = 1;
    float                                                  m_SnapTransDur     = 0.35f;
    std::mutex                                             m_LanFrameMutex;
    std::vector<uint8_t>                                   m_LanLatestJpeg;
    std::atomic<bool>                                      m_LanHasFrame{false};
    int64_t                                                m_LanLastCaptureMs = 0;
    uint64_t                                               m_LastChatMsgId = 0;
    int                                                    m_ActiveThemeIdx = 0;

    // Countdown timer state & performance throttle caches
    int     m_TimerInitialSec      = 300;
    int     m_TimerRemainingSec    = 300;
    bool    m_TimerRunning         = false;
    int     m_TickCounter          = 0;
    bool    m_HasActivePublicAlert = false;
    float   m_LastMediaPos         = -1.0f;
    int64_t m_LastMediaSec         = -1;
    int64_t m_LastMediaLenSec      = -1;
    bool    m_LastMediaPaused      = false;
    float   m_LastMusicPos         = -1.0f;
    int64_t m_LastMusicSec         = -1;
    int64_t m_LastMusicLenSec      = -1;
    bool    m_LastMusicPaused      = false;
    std::unordered_map<std::string, slint::Image> m_ThumbCache;

    // Splash state
    int    m_SplashStep        = 0;
    std::unique_ptr<slint::Timer> m_SplashTimer;
    std::unique_ptr<slint::Timer> m_TickTimer;
};

} // namespace ProyecThor::SlintUI
