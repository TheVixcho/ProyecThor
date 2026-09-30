#pragma once
#ifndef PROYECTHOR_PRESENTATION_CORE_H
#define PROYECTHOR_PRESENTATION_CORE_H
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <imgui.h>

#include "NetworkStreamServer.h"
#include "ChatMessageStore.h"
#include "ui/windowing/SecondaryOutputWindow.h"
#include "ui/panels/overlay/OverlayTypes.h"

struct GLFWwindow;

namespace ProyecThor::UI {
    class AudioPanel;
    class Announcements;
    class OClock;
    class CapturePanel;
    class LabPanel;
}

namespace ProyecThor::Core {

    class VLCBasePlayer;

    enum class ItemType { None = -1, Video = 0, Image = 1, Song = 2, Bible = 3, Documents = 4, Audio = 5 };

    enum class OutputContentMode { Live = 0, ClockOnly = 1, Blank = 2 };

    struct LibrarySelection {
        std::string title;
        ItemType type = ItemType::None;
        std::vector<std::string> contentData;
    };

    struct TextEffectsData {
        bool  bgEnabled = false;
        float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.55f };

        bool  borderEnabled = false;
        float borderColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float borderWidth = 0.4f;

        bool  shadowEnabled = true;
        float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
        float shadowIntensity = 0.4f;

        bool  chromaticAberrationEnabled = false;
        float chromaticAberrationIntensity = 0.4f;

        bool  glowEnabled = false;
        float glowColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float glowIntensity = 0.5f;

        bool  neonEnabled = false;
        float neonColor[4] = { 0.15f, 0.9f, 1.0f, 1.0f };
        float neonIntensity = 0.6f;

        bool  underlineEnabled = false;
        float underlineColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float underlineThickness = 0.3f;

        bool  text3dEnabled = false;
        float text3dColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float text3dDepth = 0.4f;

        bool  gradientEnabled = false;
        float gradientColorA[4] = { 1.0f, 0.35f, 0.35f, 1.0f };
        float gradientColorB[4] = { 0.35f, 0.45f, 1.0f, 1.0f };
        float gradientAngle = 0.0f;

