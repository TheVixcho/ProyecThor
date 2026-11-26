#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "main_window.h"
#include "SlintVideoPlayer.h"
#include "SlintMobileServer.h"
#include "MonitorInfo.h"
#include "NetworkStreamServer.h"
#include "FrameEncodeWorker.h"
#include "SmartTvCaster.h"
#include "ChatMessageStore.h"
#include "BibleTypes.h"
#include <map>

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
        bool        isCustomOverlay = false;
        int         customOverlayIdx = -1;
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
        int         textFitMode = 0; // 0=Normal, 1=Auto-Achicar, 2=Auto-Cortar en bloques
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

    struct CustomOverlayConfig {
        std::string id;
        std::string path;
        std::string name         = "Reloj en Vivo";
        bool        showClock    = true;
        int         clockMode    = 0;     // 0=Hora en Vivo, 1=En Retroceso (↓), 2=Hacia Arriba (↑)
        int         timeFormat   = 0;     // 0=HH:MM:SS, 1=HH:MM, 2=hh:mm AM/PM, 3=hh:mm:ss AM/PM, 4=MM:SS
        int         initialSec   = 300;   // Duración inicial para retroceso (seg)
        int         currentSec   = 300;   // Contador activo (seg)
        bool        timerRunning = false;

        std::string titleText;
        std::string subText;

        uint8_t     textR = 255, textG = 255, textB = 255;
        uint8_t     bgR   = 9,   bgG   = 9,   bgB   = 11;
        float       bgOpacity    = 0.82f;
        bool        showBorder   = true;
        uint8_t     borderR = 56, borderG = 189, borderB = 248;
        float       fontSize     = 42.0f;
        float       borderRadius = 12.0f;
        float       boxPadding   = 16.0f;
        float       minBoxWidth  = 0.0f;
        int         posPreset    = 3;     // 0=Centro, 1=Sup-Izq, 2=Sup-Cen, 3=Sup-Der, 4=Inf-Izq, 5=Inf-Cen, 6=Inf-Der, 7=Custom
        float       posX         = 0.85f;
        float       posY         = 0.10f;
    };

    struct OverlayQueueEntry {
        std::string title;
        std::string path;
        bool        isCustom = false;
    };

    struct ScreenOutputState {
        int         id = 0; // 0=Público, 1=LAN, 2=Stage, >=3=Sala Extra
        std::string name;
        std::string roleBadge;
        bool        isCustomRoom = false;

        std::string rawText;
        std::string rawRef;
        std::string displayText;
        std::string displayRef;
        bool        isBible = false;

        std::string  bgName = "Sin fondo";
        std::string  bgPath;
        bool         hasBgImage = false;
        bool         bgIsStatic = false;
        bool         bgIsVideo  = false;
        slint::Image bgImage;
        slint::Image bgThumb;
        bool         hasBgThumb = false;
        slint::Color bgSolidColor = slint::Color::from_rgb_uint8(3, 3, 4);
        float        bgSolidRgb[3] = { 0.0f, 0.0f, 0.0f };

        std::string  overlayName = "Sin overlay";
        std::string  overlayPath;
        bool         hasOverlayImage = false;
        slint::Image overlayImage;
        slint::Image overlayThumb;
        bool         hasOverlayThumb = false;

        bool                hasCustomOverlay = false;
        CustomOverlayConfig customOverlay;
        std::string         alertText;

        SavedTextStyle textStyle;
        int            assignedStyleIdx = -1; // -1 = personalizado/predeterminado, >=0 = índice en m_SavedStyles
        bool           useOwnStyle      = false; // true = estilo independiente (no sigue ni afecta a Público)
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
    void ApplyTargetFps(int fps);
    void ApplyGpuAndUpscalerConfig();
    void ApplyLowSpecPreset();
    void PurgeMemoryCaches();
    void DeliverVideoFrameToOutputs();
    void DetectGpuCapabilitiesAsync();
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
    void SelectStyleTarget(int targetMode);
    void SetScreenOwnStyle(int screenIdx, bool enabled);
    void ApplyCustomStyleToScreen(int styleIdx, int screenIdx);
    void ApplyStyleToScreen(int screenIdx, const SavedTextStyle& st, int styleIdx);
    void ApplyStyleToProjector(const SavedTextStyle& st);
    void SyncLanStyleProperties();
    void SetScreenLiveText(ScreenOutputState& sc, const std::string& rawText, const std::string& rawRef, bool isBible);
    std::string ResolveStyleFontName(const SavedTextStyle& st, bool indexBox = false) const;
    std::string ResolveFontFilePath(const std::string& family) const;
    std::string GetStyleTargetShortLabel(int targetMode) const;
    void RefreshSavedStylesModel();
    void LoadThemeFilesFromDisk();
    void SaveThemeFileToDisk(const SavedTextStyle& st);
    SavedTextStyle CaptureCurrentStyleFromUI(const std::string& name) const;
    void ApplyStyleStructToUI(const SavedTextStyle& st);

    // Bible Fullscreen helpers
    void RefreshBibleBooks(const std::string& bookFilter = "");
    void SelectBibleBook(int filteredBookIdx);
    void OnBibleSearchEdited(const std::string& query);
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
    void ExtractMusicWaveformAsync(const std::string& audioPath);
    void UpdateMusicWaveformAndDynamicEq();
    void ApplyMusicEqualizer(bool force = true);
    void ApplyMusicEqPreset(int presetIdx);

    // Library Hub (Tab 13 - main unified landing view)
    void RefreshLibraryHub();

    // Multi-Screen / Multi-Room Matrix & Photoshop-Style Real Active Layers
    void InitMultiScreens();
    bool IsScreenTargeted(int screenIdx) const;
    std::string GetActiveClockOverlayString() const;
    void SyncClockOverlay();
    void SyncScreenOutputs();
    void SyncLanFromScreenState();
    void SyncInspectedScreenToPreview();
    void RefreshActiveLayersModel();
    void RefreshScreenMatrixModel();
    void RemoveActiveLayer(const std::string& layerId);
    void RemoveLayerOnScreen(int screenIdx, const std::string& layerId);
    void ToggleLayerOnScreen(int screenIdx, const std::string& layerId);
    void SendCurrentSelectionToScreen(int screenIdx);
    void ClonePublicToScreen(int screenIdx);
    void ClearAllOnScreen(int screenIdx);
    void CreateExtraScreen(const std::string& name);
    void DeleteExtraScreen(int screenIdx);

    // Documents (Tab 10 - PDF/Image pages)
    void ScanDocuments();
    void OpenDocumentAsync(const std::string& path, bool autoProjectFirst = false);
    void ProjectDocPage(int pageIdx);

    // Unified File / Library Item Management (Play, Queue, Rename, Delete)
    void HandleItemPlay(int kind, const std::string& path);
    void HandleItemQueue(int kind, const std::string& path);
    void HandleItemRename(int kind, const std::string& path, const std::string& newName);
    void HandleItemDelete(int kind, const std::string& path);
    void RefreshAllLibraryViewsAfterMutation(int kind, const std::string& selectPathAfter = "");

    // Changelog, Debug & Firewall diagnostics
    void LoadChangelogText();
    void AppendDebugLog(const std::string& msg);
    void SampleAppTelemetry();
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
    void LoadCustomOverlaysFromDisk();
    bool SaveCustomOverlayToDisk(CustomOverlayConfig& cfg);
    void OpenNewOverlayEditor(bool withClock = true);
    void OpenEditOverlayEditor(int layerIndex);
    void OpenEditActiveScreenOverlay();
    void SyncOverlayEditorUiFromState();
    void SyncOverlayEditorStateFromUi();
    std::string FormatOverlayClockText(const CustomOverlayConfig& cfg) const;
    void ApplyOverlayConfigToTargets(const CustomOverlayConfig& cfg, int explicitTargetMode = -1);
    void SyncOverlayTimerToAll(const CustomOverlayConfig& src);
    void SaveOverlayEditor(bool alsoApply);
    void CancelOverlayEditor();
    void OverlayEditorSetMinutes(int mins);
    void OverlayEditorSetManualTime(const std::string& minStr, const std::string& secStr);
    void OverlayEditorTimerToggle();
    void OverlayEditorTimerReset();
    void SetOverlayEditorTextColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void SetOverlayEditorBgColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void SetOverlayEditorBorderColorRGB(uint8_t r, uint8_t g, uint8_t b);
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

    // Monitores físicos detectados y pantalla elegida para la proyección.
    std::vector<MonitorInfo> m_Monitors;
    int                      m_ProjectionMonitorIdx = 0;
    void RefreshMonitorList();
    void PositionProjectorOnSelectedMonitor();
    int  m_BgScaleMode      = 0;
    int  m_OutputResPreset  = 1; // 0=720p, 1=1080p, 2=16:10, 3=4:3, 4=2160p
    int  m_TargetFps        = 60; // 30, 60, 120
    int  m_GpuHwdecMode     = 0;  // 0=Auto, 1=NVIDIA(NVDEC/CUDA), 2=D3D11VA, 3=VAAPI/Vulkan, 4=CPU
    bool m_NvencEnabled     = false;
    bool m_NvencAvailable   = false;
    std::string m_DetectedGpuName = "Detectando GPU...";
    int   m_UpscalerMode      = 2;     // 0=Off(Bilinear), 1=FSR 1.0, 2=FidelityFX CAS+, 3=NVIDIA NIS, 4=LanczosHQ 4K
    float m_UpscalerSharpness = 0.55f; // 0.0 .. 1.0
    bool  m_SmoothMotion60    = false; // Real-time 24/30 -> 60 FPS motion interpolation
    bool  m_EnableGlassTranslucency = true;
    bool  m_EnableGlassSheen        = true;
    bool  m_EnableAmbientBg         = true;
    uint64_t m_DeliveredVideoSeq = 0;
    std::shared_ptr<std::atomic<bool>> m_AppAliveFlag;
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
    std::shared_ptr<slint::VectorModel<ActiveLayerItem>>     m_ActiveLayersModel;
    std::shared_ptr<slint::VectorModel<ScreenMatrixItem>>    m_ScreenMatrixModel;
    std::shared_ptr<slint::VectorModel<float>>               m_MusicLiveWaveModel;
    std::shared_ptr<slint::VectorModel<float>>               m_MusicTrackWaveModel;

    std::mutex                             m_SpectrumMutex;
    std::vector<std::array<float, 12>>     m_TrackSpectrumFrames; // Real 20Hz (50ms) 12-band FFT spectrum of current song
    float                                  m_TrackSpectrumPeak = 0.08f;
    float                                  m_LiveWaveSmoothed[12] = {};
    std::vector<float>                     m_MusicTrackEnvelope;
    float                                  m_MusicTrackAvgEnergy = 0.5f;
    bool                                   m_WaveIdleSettled     = true;
    float                                  m_DynEqOffsets[5]     = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float                                  m_DynEqApplied[5]     = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int                                    m_DynEqTickCounter    = 0;
    std::atomic<uint64_t>                  m_WaveExtractGen{0};

    std::vector<ScreenOutputState> m_Screens;
    int                            m_OutputTargetMode   = 0; // 0=Todas, 1=Solo Público, 2=Solo LAN, 3=Solo Stage, >=4=Sala
    int                            m_InspectedScreenIdx = 0; // 0=Público, 1=LAN, 2=Stage, >=3=Sala
    int                            m_StyleTargetMode    = 0; // 0=Todas, 1=Público, 2=LAN, 3=Stage, >=4=Sala

    std::vector<std::string>       m_AllSystemFonts;
    std::unordered_map<std::string, std::string> m_SystemFontFiles;
    std::string                    m_DefaultSansFont;
    std::string                    m_DefaultSerifFont;
    std::string                    m_DefaultMonoFont;

    std::vector<SongPlaylist>      m_SongPlaylists;
    int                            m_ActiveSongPlaylistIdx = -1;
    bool                           m_PlaylistFilterActive  = false;
    std::vector<OverlayQueueEntry> m_OverlayQueue;
    std::string                    m_ActiveOverlayPath;

    std::vector<MediaEntry>          m_BgDockEntries;
    std::vector<MediaEntry>          m_OverlayLayerEntries;
    std::vector<CustomOverlayConfig> m_CustomOverlays;
    CustomOverlayConfig              m_EditingOverlay;
    int                              m_EditingOverlayIdx  = -1;
    bool                             m_OverlayEditorOpen  = false;
    std::vector<SavedTextStyle>      m_SavedStyles;
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
    std::map<std::string, int>     m_SearchMatchedStanzas;
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
    std::string                                            m_SnapFontFamily   = "Predeterminada";
    int                                                    m_SnapFontWeight   = 700;
    bool                                                   m_SnapTextShadow   = true;
    bool                                                   m_SnapTextOutline  = false;
    bool                                                   m_SnapTextGlow     = false;
    bool                                                   m_SnapTextNeon     = false;
    bool                                                   m_SnapText3d       = false;
    bool                                                   m_SnapTextUnderline = false;
    bool                                                   m_SnapTextBoxBg    = false;
    std::string                                            m_LanFontCacheName;
    std::string                                            m_LanFontCachePath;
    uint64_t                                               m_LanFontVersion   = 1;
    int                                                    m_SnapTransType    = 1;
    float                                                  m_SnapTransDur     = 0.35f;
    std::mutex                                             m_LanFrameMutex;
    std::vector<uint8_t>                                   m_LanLatestJpeg;
    std::atomic<bool>                                      m_LanHasFrame{false};
    int64_t                                                m_LanLastCaptureMs = 0;
    ProyecThor::Core::OverlaySnapshot                      m_SnapOverlay;
    std::string                                            m_SnapOverlayPngPath;
    uint64_t                                               m_LanOverlayVersion = 1;
    uint64_t                                               m_LastChatMsgId = 0;
    int                                                    m_ActiveThemeIdx = 0;

    // Performance throttle & alert animation state
    int     m_TickCounter          = 0;
    bool    m_HasActivePublicAlert = false;

    // Live Telemetry (FPS, CPU %, RAM MB, Threads)
    std::chrono::steady_clock::time_point m_PerfLastSampleTime{};
    int      m_PerfTickFrames     = 0;
    int      m_PerfVideoFrames    = 0;
    uint64_t m_PerfLastCpuNs      = 0;
    double   m_PerfFps            = 0.0;
    double   m_PerfVideoFps       = 0.0;
    double   m_PerfCpuPercent     = 0.0;
    double   m_PerfRamMb          = 0.0;
    double   m_PerfPeakRamMb      = 0.0;
    int      m_PerfThreads        = 0;
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