        bool  opacityGradientEnabled = false;
        float opacityGradientAngle = 0.0f;
        float opacityGradientStrength = 0.5f;
    };

    struct TextBoxStyle {
        float       posX          = 0.5f;
        float       posY          = 0.5f;
        float       sizeW         = 0.92f;
        float       sizeH         = 0.89f;
        std::string fontName      = "Predeterminada";
        float       color[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        float       textSize      = 60.0f;
        int         hAlign        = 1;
        int         vAlign        = 1;
        bool        autoScale     = true;
        TextEffectsData effects;

        bool        bgMediaEnabled = false;
        std::string bgMediaPath;
        float       bgMediaOpacity = 1.0f;
    };

    struct SavedStyle {
        std::string name;
        float       size          = 60.0f;
        float       color[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int         hAlign        = 1;
        int         vAlign        = 1;
        float       margins[4]    = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool        autoScale     = true;
        std::string fontName      = "Predeterminada";
        TextEffectsData effects;

        TextBoxStyle lyrics;

        TextBoxStyle index;
        bool         indexEnabled = false;
    };

    std::string PackTextEffects(const TextEffectsData& e);
    void        UnpackTextEffects(const std::string& v, TextEffectsData& e);

    struct PresentationState {

        enum class BackgroundType { SolidColor, Video, Audio };
        BackgroundType bgType = BackgroundType::SolidColor;
        std::string bgPath;
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };

        bool isProjecting       = false;
        int  targetMonitorIndex = 0;
        bool windowFullscreen   = true;

        std::vector<int> extraTargetMonitors;

        bool isStaging          = false;
        int  stageMonitorIndex  = 0;

        std::vector<int> extraStageMonitors;

        std::string currentText;
        bool  showText          = false;

        std::string currentRef;

        std::string nextText;

        uint64_t transitionTrigger  = 0;

        uint64_t textTransitionTrigger = 0;

        int      transitionType     = 0;
        float    transitionDuration = 1.0f;

        float textSize          = 60.0f;
        float textColor[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int   textAlignment     = 1;
        int   vAlignment        = 1;
        float margins[4]        = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool  autoScale         = true;
        std::string selectedFont = "Predeterminada";
        TextEffectsData effects;

        float refTextSize   = 28.0f;
        float verseTextSize = 60.0f;

        int songTextAlignment  = 1;
        int songVAlignment     = 1;
        int bibleTextAlignment = 1;
        int bibleVAlignment    = 1;

        TextBoxStyle lyricsBox;

        TextBoxStyle indexBox;
        bool         indexEnabled = false;

        float livePosition      = 0.0f;
        int   liveVolume        = 100;
        bool  liveMuted         = false;
        bool  liveLoop          = false;

        std::string quickNoteText;
        bool showQuickNote = false;

        std::string lanQuickNoteText;
        bool        showLanQuickNote = false;

        bool        isStreamingNet = false;
        std::string networkURL;

        bool        isChatRunning = false;
        std::string chatURL;
    };

    class PresentationCoreImpl;

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }
void SetGlobalMute(bool mute);
    bool GetGlobalMute() const;
        PresentationCore();
        ~PresentationCore();

        PresentationCore(const PresentationCore&)            = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;

        void Update();

        void RenderBackground(int outputW, int outputH);
        void RenderProjectorWindow();
        PresentationState GetState();
        void ApplyStyleByName(const std::string& styleName);
        void ApplyStyleSnapshot(const SavedStyle& style);
        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;

        void               SetLanContentMode(OutputContentMode mode) { m_LanContentMode = mode; }
        OutputContentMode  GetLanContentMode() const { return m_LanContentMode.load(); }

        void  SetBackgroundPingPongLoop(bool enabled);
        bool  GetBackgroundPingPongLoop() const;

        void ClearQuickNote();

        void SetTransitionConfig(int type, float durationSeconds);
        void SetBackgroundTransitionProgress(float progress);

        void SetBackgroundBlendDuration(float seconds);

        void SetLiveQuickNote(const std::string& text, const float* colorOverride = nullptr);
        void SetLiveQuickNoteLAN(const std::string& text, const float* colorOverride = nullptr, const std::string& styleName = "");
        void ClearQuickNoteLAN();

        void PushRemoteClockTitle(const std::string& text);
        std::vector<std::string> DrainRemoteClockTitles();

        void*          GetBackgroundTexture();
        void*          GetProcessedBackgroundTexture(int targetW, int targetH);

        void*          GetPreviewBackgroundTexture(int targetW, int targetH);

        void*          GetBackgroundFillTexture(int workW, int workH);
        void           SetFillBlurEnabled(bool enabled);
        bool           GetFillBlurEnabled() const;
        void           SetFillBlurBrightness(float v);
        float          GetFillBlurBrightness() const;

        VLCBasePlayer* GetBackgroundPlayer();
        void           GetBackgroundVideoSize(int& width, int& height);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void  SetNISEnabled(bool enabled);
        bool  GetNISEnabled() const;
        void  SetNISSharpness(float sharpness);
        float GetNISSharpness() const;

        void SetVideoRenderEngine(int engine);
        int  GetVideoRenderEngine() const;

        void        SetVLCHardwareDecoder(const std::string& dec);
        std::string GetVLCHardwareDecoder() const;

        void        SetVLCVideoOutput(const std::string& vout);
        std::string GetVLCVideoOutput() const;

        void        SetVLCDeinterlace(const std::string& deint);
        std::string GetVLCDeinterlace() const;

        bool        IsActiveNativeVideo() const;

        void        SetWindowFullscreen(bool fullscreen);
        bool        GetWindowFullscreen() const;

        void        SetVLCNativeForFondos(bool enable);
        bool        GetVLCNativeForFondos() const;

        void        ReloadVLCPlayers();

        void  SetCRTEnabled(bool enabled);
        bool  GetCRTEnabled() const;
        void  SetCRTScanlineIntensity(float intensity);
        float GetCRTScanlineIntensity() const;

        void  SetGrainEnabled(bool enabled);
        bool  GetGrainEnabled() const;
        void  SetGrainIntensity(float intensity);
        float GetGrainIntensity() const;

        void  SetFXAAEnabled(bool enabled);
        bool  GetFXAAEnabled() const;

        void  SetSaturationEnabled(bool enabled);
        bool  GetSaturationEnabled() const;
        void  SetSaturationAmount(float amount);
        float GetSaturationAmount() const;

        void  SetVignetteEnabled(bool enabled);
        bool  GetVignetteEnabled() const;
        void  SetVignetteIntensity(float intensity);
        float GetVignetteIntensity() const;

        void  SetBlurEnabled(bool enabled);
        bool  GetBlurEnabled() const;
        void  SetBlurIntensity(float intensity);
        float GetBlurIntensity() const;

        void  SetSharpenEnabled(bool enabled);
        bool  GetSharpenEnabled() const;
        void  SetSharpenIntensity(float intensity);
        float GetSharpenIntensity() const;

        void  SetBloomEnabled(bool enabled);
        bool  GetBloomEnabled() const;
        void  SetBloomIntensity(float intensity);
        float GetBloomIntensity() const;

        void  SetChromaticAberrationEnabled(bool enabled);
        bool  GetChromaticAberrationEnabled() const;
        void  SetChromaticAberrationIntensity(float intensity);
        float GetChromaticAberrationIntensity() const;

        void  SetVHSEnabled(bool enabled);
        bool  GetVHSEnabled() const;
        void  SetVHSIntensity(float intensity);
        float GetVHSIntensity() const;

        void  SetCineEnabled(bool enabled);
        bool  GetCineEnabled() const;
        void  SetCineIntensity(float intensity);
        float GetCineIntensity() const;
        void  SetCineTint(int tint);
        int   GetCineTint() const;

        void  SetContrastEnabled(bool enabled);
        bool  GetContrastEnabled() const;
        void  SetContrastAmount(float amount);
        float GetContrastAmount() const;

        void  SetLuminosityEnabled(bool enabled);
        bool  GetLuminosityEnabled() const;
        void  SetLuminosityAmount(float amount);
        float GetLuminosityAmount() const;

        void  SetTAAEnabled(bool enabled);
        bool  GetTAAEnabled() const;
        void  SetTAAIntensity(float intensity);
        float GetTAAIntensity() const;

        void  SetGlitchEnabled(bool enabled);
        bool  GetGlitchEnabled() const;
        void  SetGlitchIntensity(float intensity);
        float GetGlitchIntensity() const;
        void  SetGlitchSpeed(float speed);
        float GetGlitchSpeed() const;
        void  SetGlitchMode(int mode);
        int   GetGlitchMode() const;

        void  SetColorGradingEnabled(bool enabled);
        bool  GetColorGradingEnabled() const;
        void  SetColorGradingIntensity(float intensity);
        float GetColorGradingIntensity() const;
        void  SetColorGradingPreset(int preset);
        int   GetColorGradingPreset() const;

        void  SetPixelateEnabled(bool enabled);
        bool  GetPixelateEnabled() const;
        void  SetPixelateSize(float size);
        float GetPixelateSize() const;
        void  SetPixelateColorDepth(int depth);
        int   GetPixelateColorDepth() const;

        void  SetRadialBlurEnabled(bool enabled);
        bool  GetRadialBlurEnabled() const;
        void  SetRadialBlurIntensity(float intensity);
        float GetRadialBlurIntensity() const;

        void  SetWavesEnabled(bool enabled);
        bool  GetWavesEnabled() const;
        void  SetWavesIntensity(float intensity);
        float GetWavesIntensity() const;
        void  SetWavesSpeed(float speed);
        float GetWavesSpeed() const;
        void  SetWavesFrequency(float freq);
        float GetWavesFrequency() const;

        void  SetMirrorEnabled(bool enabled);
        bool  GetMirrorEnabled() const;
        void  SetMirrorMode(int mode);
        int   GetMirrorMode() const;

        void  SetThermalEnabled(bool enabled);
        bool  GetThermalEnabled() const;
        void  SetThermalIntensity(float intensity);
        float GetThermalIntensity() const;
        void  SetThermalMode(int mode);
        int   GetThermalMode() const;

        void  SetHalftoneEnabled(bool enabled);
        bool  GetHalftoneEnabled() const;
        void  SetHalftoneDotScale(float scale);
        float GetHalftoneDotScale() const;
        void  SetHalftoneMode(int mode);
        int   GetHalftoneMode() const;

        void  SetVolumetricFogEnabled(bool enabled);
        bool  GetVolumetricFogEnabled() const;
        void  SetVolumetricFogDensity(float density);
        float GetVolumetricFogDensity() const;
        void  SetVolumetricFogSpeed(float speed);
        float GetVolumetricFogSpeed() const;
        void  SetVolumetricFogScale(float scale);
        float GetVolumetricFogScale() const;
        void  SetVolumetricFogColorMode(int mode);
        int   GetVolumetricFogColorMode() const;

        void  SetVolumetricCloudsEnabled(bool enabled);
        bool  GetVolumetricCloudsEnabled() const;
        void  SetVolumetricCloudsCoverage(float coverage);
        float GetVolumetricCloudsCoverage() const;
        void  SetVolumetricCloudsDensity(float density);
        float GetVolumetricCloudsDensity() const;
        void  SetVolumetricCloudsSpeed(float speed);
        float GetVolumetricCloudsSpeed() const;
        void  SetVolumetricCloudsSunIntensity(float intensity);
        float GetVolumetricCloudsSunIntensity() const;

        void  SetZonedDistortionEnabled(bool enabled);
        bool  GetZonedDistortionEnabled() const;
        void  SetZonedDistortionIntensity(float intensity);
        float GetZonedDistortionIntensity() const;
        void  SetZonedDistortionSpeed(float speed);
        float GetZonedDistortionSpeed() const;
        void  SetZonedDistortionZone(int zone);
        int   GetZonedDistortionZone() const;
        void  SetZonedDistortionFeather(float feather);
        float GetZonedDistortionFeather() const;

        void   SetProjectorPostFXViewportID(ImGuiID id);
        bool   IsProjectorPostFXViewport(ImGuiID id) const;

        void* GetProjectorNativeWindow() const;
        void   RenderProjectorViewportPostFX(ImGuiViewport* viewport,
                                              void (*defaultRenderFn)(ImGuiViewport*, void*));

        void RegisterExtraProjectorViewport(ImGuiID id);
        bool IsExtraProjectorViewport(ImGuiID id) const;
        void RenderExtraProjectorViewportPostFX(ImGuiID id, ImGuiViewport* viewport,
                                                 void (*defaultRenderFn)(ImGuiViewport*, void*));

        void PruneExtraProjectorViewports(const std::vector<ImGuiID>& stillActiveThisFrame);

        void             SetSelection(const LibrarySelection& selection, bool fromQueue = false);
        LibrarySelection GetSelection();
        LibrarySelection PeekSelection();
        bool             IsSelectionFromQueue() const { return m_SelectionFromQueue; }

        void StopBackgroundMedia();
        void BlockBackgroundPath(const std::string& path);
        void UnblockBackgroundPath();

        void SetLayer0_Color(float r, float g, float b);

        void SetLayer2_Text(const std::string& text, int slideIndex = -1, bool triggerTransition = true);
        void ClearLayer2();
        void SetNextText(const std::string& text);

        int  GetActiveSlideIndex() const;
        void SetActiveSlideIndex(int index);
        void UpdateCurrentSelectionStanza(int stanzaIndex, const std::string& newText);

        void        SetClockStyleCue(const std::string& styleName);
        std::string ConsumeClockStyleCue();

        void SetPendingTransitionOverride(const std::string& name, float duration);
        bool ConsumePendingTransitionOverride(std::string& outName, float& outDuration);

        void        RequestSongEditorOpen(const std::string& filename);
        bool        ConsumeSongEditorOpenRequest(std::string& outFilename);

        void        SetSongSearchQuery(const std::string& query) { std::lock_guard<std::recursive_mutex> lock(m_Mutex); m_SongSearchQuery = query; }
        std::string GetSongSearchQuery() const { std::lock_guard<std::recursive_mutex> lock(m_Mutex); return m_SongSearchQuery; }

        void RequestNavigate(int delta);
        int  ConsumeNavigateCue();

        void*          GetPreviewTexture();
        VLCBasePlayer* GetPreviewPlayer();
        void           SetPreviewMedia(const std::string& path);
        void           StopPreviewMedia();

        void RequestPreviewLoad(const std::string& path, bool loop, bool startMuted);
        void RequestPreviewStop();
        void StopPreviewSync();
        void ClearSelection();
        void ReleasePathUsages(const std::string& path);

        void UpdateLyricsBoxStyle(const TextBoxStyle& box);

        void UpdateIndexBoxStyle(const TextBoxStyle& box, bool enabled);

        void SetCurrentRef(const std::string& ref);

        TextEffectsData GetTextEffects() const;

        void        SetProjecting(bool projecting);
        bool        IsProjecting() const;
        void        SetTargetMonitor(int index);
        void        SetProjectorSize(int w, int h);

        void        SetStaging(bool active, int monitorIndex = -1);
        bool        IsStaging() const;

        void SetMainWindow(GLFWwindow* mainWindow) { m_MainWindow = mainWindow; }

        bool CreateSecondaryWindow(const std::string& id, int monitorIndex,
                                    const std::string& title,
                                    SecondaryOutputWindow::RenderFn renderFn);
        void DestroySecondaryWindow(const std::string& id);
        void DestroyAllSecondaryWindows();
        bool IsSecondaryWindowActive(const std::string& id) const;

        void RenderAllSecondaryWindows();

        bool        CreateProjectorWindow(int monitorIndex);
        void        DestroyProjectorWindow();
        bool        IsProjectorWindowActive() const;
        GLFWwindow* GetProjectorWindow() const;

        bool        CreateStageWindow(int monitorIndex);
        void        DestroyStageWindow();
        bool        IsStageWindowActive() const;
        GLFWwindow* GetStageWindow() const;

        void        SetStageAlertMessage(const std::string& msg);
        std::string GetStageAlertMessage() const;
        void        ClearStageAlertMessage();

        void        SetProjectorRenderFn(SecondaryOutputWindow::RenderFn fn) { m_ProjectorRenderFn = std::move(fn); }
        void        SetStageRenderFn(SecondaryOutputWindow::RenderFn fn)     { m_StageRenderFn = std::move(fn); }

        float GetLivePosition();
        void  SetLivePosition(float pos);
        int   GetLiveVolume();
        bool  GetLiveMute();
        void  SetLiveVolume(int volume);
        void  SetLiveMute(bool mute);

        void SetLiveEqualizerEnabled(bool enabled);
        void SetLiveEqualizerPreamp(float preampDb);
        void SetLiveEqualizerBand(int index, float ampDb);

        bool  GetLiveLoop();
        void  SetLiveLoop(bool loop);

        void        LoadFontsIntoImGui();
        void        LoadSingleFontIntoImGui(const std::string& fontPath);
        void        SyncFontListFromDisk(std::vector<std::string>& outList);
        std::string GetActiveFontName() const;
        ImFont*     GetImGuiFont(const std::string& fontName, float size = 0.0f);
        std::string GetActiveFontFilePath() const;

        void                     SaveStyle(const SavedStyle& style);
        void                     DeleteStyle(const std::string& name);
        std::vector<std::string> GetSavedStyleNames() const;
        bool                     GetSavedStyle(const std::string& name, SavedStyle& outStyle) const;

        void        SetCategoryDefaultStyle(ItemType category, const std::string& styleName);
        std::string GetCategoryDefaultStyle(ItemType category) const;
        void        LoadCategoryStyles();
        void        SaveCategoryStyles() const;

        void ToggleNetworkStream(bool enable, int port = 8080);
        bool IsStreamingNet() const;
        bool RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB);

        unsigned int RenderPublicCompositeToTexture(int w, int h);

        NetworkStreamServer* GetNetworkServer() { return m_NetworkServer.get(); }

        void ToggleChatServer(bool enable, int port = 8080);
        bool IsChatRunning() const;
        ChatMessageStore* GetChatMessageStore() { return &m_ChatMessageStore; }

       void PushFrame(std::vector<uint8_t> jpegData)
{

    bool hasRealFrame = !jpegData.empty();

    {
        std::lock_guard<std::mutex> lk(m_FrameMutex);
        m_LatestFrame = std::move(jpegData);
    }
    m_FrameProviderActive.store(hasRealFrame);
    ++m_StreamVersion;
}

        void SetBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);
        bool GetContentAllowsAudio() const;

        void        SetOverlayMedia(const std::string& pngPath);
        void        ClearOverlay();
        bool        HasOverlay() const;
        std::string GetOverlayPath() const;
        void*       GetOverlayTexture();

        void        SetOverlayClockLayer(bool hasClock, const ProyecThor::UI::OverlayLayer& layer,
                                          int canvasW, int canvasH);
        bool        HasOverlayClockLayer() const;
        ProyecThor::UI::OverlayLayer GetOverlayClockLayer() const;
        int         GetOverlayClockCanvasW() const;
        int         GetOverlayClockCanvasH() const;

        void         SetLiveOverlayClockText(const std::string& text, const float* colorOverride = nullptr);
        std::string  GetLiveOverlayClockText() const;
        bool         HasLiveOverlayClockColorOverride() const;
        void         GetLiveOverlayClockColorOverride(float outRGBA[4]) const;

        void SetBackgroundAudio();

        void              SetAudioPanelRef(ProyecThor::UI::AudioPanel* panel) { m_AudioPanelRef = panel; }
        ProyecThor::UI::AudioPanel* GetAudioPanelRef() const { return m_AudioPanelRef; }

        void                        SetAnnouncementsRef(ProyecThor::UI::Announcements* a) { m_AnnouncementsRef = a; }
        ProyecThor::UI::Announcements* GetAnnouncementsRef() const { return m_AnnouncementsRef; }

        void                 SetOClockRef(ProyecThor::UI::OClock* c) { m_OClockRef = c; }
        ProyecThor::UI::OClock* GetOClockRef() const { return m_OClockRef; }

        void                       SetCapturePanelRef(ProyecThor::UI::CapturePanel* c) { m_CapturePanelRef = c; }
        ProyecThor::UI::CapturePanel* GetCapturePanelRef() const { return m_CapturePanelRef; }

        void   SetLive3DModelActive(bool active) { m_Live3DModelActive = active; }
        bool   IsLive3DModelActive() const { return m_Live3DModelActive; }
        void   SetLive3DModelTexture(void* texID) { m_Live3DModelTexture = texID; }
        void*  GetLive3DModelTexture() const { return m_Live3DModelTexture; }

        void                       SetLabPanelRef(ProyecThor::UI::LabPanel* p) { m_LabPanelRef = p; }
        ProyecThor::UI::LabPanel*  GetLabPanelRef() const { return m_LabPanelRef; }
        void                       SetLiveLabActive(bool active) { m_LiveLabActive = active; }
        bool                       IsLiveLabActive() const { return m_LiveLabActive; }

        void PreloadNextBackgroundMedia(const std::string& path, bool allowAudio = false);
        void CommitNextBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);

        bool  IsBackgroundSwapPending() const;
        float GetBackgroundSwapEta() const;

        void*  GetStandbyBackgroundTexture();
        void*  GetPreviewStandbyBackgroundTexture(int targetW, int targetH);
        float  GetBackgroundBlendProgress() const;
        bool   IsBackgroundStandbyReady();
        int    GetBackgroundTransitionType() const;

        void  SetLoadingLogoPath(const std::string& path);
        void* GetLoadingLogoTexture() const;
        int   GetLoadingLogoWidth()  const { return m_LoadingLogoW; }
        int   GetLoadingLogoHeight() const { return m_LoadingLogoH; }
        bool  ShouldShowLoadingScreen() const;

        unsigned int GetBoxBgTexture(bool isLyrics, const std::string& path);

    private:
        void RenderDefaultStyleCombo();
        void EnsureFBO(int w, int h);
        void DestroyFBO();

        std::string ResolveFontFilePath(const std::string& fontName) const;
bool m_GlobalMuted = false;
        unsigned int m_FBO          = 0;
        unsigned int m_FBOTex       = 0;
        unsigned int m_FBORenderBuf = 0;
        int          m_FBOWidth     = 0;
        int          m_FBOHeight    = 0;
        double       m_LastFBOCaptureTime = 0.0;

        unsigned int m_PBO[2] = { 0, 0 };
        int          m_PBOIndex = 0;

        bool m_stretchToFill = false;
        ImGuiID m_ProjectorPostFXViewportID = 0;
        std::unique_ptr<PresentationCoreImpl> m_Impl;

        std::string m_OverlayPath;
        bool        m_HasOverlay = false;

        ProyecThor::UI::OverlayLayer m_OverlayClockLayer;
        bool        m_HasOverlayClockLayer  = false;
        int         m_OverlayClockCanvasW   = 1920;
        int         m_OverlayClockCanvasH   = 1080;
        std::string m_LiveOverlayClockText;
        bool        m_HasLiveOverlayClockColorOverride = false;
        float       m_LiveOverlayClockColorOverride[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        std::string m_LiveQuickNoteLANStyleName;
        bool        m_HasLiveQuickNoteLANColorOverride = false;
        float       m_LiveQuickNoteLANColorOverride[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        mutable std::recursive_mutex m_Mutex;

        std::vector<std::string> m_PendingClockTitles;

        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;
        bool              m_SelectionFromQueue = false;
        int               m_ActiveSlideIndex   = -1;

        std::string m_PendingClockStyleCue;
        bool        m_HasClockStyleCue = false;

        std::string m_PendingTransitionName;
        float       m_PendingTransitionDuration = -1.0f;
        bool        m_HasTransitionOverride = false;

        std::string m_PendingSongEditorOpenFile;
        bool        m_HasSongEditorOpenRequest = false;

        std::string  m_LoadingLogoPath;
        unsigned int m_LoadingLogoTex = 0;
        int          m_LoadingLogoW = 0;
        int          m_LoadingLogoH = 0;

        std::string  m_LyricsBgTexPath;
        unsigned int m_LyricsBgTex = 0;
        std::string  m_IndexBgTexPath;
        unsigned int m_IndexBgTex = 0;

        int         m_ProjectorWidth  = 1920;
        int         m_ProjectorHeight = 1080;
        std::string m_ActiveFontName  = "Predeterminada";
        std::unordered_map<std::string, ImFont*>      m_ImGuiFonts;
        std::unordered_map<std::string, SavedStyle>   m_SavedStyles;
        std::unordered_map<int, std::string>          m_CategoryDefaultStyles;

        GLFWwindow* m_MainWindow = nullptr;

        struct SecondaryOutput {
            SecondaryOutputWindow           window;
            SecondaryOutputWindow::RenderFn renderFn;
        };
        std::unordered_map<std::string, SecondaryOutput> m_SecondaryWindows;
        mutable std::mutex m_SecondaryWindowsMutex;

        static constexpr const char* kProjectorId = "projector";
        static constexpr const char* kStageId     = "stage";
        std::string                m_StageAlertMessage;
        mutable std::mutex         m_StageAlertMutex;
        SecondaryOutputWindow::RenderFn m_ProjectorRenderFn;
        SecondaryOutputWindow::RenderFn m_StageRenderFn;

        std::unique_ptr<NetworkStreamServer> m_NetworkServer;
        ChatMessageStore                     m_ChatMessageStore;

        void WireNetworkServerProviders(NetworkStreamServer& srv);

        ProyecThor::UI::AudioPanel*    m_AudioPanelRef    = nullptr;
        ProyecThor::UI::Announcements* m_AnnouncementsRef = nullptr;
        ProyecThor::UI::OClock*        m_OClockRef        = nullptr;
        ProyecThor::UI::CapturePanel*  m_CapturePanelRef  = nullptr;
        ProyecThor::UI::LabPanel*      m_LabPanelRef      = nullptr;

        std::atomic<bool>              m_Live3DModelActive{ false };
        void*                          m_Live3DModelTexture = nullptr;

        std::atomic<bool>              m_LiveLabActive{ false };

        void SetBgTypeLocked(PresentationState::BackgroundType newType);
        std::atomic<uint64_t>                m_StreamVersion { 0 };

        mutable std::mutex    m_FrameMutex;
        std::vector<uint8_t>  m_LatestFrame;
        std::atomic<bool>     m_FrameProviderActive { false };

        std::atomic<OutputContentMode> m_LanContentMode { OutputContentMode::Live };
        std::string                    m_SongSearchQuery;
        std::atomic<int>               m_PendingNavigateDelta { 0 };
    };

}

#endif
