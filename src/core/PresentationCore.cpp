#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "graphics/shaders/CompositePostChain.h"
#include "stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "core/settings/SettingsManager.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <vector>
#include "AppPaths.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include "NetworkStreamServer.h"
#include "PreviewLoadWorker.h"
#include "ui/views/Audio.h"

namespace ProyecThor::Core {

   class PresentationCoreImpl {
    public:

        BackgroundLayer background{ false };

        BackgroundLayer preview{ true };

        PreviewLoadWorker previewLoader;

        Shaders::CompositePostChain compositeFX;

        std::unordered_map<ImGuiID, std::unique_ptr<Shaders::CompositePostChain>> extraCompositeFX;

        GLuint overlayTex  = 0;
        int    overlayTexW = 0;
        int    overlayTexH = 0;
    };

    PresentationCore::PresentationCore()
        : m_Impl(std::make_unique<PresentationCoreImpl>()) {}

    PresentationCore::~PresentationCore() {
        if (m_NetworkServer && m_NetworkServer->IsRunning())
            m_NetworkServer->Stop();
        DestroyFBO();
        DestroyAllSecondaryWindows();
    }
LibrarySelection PresentationCore::GetSelection() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        LibrarySelection sel  = m_CurrentSelection;
        m_CurrentSelection.title = "";
        m_CurrentSelection.type  = ItemType::None;
        m_CurrentSelection.contentData.clear();
        return sel;
    }

    LibrarySelection PresentationCore::PeekSelection() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_CurrentSelection;
    }

void PresentationCore::SetLiveQuickNote(const std::string& text, const float* ) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.currentText   = text;
    m_State.showText      = !text.empty();
    m_State.showQuickNote = true;
    m_State.isProjecting  = true;

    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::ClearQuickNote() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.currentText   = "";
    m_State.showText      = false;
    m_State.showQuickNote = false;
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

    void PresentationCore::SetLiveQuickNoteLAN(const std::string& text, const float* colorOverride, const std::string& styleName) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.lanQuickNoteText = text;
        m_State.showLanQuickNote = !text.empty();
        m_LiveQuickNoteLANStyleName = styleName;
        m_HasLiveQuickNoteLANColorOverride = (colorOverride != nullptr);
        if (colorOverride) {
            for (int i = 0; i < 4; i++) m_LiveQuickNoteLANColorOverride[i] = colorOverride[i];
        }
        ++m_StreamVersion;
    }

    void PresentationCore::ClearQuickNoteLAN() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.lanQuickNoteText = "";
        m_State.showLanQuickNote = false;
        m_LiveQuickNoteLANStyleName.clear();
        m_HasLiveQuickNoteLANColorOverride = false;
        ++m_StreamVersion;
    }

    void PresentationCore::PushRemoteClockTitle(const std::string& text) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_PendingClockTitles.push_back(text);
    }

    std::vector<std::string> PresentationCore::DrainRemoteClockTitles() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        std::vector<std::string> out;
        out.swap(m_PendingClockTitles);
        return out;
    }

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State;
    }
void PresentationCore::SetGlobalMute(bool mute) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_GlobalMuted = mute;
}

bool PresentationCore::GetGlobalMute() const {
    return m_GlobalMuted;
}
    void* PresentationCore::GetPreviewTexture() {
        return m_Impl ? m_Impl->preview.GetTextureID() : nullptr;
    }

    VLCBasePlayer* PresentationCore::GetPreviewPlayer() {
        return m_Impl ? m_Impl->preview.GetPlayer() : nullptr;
    }

    void PresentationCore::SetPreviewMedia(const std::string& path) {
        if (m_Impl) m_Impl->preview.SetVideo(path);
    }

    void PresentationCore::StopPreviewMedia() {
        if (m_Impl) m_Impl->preview.SetSolidColor(0.0f, 0.0f, 0.0f);
    }

    void PresentationCore::RequestPreviewLoad(const std::string& path, bool loop, bool startMuted) {
        if (!m_Impl) return;
        m_Impl->previewLoader.RequestLoad(m_Impl->preview.GetPlayer(), path, loop, startMuted);
    }

    void PresentationCore::RequestPreviewStop() {
        if (!m_Impl) return;
        m_Impl->previewLoader.RequestStop(m_Impl->preview.GetPlayer());
    }

    void PresentationCore::StopPreviewSync() {
        if (!m_Impl) return;
        m_Impl->previewLoader.RequestStopSync(m_Impl->preview.GetPlayer(), 1000);
        m_Impl->preview.SetSolidColor(0.0f, 0.0f, 0.0f);
    }

    void PresentationCore::ClearSelection() {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_CurrentSelection = LibrarySelection{};
            m_SelectionFromQueue = false;
            m_ActiveSlideIndex = -1;
            ++m_StreamVersion;
        }
    }

    void PresentationCore::ReleasePathUsages(const std::string& path) {
        if (path.empty()) return;

        std::string normTarget = path;
        std::replace(normTarget.begin(), normTarget.end(), '\\', '/');
        std::transform(normTarget.begin(), normTarget.end(), normTarget.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        StopPreviewSync();

        bool clearSel = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            std::string normSel = m_CurrentSelection.title;
            std::replace(normSel.begin(), normSel.end(), '\\', '/');
            std::transform(normSel.begin(), normSel.end(), normSel.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (!normSel.empty() && (normSel == normTarget || normTarget.find(normSel) != std::string::npos || normSel.find(normTarget) != std::string::npos)) {
                clearSel = true;
            }
        }
        if (clearSel) {
            ClearSelection();
        }

        bool stopBg = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            std::string normBg = m_State.bgPath;
            std::replace(normBg.begin(), normBg.end(), '\\', '/');
            std::transform(normBg.begin(), normBg.end(), normBg.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (!normBg.empty() && (normBg == normTarget || normTarget.find(normBg) != std::string::npos || normBg.find(normTarget) != std::string::npos)) {
                stopBg = true;
            }
        }
        if (stopBg) {
            StopBackgroundMedia();
        }
    }

    void* PresentationCore::GetProcessedBackgroundTexture(int targetW, int targetH) {
        return m_Impl ? m_Impl->background.GetProcessedTexture(targetW, targetH) : nullptr;
    }

    void* PresentationCore::GetPreviewBackgroundTexture(int targetW, int targetH) {
        if (!m_Impl) return nullptr;
        void* rawTex = m_Impl->background.GetProcessedTexture(targetW, targetH);
        if (!rawTex) return nullptr;

        GLuint raw = static_cast<GLuint>(reinterpret_cast<uintptr_t>(rawTex));
        GLuint processed = m_Impl->compositeFX.ProcessBackgroundForPreview(raw, targetW, targetH);
        return (void*)(uintptr_t)processed;
    }

    void* PresentationCore::GetBackgroundFillTexture(int workW, int workH) {
        return m_Impl ? m_Impl->background.GetBlurredFillTexture(workW, workH) : nullptr;
    }
    void PresentationCore::SetFillBlurEnabled(bool enabled) {
        if (m_Impl) m_Impl->background.SetFillBlurEnabled(enabled);
    }
    bool PresentationCore::GetFillBlurEnabled() const {
        return m_Impl ? m_Impl->background.GetFillBlurEnabled() : false;
    }
    void PresentationCore::SetFillBlurBrightness(float v) {
        if (m_Impl) m_Impl->background.SetFillBlurBrightness(v);
    }
    float PresentationCore::GetFillBlurBrightness() const {
        return m_Impl ? m_Impl->background.GetFillBlurBrightness() : 0.6f;
    }

    void PresentationCore::SetBackgroundPingPongLoop(bool enabled) {
        if (m_Impl) m_Impl->background.SetPingPongLoop(enabled);
    }

    bool PresentationCore::GetBackgroundPingPongLoop() const {
        return m_Impl ? m_Impl->background.GetPingPongLoop() : false;
    }

    void PresentationCore::SetFSREnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->background.SetFSREnabled(enabled);

        if (enabled) m_Impl->background.SetNISEnabled(false);
    }

    bool PresentationCore::GetFSREnabled() const {
        return m_Impl ? m_Impl->background.GetFSREnabled() : false;
    }

    void PresentationCore::SetFSRSharpness(float sharpness) {
        if (m_Impl) m_Impl->background.SetFSRSharpness(sharpness);
    }

    float PresentationCore::GetFSRSharpness() const {
        return m_Impl ? m_Impl->background.GetFSRSharpness() : 0.2f;
    }

    void PresentationCore::SetNISEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->background.SetNISEnabled(enabled);
        if (enabled) m_Impl->background.SetFSREnabled(false);
    }

    bool PresentationCore::GetNISEnabled() const {
        return m_Impl ? m_Impl->background.GetNISEnabled() : false;
    }

    void PresentationCore::SetNISSharpness(float sharpness) {
        if (m_Impl) m_Impl->background.SetNISSharpness(sharpness);
    }

    float PresentationCore::GetNISSharpness() const {
        return m_Impl ? m_Impl->background.GetNISSharpness() : 0.5f;
    }

    void PresentationCore::SetVideoRenderEngine(int engine) {
#ifdef _WIN32
        if (m_Impl) m_Impl->background.SetUseNativeEngine(engine != 0);
#else

        if (m_Impl) m_Impl->background.SetUseNativeEngine(false);
#endif
    }
    int PresentationCore::GetVideoRenderEngine() const {
        return (m_Impl && m_Impl->background.GetUseNativeEngine()) ? 1 : 0;
    }

    void PresentationCore::SetVLCHardwareDecoder(const std::string& dec) {
        VLCBasePlayer::SetDefaultHwDecoder(dec);
    }
    std::string PresentationCore::GetVLCHardwareDecoder() const {
        return VLCBasePlayer::GetDefaultHwDecoder();
    }

    void PresentationCore::SetVLCVideoOutput(const std::string& vout) {
        VLCBasePlayer::SetDefaultVideoOutput(vout);
    }
    std::string PresentationCore::GetVLCVideoOutput() const {
        return VLCBasePlayer::GetDefaultVideoOutput();
    }

    void PresentationCore::SetVLCDeinterlace(const std::string& deint) {
        VLCBasePlayer::SetDefaultDeinterlace(deint);
    }
    std::string PresentationCore::GetVLCDeinterlace() const {
        return VLCBasePlayer::GetDefaultDeinterlace();
    }

    bool PresentationCore::IsActiveNativeVideo() const {
        return m_Impl ? m_Impl->background.IsActiveNative() : false;
    }

    void PresentationCore::SetWindowFullscreen(bool fullscreen) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.windowFullscreen = fullscreen;
    }
    bool PresentationCore::GetWindowFullscreen() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.windowFullscreen;
    }

    void PresentationCore::SetVLCNativeForFondos(bool enable) {
        if (m_Impl) m_Impl->background.SetVLCNativeForFondos(enable);
    }
    bool PresentationCore::GetVLCNativeForFondos() const {
        return m_Impl ? m_Impl->background.GetVLCNativeForFondos() : false;
    }

    void PresentationCore::ReloadVLCPlayers() {
        if (m_Impl) {
            m_Impl->background.ReloadPlayers();
        }
    }

    void PresentationCore::SetCRTEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetCRTEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetCRTEnabled(enabled);
    }
    bool PresentationCore::GetCRTEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetCRTEnabled() : false;
    }
    void PresentationCore::SetCRTScanlineIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetCRTScanlineIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetCRTScanlineIntensity(intensity);
    }
    float PresentationCore::GetCRTScanlineIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetCRTScanlineIntensity() : 0.5f;
    }

    void PresentationCore::SetGrainEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGrainEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGrainEnabled(enabled);
    }
    bool PresentationCore::GetGrainEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetGrainEnabled() : false;
    }
    void PresentationCore::SetGrainIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGrainIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGrainIntensity(intensity);
    }
    float PresentationCore::GetGrainIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetGrainIntensity() : 0.15f;
    }

    void PresentationCore::SetFXAAEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetFXAAEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetFXAAEnabled(enabled);
    }
    bool PresentationCore::GetFXAAEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetFXAAEnabled() : false;
    }

    void PresentationCore::SetSaturationEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetSaturationEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetSaturationEnabled(enabled);
    }
    bool PresentationCore::GetSaturationEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetSaturationEnabled() : false;
    }
    void PresentationCore::SetSaturationAmount(float amount) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetSaturationAmount(amount);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetSaturationAmount(amount);
    }
    float PresentationCore::GetSaturationAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetSaturationAmount() : 1.3f;
    }

    void PresentationCore::SetVignetteEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVignetteEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVignetteEnabled(enabled);
    }
    bool PresentationCore::GetVignetteEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVignetteEnabled() : false;
    }
    void PresentationCore::SetVignetteIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVignetteIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVignetteIntensity(intensity);
    }
    float PresentationCore::GetVignetteIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVignetteIntensity() : 0.45f;
    }

    void PresentationCore::SetBlurEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetBlurEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetBlurEnabled(enabled);
    }
    bool PresentationCore::GetBlurEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetBlurEnabled() : false;
    }
    void PresentationCore::SetBlurIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetBlurIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetBlurIntensity(intensity);
    }
    float PresentationCore::GetBlurIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetBlurIntensity() : 0.35f;
    }

    void PresentationCore::SetSharpenEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetSharpenEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetSharpenEnabled(enabled);
    }
    bool PresentationCore::GetSharpenEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetSharpenEnabled() : false;
    }
    void PresentationCore::SetSharpenIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetSharpenIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetSharpenIntensity(intensity);
    }
    float PresentationCore::GetSharpenIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetSharpenIntensity() : 0.35f;
    }

    void PresentationCore::SetBloomEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetBloomEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetBloomEnabled(enabled);
    }
    bool PresentationCore::GetBloomEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetBloomEnabled() : false;
    }
    void PresentationCore::SetBloomIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetBloomIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetBloomIntensity(intensity);
    }
    float PresentationCore::GetBloomIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetBloomIntensity() : 0.35f;
    }

    void PresentationCore::SetChromaticAberrationEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetChromaticAberrationEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetChromaticAberrationEnabled(enabled);
    }
    bool PresentationCore::GetChromaticAberrationEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetChromaticAberrationEnabled() : false;
    }
    void PresentationCore::SetChromaticAberrationIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetChromaticAberrationIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetChromaticAberrationIntensity(intensity);
    }
    float PresentationCore::GetChromaticAberrationIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetChromaticAberrationIntensity() : 0.35f;
    }

    void PresentationCore::SetVHSEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVHSEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVHSEnabled(enabled);
    }
    bool PresentationCore::GetVHSEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVHSEnabled() : false;
    }
    void PresentationCore::SetVHSIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVHSIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVHSIntensity(intensity);
    }
    float PresentationCore::GetVHSIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVHSIntensity() : 0.5f;
    }

    void PresentationCore::SetCineEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetCineEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetCineEnabled(enabled);
    }
    bool PresentationCore::GetCineEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetCineEnabled() : false;
    }
    void PresentationCore::SetCineIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetCineIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetCineIntensity(intensity);
    }
    float PresentationCore::GetCineIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetCineIntensity() : 0.5f;
    }
    void PresentationCore::SetCineTint(int tint) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetCineTint(tint);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetCineTint(tint);
    }
    int PresentationCore::GetCineTint() const {
        return m_Impl ? m_Impl->compositeFX.GetCineTint() : 0;
    }

    void PresentationCore::SetContrastEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetContrastEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetContrastEnabled(enabled);
    }
    bool PresentationCore::GetContrastEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetContrastEnabled() : false;
    }
    void PresentationCore::SetContrastAmount(float amount) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetContrastAmount(amount);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetContrastAmount(amount);
    }
    float PresentationCore::GetContrastAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetContrastAmount() : 1.3f;
    }

    void PresentationCore::SetLuminosityEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetLuminosityEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetLuminosityEnabled(enabled);
    }
    bool PresentationCore::GetLuminosityEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetLuminosityEnabled() : false;
    }
    void PresentationCore::SetLuminosityAmount(float amount) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetLuminosityAmount(amount);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetLuminosityAmount(amount);
    }
    float PresentationCore::GetLuminosityAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetLuminosityAmount() : 1.2f;
    }

    void PresentationCore::SetTAAEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetTAAEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetTAAEnabled(enabled);
    }
    bool PresentationCore::GetTAAEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetTAAEnabled() : false;
    }
    void PresentationCore::SetTAAIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetTAAIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetTAAIntensity(intensity);
    }
    float PresentationCore::GetTAAIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetTAAIntensity() : 0.5f;
    }

    void PresentationCore::SetGlitchEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGlitchEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGlitchEnabled(enabled);
    }
    bool PresentationCore::GetGlitchEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetGlitchEnabled() : false;
    }
    void PresentationCore::SetGlitchIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGlitchIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGlitchIntensity(intensity);
    }
    float PresentationCore::GetGlitchIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetGlitchIntensity() : 0.40f;
    }
    void PresentationCore::SetGlitchSpeed(float speed) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGlitchSpeed(speed);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGlitchSpeed(speed);
    }
    float PresentationCore::GetGlitchSpeed() const {
        return m_Impl ? m_Impl->compositeFX.GetGlitchSpeed() : 1.0f;
    }
    void PresentationCore::SetGlitchMode(int mode) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetGlitchMode(mode);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetGlitchMode(mode);
    }
    int PresentationCore::GetGlitchMode() const {
        return m_Impl ? m_Impl->compositeFX.GetGlitchMode() : 0;
    }

    void PresentationCore::SetColorGradingEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetColorGradingEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetColorGradingEnabled(enabled);
    }
    bool PresentationCore::GetColorGradingEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetColorGradingEnabled() : false;
    }
    void PresentationCore::SetColorGradingIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetColorGradingIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetColorGradingIntensity(intensity);
    }
    float PresentationCore::GetColorGradingIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetColorGradingIntensity() : 0.75f;
    }
    void PresentationCore::SetColorGradingPreset(int preset) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetColorGradingPreset(preset);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetColorGradingPreset(preset);
    }
    int PresentationCore::GetColorGradingPreset() const {
        return m_Impl ? m_Impl->compositeFX.GetColorGradingPreset() : 1;
    }

    void PresentationCore::SetPixelateEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetPixelateEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetPixelateEnabled(enabled);
    }
    bool PresentationCore::GetPixelateEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetPixelateEnabled() : false;
    }
    void PresentationCore::SetPixelateSize(float size) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetPixelateSize(size);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetPixelateSize(size);
    }
    float PresentationCore::GetPixelateSize() const {
        return m_Impl ? m_Impl->compositeFX.GetPixelateSize() : 12.0f;
    }
    void PresentationCore::SetPixelateColorDepth(int depth) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetPixelateColorDepth(depth);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetPixelateColorDepth(depth);
    }
    int PresentationCore::GetPixelateColorDepth() const {
        return m_Impl ? m_Impl->compositeFX.GetPixelateColorDepth() : 0;
    }

    void PresentationCore::SetRadialBlurEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetRadialBlurEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetRadialBlurEnabled(enabled);
    }
    bool PresentationCore::GetRadialBlurEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetRadialBlurEnabled() : false;
    }
    void PresentationCore::SetRadialBlurIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetRadialBlurIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetRadialBlurIntensity(intensity);
    }
    float PresentationCore::GetRadialBlurIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetRadialBlurIntensity() : 0.35f;
    }

    void PresentationCore::SetWavesEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetWavesEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetWavesEnabled(enabled);
    }
    bool PresentationCore::GetWavesEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetWavesEnabled() : false;
    }
    void PresentationCore::SetWavesIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetWavesIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetWavesIntensity(intensity);
    }
    float PresentationCore::GetWavesIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetWavesIntensity() : 0.35f;
    }
    void PresentationCore::SetWavesSpeed(float speed) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetWavesSpeed(speed);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetWavesSpeed(speed);
    }
    float PresentationCore::GetWavesSpeed() const {
        return m_Impl ? m_Impl->compositeFX.GetWavesSpeed() : 1.0f;
    }
    void PresentationCore::SetWavesFrequency(float freq) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetWavesFrequency(freq);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetWavesFrequency(freq);
    }
    float PresentationCore::GetWavesFrequency() const {
        return m_Impl ? m_Impl->compositeFX.GetWavesFrequency() : 8.0f;
    }

    void PresentationCore::SetMirrorEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetMirrorEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetMirrorEnabled(enabled);
    }
    bool PresentationCore::GetMirrorEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetMirrorEnabled() : false;
    }
    void PresentationCore::SetMirrorMode(int mode) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetMirrorMode(mode);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetMirrorMode(mode);
    }
    int PresentationCore::GetMirrorMode() const {
        return m_Impl ? m_Impl->compositeFX.GetMirrorMode() : 0;
    }

    void PresentationCore::SetThermalEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetThermalEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetThermalEnabled(enabled);
    }
    bool PresentationCore::GetThermalEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetThermalEnabled() : false;
    }
    void PresentationCore::SetThermalIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetThermalIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetThermalIntensity(intensity);
    }
    float PresentationCore::GetThermalIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetThermalIntensity() : 0.85f;
    }
    void PresentationCore::SetThermalMode(int mode) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetThermalMode(mode);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetThermalMode(mode);
    }
    int PresentationCore::GetThermalMode() const {
        return m_Impl ? m_Impl->compositeFX.GetThermalMode() : 0;
    }

    void PresentationCore::SetHalftoneEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetHalftoneEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetHalftoneEnabled(enabled);
    }
    bool PresentationCore::GetHalftoneEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetHalftoneEnabled() : false;
    }
    void PresentationCore::SetHalftoneDotScale(float scale) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetHalftoneDotScale(scale);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetHalftoneDotScale(scale);
    }
    float PresentationCore::GetHalftoneDotScale() const {
        return m_Impl ? m_Impl->compositeFX.GetHalftoneDotScale() : 10.0f;
    }
    void PresentationCore::SetHalftoneMode(int mode) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetHalftoneMode(mode);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetHalftoneMode(mode);
    }
    int PresentationCore::GetHalftoneMode() const {
        return m_Impl ? m_Impl->compositeFX.GetHalftoneMode() : 0;
    }

    void PresentationCore::SetVolumetricFogEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricFogEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricFogEnabled(enabled);
    }
    bool PresentationCore::GetVolumetricFogEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricFogEnabled() : false;
    }
    void PresentationCore::SetVolumetricFogDensity(float density) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricFogDensity(density);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricFogDensity(density);
    }
    float PresentationCore::GetVolumetricFogDensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricFogDensity() : 0.50f;
    }
    void PresentationCore::SetVolumetricFogSpeed(float speed) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricFogSpeed(speed);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricFogSpeed(speed);
    }
    float PresentationCore::GetVolumetricFogSpeed() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricFogSpeed() : 1.0f;
    }
    void PresentationCore::SetVolumetricFogScale(float scale) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricFogScale(scale);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricFogScale(scale);
    }
    float PresentationCore::GetVolumetricFogScale() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricFogScale() : 3.5f;
    }
    void PresentationCore::SetVolumetricFogColorMode(int mode) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricFogColorMode(mode);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricFogColorMode(mode);
    }
    int PresentationCore::GetVolumetricFogColorMode() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricFogColorMode() : 0;
    }

    void PresentationCore::SetVolumetricCloudsEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricCloudsEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricCloudsEnabled(enabled);
    }
    bool PresentationCore::GetVolumetricCloudsEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricCloudsEnabled() : false;
    }
    void PresentationCore::SetVolumetricCloudsCoverage(float coverage) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricCloudsCoverage(coverage);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricCloudsCoverage(coverage);
    }
    float PresentationCore::GetVolumetricCloudsCoverage() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricCloudsCoverage() : 0.55f;
    }
    void PresentationCore::SetVolumetricCloudsDensity(float density) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricCloudsDensity(density);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricCloudsDensity(density);
    }
    float PresentationCore::GetVolumetricCloudsDensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricCloudsDensity() : 0.60f;
    }
    void PresentationCore::SetVolumetricCloudsSpeed(float speed) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricCloudsSpeed(speed);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricCloudsSpeed(speed);
    }
    float PresentationCore::GetVolumetricCloudsSpeed() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricCloudsSpeed() : 0.80f;
    }
    void PresentationCore::SetVolumetricCloudsSunIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetVolumetricCloudsSunIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetVolumetricCloudsSunIntensity(intensity);
    }
    float PresentationCore::GetVolumetricCloudsSunIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVolumetricCloudsSunIntensity() : 0.65f;
    }

    void PresentationCore::SetZonedDistortionEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetZonedDistortionEnabled(enabled);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetZonedDistortionEnabled(enabled);
    }
    bool PresentationCore::GetZonedDistortionEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetZonedDistortionEnabled() : false;
    }
    void PresentationCore::SetZonedDistortionIntensity(float intensity) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetZonedDistortionIntensity(intensity);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetZonedDistortionIntensity(intensity);
    }
    float PresentationCore::GetZonedDistortionIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetZonedDistortionIntensity() : 0.45f;
    }
    void PresentationCore::SetZonedDistortionSpeed(float speed) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetZonedDistortionSpeed(speed);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetZonedDistortionSpeed(speed);
    }
    float PresentationCore::GetZonedDistortionSpeed() const {
        return m_Impl ? m_Impl->compositeFX.GetZonedDistortionSpeed() : 1.20f;
    }
    void PresentationCore::SetZonedDistortionZone(int zone) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetZonedDistortionZone(zone);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetZonedDistortionZone(zone);
    }
    int PresentationCore::GetZonedDistortionZone() const {
        return m_Impl ? m_Impl->compositeFX.GetZonedDistortionZone() : 0;
    }
    void PresentationCore::SetZonedDistortionFeather(float feather) {
        if (!m_Impl) return;
        m_Impl->compositeFX.SetZonedDistortionFeather(feather);
        for (auto& [id, chain] : m_Impl->extraCompositeFX) chain->SetZonedDistortionFeather(feather);
    }
    float PresentationCore::GetZonedDistortionFeather() const {
        return m_Impl ? m_Impl->compositeFX.GetZonedDistortionFeather() : 0.35f;
    }

    void PresentationCore::SetProjectorPostFXViewportID(ImGuiID id) {
        m_ProjectorPostFXViewportID = id;
    }
    bool PresentationCore::IsProjectorPostFXViewport(ImGuiID id) const {
        return id != 0 && id == m_ProjectorPostFXViewportID;
    }

    void* PresentationCore::GetProjectorNativeWindow() const {
#ifdef _WIN32
        if (m_ProjectorPostFXViewportID == 0) return nullptr;
        ImGuiViewport* vp = ImGui::FindViewportByID(m_ProjectorPostFXViewportID);
        if (!vp) return nullptr;

        if (vp->PlatformHandleRaw) return vp->PlatformHandleRaw;
        if (vp->PlatformHandle)
            return (void*)glfwGetWin32Window(static_cast<GLFWwindow*>(vp->PlatformHandle));
        return nullptr;
#else
        return nullptr;
#endif
    }
    void PresentationCore::RenderProjectorViewportPostFX(ImGuiViewport* viewport,
                                                         void (*defaultRenderFn)(ImGuiViewport*, void*))
    {
        if (m_Impl) m_Impl->compositeFX.RenderViewport(viewport, defaultRenderFn);
        else if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
    }

    void PresentationCore::RegisterExtraProjectorViewport(ImGuiID id) {
        if (!m_Impl || id == 0) return;
        auto& map = m_Impl->extraCompositeFX;
        if (map.find(id) != map.end()) return;

        auto chain = std::make_unique<Shaders::CompositePostChain>();
        const auto& src = m_Impl->compositeFX;
        chain->SetCRTEnabled(src.GetCRTEnabled());
        chain->SetCRTScanlineIntensity(src.GetCRTScanlineIntensity());
        chain->SetGrainEnabled(src.GetGrainEnabled());
        chain->SetGrainIntensity(src.GetGrainIntensity());
        chain->SetFXAAEnabled(src.GetFXAAEnabled());
        chain->SetSaturationEnabled(src.GetSaturationEnabled());
        chain->SetSaturationAmount(src.GetSaturationAmount());
        chain->SetVignetteEnabled(src.GetVignetteEnabled());
        chain->SetVignetteIntensity(src.GetVignetteIntensity());
        chain->SetBlurEnabled(src.GetBlurEnabled());
        chain->SetBlurIntensity(src.GetBlurIntensity());
        chain->SetSharpenEnabled(src.GetSharpenEnabled());
        chain->SetSharpenIntensity(src.GetSharpenIntensity());
        chain->SetBloomEnabled(src.GetBloomEnabled());
        chain->SetBloomIntensity(src.GetBloomIntensity());
        chain->SetChromaticAberrationEnabled(src.GetChromaticAberrationEnabled());
        chain->SetChromaticAberrationIntensity(src.GetChromaticAberrationIntensity());
        chain->SetVHSEnabled(src.GetVHSEnabled());
        chain->SetVHSIntensity(src.GetVHSIntensity());
        chain->SetCineEnabled(src.GetCineEnabled());
        chain->SetCineIntensity(src.GetCineIntensity());
        chain->SetCineTint(src.GetCineTint());
        chain->SetContrastEnabled(src.GetContrastEnabled());
        chain->SetContrastAmount(src.GetContrastAmount());
        chain->SetLuminosityEnabled(src.GetLuminosityEnabled());
        chain->SetLuminosityAmount(src.GetLuminosityAmount());
        chain->SetTAAEnabled(src.GetTAAEnabled());
        chain->SetTAAIntensity(src.GetTAAIntensity());
        chain->SetGlitchEnabled(src.GetGlitchEnabled());
        chain->SetGlitchIntensity(src.GetGlitchIntensity());
        chain->SetGlitchSpeed(src.GetGlitchSpeed());
        chain->SetGlitchMode(src.GetGlitchMode());
        chain->SetColorGradingEnabled(src.GetColorGradingEnabled());
        chain->SetColorGradingIntensity(src.GetColorGradingIntensity());
        chain->SetColorGradingPreset(src.GetColorGradingPreset());
        chain->SetPixelateEnabled(src.GetPixelateEnabled());
        chain->SetPixelateSize(src.GetPixelateSize());
        chain->SetPixelateColorDepth(src.GetPixelateColorDepth());
        chain->SetRadialBlurEnabled(src.GetRadialBlurEnabled());
        chain->SetRadialBlurIntensity(src.GetRadialBlurIntensity());
        chain->SetWavesEnabled(src.GetWavesEnabled());
        chain->SetWavesIntensity(src.GetWavesIntensity());
        chain->SetWavesSpeed(src.GetWavesSpeed());
        chain->SetWavesFrequency(src.GetWavesFrequency());
        chain->SetMirrorEnabled(src.GetMirrorEnabled());
        chain->SetMirrorMode(src.GetMirrorMode());
        chain->SetThermalEnabled(src.GetThermalEnabled());
        chain->SetThermalIntensity(src.GetThermalIntensity());
        chain->SetThermalMode(src.GetThermalMode());
        chain->SetHalftoneEnabled(src.GetHalftoneEnabled());
        chain->SetHalftoneDotScale(src.GetHalftoneDotScale());
        chain->SetHalftoneMode(src.GetHalftoneMode());
        chain->SetVolumetricFogEnabled(src.GetVolumetricFogEnabled());
        chain->SetVolumetricFogDensity(src.GetVolumetricFogDensity());
        chain->SetVolumetricFogSpeed(src.GetVolumetricFogSpeed());
        chain->SetVolumetricFogScale(src.GetVolumetricFogScale());
        chain->SetVolumetricFogColorMode(src.GetVolumetricFogColorMode());
        chain->SetVolumetricCloudsEnabled(src.GetVolumetricCloudsEnabled());
        chain->SetVolumetricCloudsCoverage(src.GetVolumetricCloudsCoverage());
        chain->SetVolumetricCloudsDensity(src.GetVolumetricCloudsDensity());
        chain->SetVolumetricCloudsSpeed(src.GetVolumetricCloudsSpeed());
        chain->SetVolumetricCloudsSunIntensity(src.GetVolumetricCloudsSunIntensity());
        chain->SetZonedDistortionEnabled(src.GetZonedDistortionEnabled());
        chain->SetZonedDistortionIntensity(src.GetZonedDistortionIntensity());
        chain->SetZonedDistortionSpeed(src.GetZonedDistortionSpeed());
        chain->SetZonedDistortionZone(src.GetZonedDistortionZone());
        chain->SetZonedDistortionFeather(src.GetZonedDistortionFeather());

        map[id] = std::move(chain);
    }

    bool PresentationCore::IsExtraProjectorViewport(ImGuiID id) const {
        return m_Impl && id != 0 && m_Impl->extraCompositeFX.find(id) != m_Impl->extraCompositeFX.end();
    }

    void PresentationCore::RenderExtraProjectorViewportPostFX(ImGuiID id, ImGuiViewport* viewport,
                                                                void (*defaultRenderFn)(ImGuiViewport*, void*))
    {
        if (!m_Impl) { if (defaultRenderFn) defaultRenderFn(viewport, nullptr); return; }
        auto it = m_Impl->extraCompositeFX.find(id);
        if (it != m_Impl->extraCompositeFX.end())
            it->second->RenderViewport(viewport, defaultRenderFn);
        else if (defaultRenderFn)
            defaultRenderFn(viewport, nullptr);
    }

    void PresentationCore::PruneExtraProjectorViewports(const std::vector<ImGuiID>& stillActiveThisFrame) {
        if (!m_Impl) return;
        auto& map = m_Impl->extraCompositeFX;
        for (auto it = map.begin(); it != map.end(); ) {
            bool stillActive = std::find(stillActiveThisFrame.begin(), stillActiveThisFrame.end(), it->first)
                                != stillActiveThisFrame.end();
            it = stillActive ? std::next(it) : map.erase(it);
        }
    }

    void PresentationCore::SetStretchToFill(bool s) {
        m_stretchToFill = s;
        if (m_Impl)
            m_Impl->background.SetStretchToFill(s);
    }

    bool PresentationCore::GetStretchToFill() const {
        return m_Impl ? m_Impl->background.GetStretchToFill() : false;
    }

    void PresentationCore::Update() {
        if (m_Impl) {
            m_Impl->background.Update();
            m_Impl->preview.Update();
        }
    }

    void PresentationCore::RenderBackground(int outputW, int outputH) {
        if (m_Impl)
            m_Impl->background.Render(outputW, outputH);
    }

    void PresentationCore::RenderProjectorWindow() {
    if (m_Impl) {
        if (ShouldShowLoadingScreen()) {

            m_Impl->background.RenderLogo(
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                m_LoadingLogoW, m_LoadingLogoH, m_ProjectorWidth, m_ProjectorHeight);
        } else {
            m_Impl->background.Render(m_ProjectorWidth, m_ProjectorHeight);
        }
    }
}

    bool PresentationCore::CreateSecondaryWindow(const std::string& id, int monitorIndex,
                                                  const std::string& title,
                                                  SecondaryOutputWindow::RenderFn renderFn)
    {
        if (!m_MainWindow) {
            std::cerr << "[PresentationCore] CreateSecondaryWindow('" << id
                      << "'): falta SetMainWindow() previo.\n";
            return false;
        }

        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);

        SecondaryOutput& out = m_SecondaryWindows[id];
        if (!out.window.Create(m_MainWindow, monitorIndex, title))
        {
            m_SecondaryWindows.erase(id);
            return false;
        }
        out.renderFn = std::move(renderFn);
        return true;
    }

    void PresentationCore::DestroySecondaryWindow(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        m_SecondaryWindows.erase(id);
    }

    void PresentationCore::DestroyAllSecondaryWindows()
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        m_SecondaryWindows.clear();
    }

    bool PresentationCore::IsSecondaryWindowActive(const std::string& id) const
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        auto it = m_SecondaryWindows.find(id);
        return it != m_SecondaryWindows.end() && it->second.window.IsActive();
    }

    void PresentationCore::RenderAllSecondaryWindows()
    {

        std::vector<SecondaryOutput*> active;
        {
            std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
            active.reserve(m_SecondaryWindows.size());
            for (auto& [id, out] : m_SecondaryWindows)
                if (out.window.IsActive())
                    active.push_back(&out);
        }

        for (auto* out : active)
            out->window.RenderFrame(out->renderFn);
    }

    bool PresentationCore::CreateProjectorWindow(int monitorIndex)
    {
#ifdef _WIN32

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = monitorIndex;
        return false;
#else
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (monitorIndex >= 0 && monitorIndex < monitorCount) {
            if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[monitorIndex])) {
                SetProjectorSize(vm->width, vm->height);
            }
        }

        bool ok = CreateSecondaryWindow(kProjectorId, monitorIndex, "ProyecThor - Proyector",
            [this](int w, int h) {
                SetProjectorSize(w, h);
                if (m_ProjectorRenderFn) {
                    m_ProjectorRenderFn(w, h);
                } else {
                    RenderProjectorWindow();
                }
            });

        if (ok) {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.targetMonitorIndex = monitorIndex;
        }
        return ok;
#endif
    }

    void PresentationCore::DestroyProjectorWindow()
    {
        DestroySecondaryWindow(kProjectorId);
    }

    bool PresentationCore::IsProjectorWindowActive() const
    {
        return IsSecondaryWindowActive(kProjectorId);
    }

    GLFWwindow* PresentationCore::GetProjectorWindow() const
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        auto it = m_SecondaryWindows.find(kProjectorId);
        return (it != m_SecondaryWindows.end()) ? it->second.window.GetWindow() : nullptr;
    }

    bool PresentationCore::CreateStageWindow(int monitorIndex)
    {
#ifdef _WIN32

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.stageMonitorIndex = monitorIndex;
        return false;
#else
        bool ok = CreateSecondaryWindow(kStageId, monitorIndex, "ProyecThor - Stage Display",
            [this](int w, int h) {
                if (m_StageRenderFn) {
                    m_StageRenderFn(w, h);
                }
            });

        if (ok) {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.stageMonitorIndex = monitorIndex;
        }
        return ok;
#endif
    }

    void PresentationCore::DestroyStageWindow()
    {
        DestroySecondaryWindow(kStageId);
    }

    bool PresentationCore::IsStageWindowActive() const
    {
        return IsSecondaryWindowActive(kStageId);
    }

    GLFWwindow* PresentationCore::GetStageWindow() const
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        auto it = m_SecondaryWindows.find(kStageId);
        return (it != m_SecondaryWindows.end()) ? it->second.window.GetWindow() : nullptr;
    }

    void PresentationCore::SetStageAlertMessage(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(m_StageAlertMutex);
        m_StageAlertMessage = msg;
    }

    std::string PresentationCore::GetStageAlertMessage() const
    {
        std::lock_guard<std::mutex> lock(m_StageAlertMutex);
        return m_StageAlertMessage;
    }

    void PresentationCore::ClearStageAlertMessage()
    {
        std::lock_guard<std::mutex> lock(m_StageAlertMutex);
        m_StageAlertMessage.clear();
    }

void PresentationCore::SetBgTypeLocked(PresentationState::BackgroundType newType)
{
    if (m_State.bgType == PresentationState::BackgroundType::Audio &&
        newType != PresentationState::BackgroundType::Audio &&
        m_AudioPanelRef)
    {
        m_AudioPanelRef->SetLiveBackground(false);
    }
    m_State.bgType = newType;
}

void PresentationCore::SetBackgroundMedia(const std::string& path, bool , bool allowAudio) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.bgPath = path;
        SetBgTypeLocked(PresentationState::BackgroundType::Video);
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }
    if (m_Impl)
        m_Impl->background.SetVideo(path, allowAudio);
}

bool PresentationCore::GetContentAllowsAudio() const {
    if (m_Impl)
        return m_Impl->background.GetContentAllowsAudio();
    return false;
}

void PresentationCore::StopBackgroundMedia() {
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.bgPath     = "";
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
        m_State.bgColor[0] = 0.0f; m_State.bgColor[1] = 0.0f; m_State.bgColor[2] = 0.0f;
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }
    if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
}

void PresentationCore::SetBackgroundAudio() {
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.bgPath = "";
        SetBgTypeLocked(PresentationState::BackgroundType::Audio);
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }

    if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
}

    void PresentationCore::SetOverlayMedia(const std::string& pngPath) {
        if (!m_Impl) return;

        int w = 0, h = 0, n = 0;
        unsigned char* data = stbi_load(pngPath.c_str(), &w, &h, &n, 4);
        if (!data) return;

        if (m_Impl->overlayTex) {
            GLuint old = m_Impl->overlayTex;
            glDeleteTextures(1, &old);
            m_Impl->overlayTex = 0;
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        m_Impl->overlayTex  = tex;
        m_Impl->overlayTexW = w;
        m_Impl->overlayTexH = h;

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_OverlayPath = pngPath;
        m_HasOverlay  = true;
    }

    void PresentationCore::ClearOverlay() {
        if (m_Impl && m_Impl->overlayTex) {
            GLuint old = m_Impl->overlayTex;
            glDeleteTextures(1, &old);
            m_Impl->overlayTex  = 0;
            m_Impl->overlayTexW = 0;
            m_Impl->overlayTexH = 0;
        }
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_OverlayPath.clear();
        m_HasOverlay = false;
    }

    bool PresentationCore::HasOverlay() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_HasOverlay;
    }

    std::string PresentationCore::GetOverlayPath() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_OverlayPath;
    }

    void* PresentationCore::GetOverlayTexture() {
        if (!m_Impl || !m_Impl->overlayTex) return nullptr;
        return (void*)(intptr_t)m_Impl->overlayTex;
    }

    void PresentationCore::SetOverlayClockLayer(bool hasClock, const ProyecThor::UI::OverlayLayer& layer,
                                                 int canvasW, int canvasH) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_HasOverlayClockLayer = hasClock;
        m_OverlayClockLayer    = layer;
        m_OverlayClockCanvasW  = canvasW;
        m_OverlayClockCanvasH  = canvasH;
    }

    bool PresentationCore::HasOverlayClockLayer() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_HasOverlayClockLayer;
    }

    ProyecThor::UI::OverlayLayer PresentationCore::GetOverlayClockLayer() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_OverlayClockLayer;
    }

    int PresentationCore::GetOverlayClockCanvasW() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_OverlayClockCanvasW;
    }

    int PresentationCore::GetOverlayClockCanvasH() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_OverlayClockCanvasH;
    }

    void PresentationCore::SetLiveOverlayClockText(const std::string& text, const float* colorOverride) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_LiveOverlayClockText = text;
        m_HasLiveOverlayClockColorOverride = (colorOverride != nullptr);
        if (colorOverride) {
            for (int i = 0; i < 4; i++) m_LiveOverlayClockColorOverride[i] = colorOverride[i];
        }
    }

    std::string PresentationCore::GetLiveOverlayClockText() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_LiveOverlayClockText;
    }

    bool PresentationCore::HasLiveOverlayClockColorOverride() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_HasLiveOverlayClockColorOverride;
    }

    void PresentationCore::GetLiveOverlayClockColorOverride(float outRGBA[4]) const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (int i = 0; i < 4; i++) outRGBA[i] = m_LiveOverlayClockColorOverride[i];
    }

    void PresentationCore::PreloadNextBackgroundMedia(const std::string& path, bool allowAudio) {

        if (m_Impl) m_Impl->background.Prefetch(path, allowAudio);
    }

    void PresentationCore::CommitNextBackgroundMedia(const std::string& path, bool , bool allowAudio) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.bgPath = path;
            SetBgTypeLocked(PresentationState::BackgroundType::Video);
            ++m_State.transitionTrigger;
            ++m_StreamVersion;
        }
        if (m_Impl)
            m_Impl->background.CommitPrefetch(path, allowAudio);
    }

    bool PresentationCore::IsBackgroundSwapPending() const {
        return m_Impl && m_Impl->background.IsSwapPending();
    }

    float PresentationCore::GetBackgroundSwapEta() const {
        return m_Impl ? m_Impl->background.GetEstimatedLoadSeconds() : 0.0f;
    }

    void* PresentationCore::GetStandbyBackgroundTexture() {
        return m_Impl ? m_Impl->background.GetStandbyTextureID() : nullptr;
    }

    void* PresentationCore::GetPreviewStandbyBackgroundTexture(int targetW, int targetH) {
        if (!m_Impl) return nullptr;
        void* rawTex = m_Impl->background.GetStandbyTextureID();
        if (!rawTex) return nullptr;

        GLuint raw = static_cast<GLuint>(reinterpret_cast<uintptr_t>(rawTex));
        GLuint processed = m_Impl->compositeFX.ProcessBackgroundForPreview(raw, targetW, targetH);
        return (void*)(uintptr_t)processed;
    }

    float PresentationCore::GetBackgroundBlendProgress() const {
        return m_Impl ? m_Impl->background.GetTransitionProgress() : 1.0f;
    }

    bool PresentationCore::IsBackgroundStandbyReady() {
        return m_Impl && m_Impl->background.StandbyHasFrame();
    }

    int PresentationCore::GetBackgroundTransitionType() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.transitionType;
    }

    void PresentationCore::SetLoadingLogoPath(const std::string& path) {
        if (path == m_LoadingLogoPath) return;

        if (m_LoadingLogoTex != 0) {
            GLuint old = m_LoadingLogoTex;
            glDeleteTextures(1, &old);
            m_LoadingLogoTex = 0;
        }
        m_LoadingLogoPath = path;
        m_LoadingLogoW = 0;
        m_LoadingLogoH = 0;

        if (path.empty()) return;

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data) {
            std::cerr << "[PresentationCore] No se pudo cargar el logo: " << path << "\n";
            return;
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        m_LoadingLogoTex = tex;
        m_LoadingLogoW   = w;
        m_LoadingLogoH   = h;
    }

    void* PresentationCore::GetLoadingLogoTexture() const {
        return m_LoadingLogoTex != 0 ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_LoadingLogoTex)) : nullptr;
    }

    unsigned int PresentationCore::GetBoxBgTexture(bool isLyrics, const std::string& path) {
        std::string& cachedPath = isLyrics ? m_LyricsBgTexPath : m_IndexBgTexPath;
        GLuint&      cachedTex  = isLyrics ? m_LyricsBgTex     : m_IndexBgTex;

        if (path == cachedPath) return cachedTex;

        if (cachedTex != 0) {
            GLuint old = cachedTex;
            glDeleteTextures(1, &old);
            cachedTex = 0;
        }
        cachedPath = path;
        if (path.empty()) return 0;

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data) return 0;

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        cachedTex = tex;
        return tex;
    }

    bool PresentationCore::ShouldShowLoadingScreen() const {

        return false;
    }

    void PresentationCore::BlockBackgroundPath(const std::string& path) {
        if (m_Impl) m_Impl->background.BlockPath(path);
    }

    void PresentationCore::UnblockBackgroundPath() {
        if (m_Impl) m_Impl->background.UnblockPath();
    }

void PresentationCore::SetLayer0_Color(float r, float g, float b) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.bgColor[0] = r; m_State.bgColor[1] = g; m_State.bgColor[2] = b;
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
        m_State.bgPath     = "";
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }
    if (m_Impl) m_Impl->background.SetSolidColor(r, g, b);
}
void PresentationCore::SetBackgroundTransitionProgress(float progress) {
    if (m_Impl) m_Impl->background.SetTransitionProgress(progress);
}
void PresentationCore::SetBackgroundBlendDuration(float seconds) {
    if (m_Impl) m_Impl->background.SetBlendSeconds(seconds);
}

    static void ApplyLyricsBoxToState(const TextBoxStyle& box, PresentationState& state,
                                       std::string& activeFontName);

    void PresentationCore::UpdateLyricsBoxStyle(const TextBoxStyle& box) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        ApplyLyricsBoxToState(box, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::UpdateIndexBoxStyle(const TextBoxStyle& box, bool enabled) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.indexBox     = box;
        m_State.indexEnabled = enabled;
        ++m_StreamVersion;
    }

    void PresentationCore::SetCurrentRef(const std::string& ref) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.currentRef = ref;
        ++m_StreamVersion;
    }

    TextEffectsData PresentationCore::GetTextEffects() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.effects;
    }

void PresentationCore::SetLayer2_Text(const std::string& text, int slideIndex, bool triggerTransition) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.currentText = text;
    m_State.showText    = !text.empty();

    m_State.currentRef.clear();
    if (slideIndex >= 0) {
        m_ActiveSlideIndex = slideIndex;
    } else if (m_CurrentSelection.type == ItemType::Song && !text.empty()) {
        for (size_t i = 0; i < m_CurrentSelection.contentData.size(); ++i) {
            if (m_CurrentSelection.contentData[i] == text) {
                m_ActiveSlideIndex = static_cast<int>(i);
                break;
            }
        }
    }
    if (triggerTransition) {
        ++m_State.textTransitionTrigger;
    }
    ++m_StreamVersion;
}

void PresentationCore::ClearLayer2() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.currentText = "";
    m_State.currentRef  = "";
    m_State.showText    = false;
    m_State.nextText    = "";
    m_ActiveSlideIndex  = -1;
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

int PresentationCore::GetActiveSlideIndex() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_ActiveSlideIndex;
}

void PresentationCore::SetActiveSlideIndex(int index) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_ActiveSlideIndex = index;
}

void PresentationCore::UpdateCurrentSelectionStanza(int stanzaIndex, const std::string& newText) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (m_CurrentSelection.type == ItemType::Song &&
        stanzaIndex >= 0 && stanzaIndex < static_cast<int>(m_CurrentSelection.contentData.size()))
    {
        m_CurrentSelection.contentData[stanzaIndex] = newText;
        ++m_StreamVersion;
    }
}

void PresentationCore::SetClockStyleCue(const std::string& styleName) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_PendingClockStyleCue = styleName;
    m_HasClockStyleCue     = true;
}

std::string PresentationCore::ConsumeClockStyleCue() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_HasClockStyleCue) return {};
    m_HasClockStyleCue = false;
    return m_PendingClockStyleCue;
}

void PresentationCore::SetPendingTransitionOverride(const std::string& name, float duration) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_PendingTransitionName     = name;
    m_PendingTransitionDuration = duration;
    m_HasTransitionOverride     = true;
}

bool PresentationCore::ConsumePendingTransitionOverride(std::string& outName, float& outDuration) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_HasTransitionOverride) return false;
    m_HasTransitionOverride = false;
    outName     = m_PendingTransitionName;
    outDuration = m_PendingTransitionDuration;
    return true;
}

void PresentationCore::RequestSongEditorOpen(const std::string& filename) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_PendingSongEditorOpenFile = filename;
    m_HasSongEditorOpenRequest  = true;
}

bool PresentationCore::ConsumeSongEditorOpenRequest(std::string& outFilename) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_HasSongEditorOpenRequest) return false;
    m_HasSongEditorOpenRequest = false;
    outFilename = m_PendingSongEditorOpenFile;
    return true;
}

void PresentationCore::SetNextText(const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.nextText = text;
    ++m_StreamVersion;
}

    void PresentationCore::SetProjecting(bool projecting) {
        int monitorIndex;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.isProjecting = projecting;
            ++m_StreamVersion;
            monitorIndex = m_State.targetMonitorIndex;
        }

        if (m_Impl)
            m_Impl->background.SetPubliclyLive(projecting, monitorIndex);
    }

    bool PresentationCore::IsProjecting() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.isProjecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        bool projecting = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.targetMonitorIndex = index;
            m_State.extraTargetMonitors =
                ProyecThor::Settings::SettingsManager::Get().GetSettings().projection.extraMonitors;
            projecting = m_State.isProjecting;
        }
        if (m_Impl) {
            m_Impl->background.SetPubliclyLive(projecting, index);
        }
    }

    void PresentationCore::SetStaging(bool active, int monitorIndex) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.isStaging = active;
        if (monitorIndex >= 0)
            m_State.stageMonitorIndex = monitorIndex;
        if (active)
            m_State.extraStageMonitors =
                ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay.extraMonitors;
        ++m_StreamVersion;
    }

    bool PresentationCore::IsStaging() const {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.isStaging;
    }

    void PresentationCore::SetProjectorSize(int w, int h) {
        m_ProjectorWidth  = w;
        m_ProjectorHeight = h;
    }

    VLCBasePlayer* PresentationCore::GetBackgroundPlayer() {
        return m_Impl ? m_Impl->background.GetPlayer() : nullptr;
    }

    void PresentationCore::GetBackgroundVideoSize(int& width, int& height) {
        if (m_Impl)
            m_Impl->background.GetActiveVideoSize(width, height);
        else
            width = height = 0;
    }

    float PresentationCore::GetLivePosition() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.livePosition;
    }

    void PresentationCore::SetLivePosition(float pos) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.livePosition = pos;
        }
        if (m_Impl) {
            VLCBasePlayer* player = m_Impl->background.GetPlayer();
            if (player) player->SetPosition(pos);
            m_Impl->background.SeekSync(pos);
        }
    }

    int PresentationCore::GetLiveVolume() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.liveVolume;
    }

    bool PresentationCore::GetLiveMute() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.liveMuted;
    }

    void PresentationCore::SetLiveVolume(int volume) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.liveVolume = volume;
        }
        if (m_Impl)
            m_Impl->background.SetLiveVolume(volume);
    }

    void PresentationCore::SetLiveMute(bool mute) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_State.liveMuted = mute;
        }
        if (m_Impl)
            m_Impl->background.SetLiveMute(mute);
    }

    void PresentationCore::SetLiveEqualizerEnabled(bool enabled) {
        if (m_Impl)
            m_Impl->background.SetLiveEqualizerEnabled(enabled);
    }

    void PresentationCore::SetLiveEqualizerPreamp(float preampDb) {
        if (m_Impl)
            m_Impl->background.SetLiveEqualizerPreamp(preampDb);
    }

    void PresentationCore::SetLiveEqualizerBand(int index, float ampDb) {
        if (m_Impl)
            m_Impl->background.SetLiveEqualizerBand(index, ampDb);
    }

    bool PresentationCore::GetLiveLoop() {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_State.liveLoop;
    }

    void PresentationCore::SetLiveLoop(bool loop) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_State.liveLoop = loop;
    }
void PresentationCore::SetTransitionConfig(int type, float durationSeconds) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_State.transitionType     = type;
    m_State.transitionDuration = std::max(0.05f, durationSeconds);
}
    static const ImWchar kProjectionGlyphRanges[] = {
        0x0020, 0x00FF,
        0x0100, 0x024F,
        0x0370, 0x03FF,
        0x0400, 0x052F,
        0x2000, 0x206F,
        0x20A0, 0x20CF,
        0x2100, 0x214F,
        0x2190, 0x21FF,
        0x2200, 0x22FF,
        0x25A0, 0x25FF,
        0x2600, 0x26FF,
        0x2700, 0x27BF,
        0x2B00, 0x2BFF,
        0x1F300, 0x1F5FF,
        0x1F600, 0x1F64F,
        0x1F680, 0x1F6FF,
        0x1F900, 0x1F9FF,
        0
    };

    void PresentationCore::LoadFontsIntoImGui() {
        ImGuiIO& io = ImGui::GetIO();
        m_ImGuiFonts["Predeterminada"] = io.Fonts->AddFontDefault();

        const float baseFontSize = 60.0f;
        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";

        try {
            if (std::filesystem::exists(fontsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(fontsDir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc") {
                        std::string fontName = entry.path().stem().string();
                        std::string fullPath = entry.path().string();
                        ImFont* font = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), baseFontSize, nullptr, kProjectionGlyphRanges);
                        if (font)
                            m_ImGuiFonts[fontName] = font;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PresentationCore] Error cargando fuentes: " << e.what() << "\n";
        }
    }

    void PresentationCore::LoadSingleFontIntoImGui(const std::string& fontPath) {
        ImGuiIO& io = ImGui::GetIO();
        const float baseFontSize = 60.0f;

        std::filesystem::path p(fontPath);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") return;

        std::string fontName = p.stem().string();
        if (m_ImGuiFonts.count(fontName)) return;

        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize, nullptr, kProjectionGlyphRanges);
        if (font)
            m_ImGuiFonts[fontName] = font;
    }

    static std::string ThemesDirPath()
    {
        std::filesystem::path dir;

#ifdef _WIN32
        wchar_t buf[MAX_PATH] = {};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
        dir = std::filesystem::path(buf) / "ProyecThor" / "themes";
#else
        const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
        std::filesystem::path base;
        if (xdgConfig && *xdgConfig)
        {
            base = std::filesystem::path(xdgConfig);
        }
        else
        {
            const char* home = std::getenv("HOME");
            base = std::filesystem::path(home ? home : ".") / ".config";
        }
        dir = base / "ProyecThor" / "themes";
#endif

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir.string();
    }

    std::string PackTextEffects(const TextEffectsData& e)
    {
        std::ostringstream ss;
        auto put = [&](float v) { ss << v << ","; };
        put(e.bgEnabled ? 1.0f : 0.0f);
        for (float c : e.bgColor) put(c);
        put(e.borderEnabled ? 1.0f : 0.0f);
        for (float c : e.borderColor) put(c);
        put(e.borderWidth);
        put(e.shadowEnabled ? 1.0f : 0.0f);
        for (float c : e.shadowColor) put(c);
        put(e.shadowIntensity);
        put(e.chromaticAberrationEnabled ? 1.0f : 0.0f);
        put(e.chromaticAberrationIntensity);
        put(e.glowEnabled ? 1.0f : 0.0f);
        for (float c : e.glowColor) put(c);
        put(e.glowIntensity);
        put(e.neonEnabled ? 1.0f : 0.0f);
        for (float c : e.neonColor) put(c);
        put(e.neonIntensity);
        put(e.underlineEnabled ? 1.0f : 0.0f);
        for (float c : e.underlineColor) put(c);
        put(e.underlineThickness);

        put(e.text3dEnabled ? 1.0f : 0.0f);
        for (float c : e.text3dColor) put(c);
        put(e.text3dDepth);

        put(e.gradientEnabled ? 1.0f : 0.0f);
        for (float c : e.gradientColorA) put(c);
        for (float c : e.gradientColorB) put(c);
        put(e.gradientAngle);

        put(e.opacityGradientEnabled ? 1.0f : 0.0f);
        put(e.opacityGradientAngle);
        ss << e.opacityGradientStrength;
        return ss.str();
    }

    void UnpackTextEffects(const std::string& v, TextEffectsData& e)
    {
        std::vector<float> f;
        std::stringstream ss(v);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (!tok.empty()) f.push_back(std::stof(tok));
        }
        if (f.size() < 37) return;

        size_t i = 0;
        e.bgEnabled = f[i++] != 0.0f;
        for (float& c : e.bgColor) c = f[i++];
        e.borderEnabled = f[i++] != 0.0f;
        for (float& c : e.borderColor) c = f[i++];
        e.borderWidth = f[i++];
        e.shadowEnabled = f[i++] != 0.0f;
        for (float& c : e.shadowColor) c = f[i++];
        e.shadowIntensity = f[i++];
        e.chromaticAberrationEnabled = f[i++] != 0.0f;
        e.chromaticAberrationIntensity = f[i++];
        e.glowEnabled = f[i++] != 0.0f;
        for (float& c : e.glowColor) c = f[i++];
        e.glowIntensity = f[i++];
        e.neonEnabled = f[i++] != 0.0f;
        for (float& c : e.neonColor) c = f[i++];
        e.neonIntensity = f[i++];
        e.underlineEnabled = f[i++] != 0.0f;
        for (float& c : e.underlineColor) c = f[i++];
        e.underlineThickness = f[i++];

        if (f.size() >= i + 19) {
            e.text3dEnabled = f[i++] != 0.0f;
            for (float& c : e.text3dColor) c = f[i++];
            e.text3dDepth = f[i++];

            e.gradientEnabled = f[i++] != 0.0f;
            for (float& c : e.gradientColorA) c = f[i++];
            for (float& c : e.gradientColorB) c = f[i++];
            e.gradientAngle = f[i++];

            e.opacityGradientEnabled = f[i++] != 0.0f;
            e.opacityGradientAngle = f[i++];
            e.opacityGradientStrength = f[i++];
        }
    }

    static void BoxFromLegacyMargins(const float margins[4], TextBoxStyle& box)
    {
        box.sizeW = std::max(0.02f, (1920.0f - margins[0] - margins[2]) / 1920.0f);
        box.sizeH = std::max(0.02f, (1080.0f - margins[1] - margins[3]) / 1080.0f);
        box.posX  = margins[0] / 1920.0f + box.sizeW * 0.5f;
        box.posY  = margins[1] / 1080.0f + box.sizeH * 0.5f;
    }

    static void WriteBoxKeys(std::ofstream& f, const char* prefix, const TextBoxStyle& box)
    {
        f << prefix << "PosX="    << box.posX  << "\n";
        f << prefix << "PosY="    << box.posY  << "\n";
        f << prefix << "SizeW="   << box.sizeW << "\n";
        f << prefix << "SizeH="   << box.sizeH << "\n";
        f << prefix << "Font="    << box.fontName << "\n";
        f << prefix << "Color="   << box.color[0] << "," << box.color[1] << ","
                                   << box.color[2] << "," << box.color[3] << "\n";
        f << prefix << "Size="    << box.textSize << "\n";
        f << prefix << "HAlign="  << box.hAlign << "\n";
        f << prefix << "VAlign="  << box.vAlign << "\n";
        f << prefix << "AutoScale=" << (box.autoScale ? 1 : 0) << "\n";
        f << prefix << "BgMediaEnabled=" << (box.bgMediaEnabled ? 1 : 0) << "\n";
        f << prefix << "BgMediaPath="    << box.bgMediaPath << "\n";
        f << prefix << "BgMediaOpacity=" << box.bgMediaOpacity << "\n";
        f << prefix << "Effects=" << PackTextEffects(box.effects) << "\n";
    }

    static bool LoadThemeFromDisk(const std::string& themesDir,
                                   const std::string& name,
                                   SavedStyle& out)
    {
        std::filesystem::path p =
            std::filesystem::path(themesDir) / (name + ".theme");
        std::ifstream f(p);
        if (!f.is_open()) return false;

        out      = SavedStyle{};
        out.name = name;

        bool hasLyricsBoxKeys = false;
        bool hasIndexBoxKeys  = false;

        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto sep = line.find('=');
            if (sep == std::string::npos) continue;
            std::string k = line.substr(0, sep);
            std::string v = line.substr(sep + 1);

            if      (k == "textSize")   out.size      = std::stof(v);
            else if (k == "textAlign")  out.hAlign    = std::stoi(v);
            else if (k == "vAlign")     out.vAlign    = std::stoi(v);
            else if (k == "autoScale")  out.autoScale = (std::stoi(v) != 0);
            else if (k == "font")       out.fontName  = v;
            else if (k == "textColor")
                sscanf(v.c_str(), "%f,%f,%f,%f",
                       &out.color[0], &out.color[1],
                       &out.color[2], &out.color[3]);
            else if (k == "margins")
                sscanf(v.c_str(), "%f,%f,%f,%f",
                       &out.margins[0], &out.margins[1],
                       &out.margins[2], &out.margins[3]);

            else if (k == "lyricsPosX")    { out.lyrics.posX  = std::stof(v); hasLyricsBoxKeys = true; }
            else if (k == "lyricsPosY")    out.lyrics.posY    = std::stof(v);
            else if (k == "lyricsSizeW")   out.lyrics.sizeW   = std::stof(v);
            else if (k == "lyricsSizeH")   out.lyrics.sizeH   = std::stof(v);
            else if (k == "lyricsFont")    out.lyrics.fontName = v;
            else if (k == "lyricsColor")
                sscanf(v.c_str(), "%f,%f,%f,%f", &out.lyrics.color[0], &out.lyrics.color[1],
                       &out.lyrics.color[2], &out.lyrics.color[3]);
            else if (k == "lyricsSize")    out.lyrics.textSize = std::stof(v);
            else if (k == "lyricsHAlign")  out.lyrics.hAlign   = std::stoi(v);
            else if (k == "lyricsVAlign")  out.lyrics.vAlign   = std::stoi(v);
            else if (k == "lyricsAutoScale") out.lyrics.autoScale = (std::stoi(v) != 0);
            else if (k == "lyricsBgMediaEnabled") out.lyrics.bgMediaEnabled = (std::stoi(v) != 0);
            else if (k == "lyricsBgMediaPath")    out.lyrics.bgMediaPath = v;
            else if (k == "lyricsBgMediaOpacity") out.lyrics.bgMediaOpacity = std::stof(v);
            else if (k == "lyricsEffects") UnpackTextEffects(v, out.lyrics.effects);

            else if (k == "indexPosX")     { out.index.posX  = std::stof(v); hasIndexBoxKeys = true; }
            else if (k == "indexPosY")     out.index.posY    = std::stof(v);
            else if (k == "indexSizeW")    out.index.sizeW   = std::stof(v);
            else if (k == "indexSizeH")    out.index.sizeH   = std::stof(v);
            else if (k == "indexFont")     out.index.fontName = v;
            else if (k == "indexColor")
                sscanf(v.c_str(), "%f,%f,%f,%f", &out.index.color[0], &out.index.color[1],
                       &out.index.color[2], &out.index.color[3]);
            else if (k == "indexSize")     out.index.textSize = std::stof(v);
            else if (k == "indexHAlign")   out.index.hAlign   = std::stoi(v);
            else if (k == "indexVAlign")   out.index.vAlign   = std::stoi(v);
            else if (k == "indexAutoScale") out.index.autoScale = (std::stoi(v) != 0);
            else if (k == "indexBgMediaEnabled") out.index.bgMediaEnabled = (std::stoi(v) != 0);
            else if (k == "indexBgMediaPath")    out.index.bgMediaPath = v;
            else if (k == "indexBgMediaOpacity") out.index.bgMediaOpacity = std::stof(v);
            else if (k == "indexEffects")  UnpackTextEffects(v, out.index.effects);
            else if (k == "indexEnabled")  out.indexEnabled = (std::stoi(v) != 0);

            else if (k == "textEffects")
                UnpackTextEffects(v, out.effects);
        }

        if (!hasLyricsBoxKeys) {
            out.lyrics.fontName  = out.fontName;
            out.lyrics.textSize  = out.size;
            for (int i = 0; i < 4; i++) out.lyrics.color[i] = out.color[i];
            out.lyrics.hAlign    = out.hAlign;
            out.lyrics.vAlign    = out.vAlign;
            out.lyrics.autoScale = out.autoScale;
            out.lyrics.effects   = out.effects;
            BoxFromLegacyMargins(out.margins, out.lyrics);
        }
        if (!hasIndexBoxKeys) out.index = out.lyrics;

        return true;
    }

    void PresentationCore::SaveStyle(const SavedStyle& style)
    {
        std::string dir = ThemesDirPath();
        std::ofstream f(std::filesystem::path(dir) / (style.name + ".theme"));
        if (!f.is_open()) return;

        f << "textColor="  << style.lyrics.color[0] << "," << style.lyrics.color[1] << ","
                            << style.lyrics.color[2] << "," << style.lyrics.color[3] << "\n";
        f << "textSize="   << style.lyrics.textSize << "\n";
        f << "textAlign="  << style.lyrics.hAlign   << "\n";
        f << "vAlign="     << style.lyrics.vAlign   << "\n";
        float legacyMargins[4] = {
            (style.lyrics.posX - style.lyrics.sizeW * 0.5f) * 1920.0f,
            (style.lyrics.posY - style.lyrics.sizeH * 0.5f) * 1080.0f,
            (1.0f - (style.lyrics.posX + style.lyrics.sizeW * 0.5f)) * 1920.0f,
            (1.0f - (style.lyrics.posY + style.lyrics.sizeH * 0.5f)) * 1080.0f,
        };
        f << "margins="    << legacyMargins[0] << "," << legacyMargins[1] << ","
                            << legacyMargins[2] << "," << legacyMargins[3] << "\n";
        f << "autoScale="  << (style.lyrics.autoScale ? 1 : 0) << "\n";
        f << "font="       << style.lyrics.fontName << "\n";
        f << "textEffects=" << PackTextEffects(style.lyrics.effects) << "\n";

        WriteBoxKeys(f, "lyrics", style.lyrics);
        WriteBoxKeys(f, "index",  style.index);
        f << "indexEnabled=" << (style.indexEnabled ? 1 : 0) << "\n";

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_SavedStyles[style.name] = style;
    }

    void PresentationCore::DeleteStyle(const std::string& name)
    {
        std::error_code ec;
        std::filesystem::remove(
            std::filesystem::path(ThemesDirPath()) / (name + ".theme"), ec);

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_SavedStyles.erase(name);
    }

    std::vector<std::string> PresentationCore::GetSavedStyleNames() const
    {
        std::string dir = ThemesDirPath();
        std::vector<std::string> names;
        try {
            for (const auto& e : std::filesystem::directory_iterator(dir))
                if (e.path().extension() == ".theme")
                    names.push_back(e.path().stem().string());
        } catch (...) {}
        std::sort(names.begin(), names.end());
        return names;
    }

    bool PresentationCore::GetSavedStyle(const std::string& name, SavedStyle& outStyle) const
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            auto it = m_SavedStyles.find(name);
            if (it != m_SavedStyles.end()) {
                outStyle = it->second;
                return true;
            }
        }
        return LoadThemeFromDisk(ThemesDirPath(), name, outStyle);
    }

    static std::string CategoryStylesFilePath()
    {
        return ProyecThor::GetAssetsPath() + "/../category_styles.ini";
    }

    static void ApplyLyricsBoxToState(const TextBoxStyle& box, PresentationState& state,
                                       std::string& activeFontName)
    {
        state.lyricsBox      = box;
        state.textSize       = box.textSize;
        state.textAlignment  = box.hAlign;
        state.vAlignment     = box.vAlign;
        state.autoScale      = box.autoScale;
        state.selectedFont   = box.fontName;
        state.effects        = box.effects;
        activeFontName       = box.fontName;
        for (int i = 0; i < 4; i++) state.textColor[i] = box.color[i];

        state.margins[0] = (box.posX - box.sizeW * 0.5f) * 1920.0f;
        state.margins[1] = (box.posY - box.sizeH * 0.5f) * 1080.0f;
        state.margins[2] = (1.0f - (box.posX + box.sizeW * 0.5f)) * 1920.0f;
        state.margins[3] = (1.0f - (box.posY + box.sizeH * 0.5f)) * 1080.0f;
    }

    static void ApplySavedStyleToState(const SavedStyle& s, PresentationState& state,
                                        std::string& activeFontName)
    {
        ApplyLyricsBoxToState(s.lyrics, state, activeFontName);
        state.indexBox     = s.index;
        state.indexEnabled = s.indexEnabled;
    }

    void PresentationCore::SetCategoryDefaultStyle(ItemType category, const std::string& styleName)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_CategoryDefaultStyles[static_cast<int>(category)] = styleName;
        }
        SaveCategoryStyles();
    }

    std::string PresentationCore::GetCategoryDefaultStyle(ItemType category) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        auto it = m_CategoryDefaultStyles.find(static_cast<int>(category));
        if (it != m_CategoryDefaultStyles.end())
            return it->second;
        return {};
    }

    void PresentationCore::LoadCategoryStyles()
    {
        std::ifstream f(CategoryStylesFilePath());
        if (!f.is_open()) return;

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto sep = line.find('=');
            if (sep == std::string::npos) continue;
            int         key = std::stoi(line.substr(0, sep));
            std::string val = line.substr(sep + 1);
            if (!val.empty())
                m_CategoryDefaultStyles[key] = val;
        }
    }

    void PresentationCore::SaveCategoryStyles() const
    {
        std::ofstream f(CategoryStylesFilePath());
        if (!f.is_open()) return;

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (const auto& pair : m_CategoryDefaultStyles) {
            if (!pair.second.empty())
                f << pair.first << "=" << pair.second << "\n";
        }
    }

    void PresentationCore::SyncFontListFromDisk(std::vector<std::string>& outList) {
        outList.clear();
        outList.push_back("Predeterminada");

        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";
        try {
            if (std::filesystem::exists(fontsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(fontsDir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc")
                        outList.push_back(entry.path().stem().string());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PresentationCore] Error sincronizando fuentes: " << e.what() << "\n";
        }
    }

    std::string PresentationCore::GetActiveFontName() const {
        return m_ActiveFontName;
    }

    ImFont* PresentationCore::GetImGuiFont(const std::string& fontName, float ) {
        auto it = m_ImGuiFonts.find(fontName);
        if (it != m_ImGuiFonts.end())
            return it->second;

        auto def = m_ImGuiFonts.find("Predeterminada");
        if (def != m_ImGuiFonts.end()) return def->second;
        return nullptr;
    }

    std::string PresentationCore::ResolveFontFilePath(const std::string& fontName) const
    {
        if (fontName.empty() || fontName == "Predeterminada") return "";

        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";
        for (const char* ext : { ".ttf", ".otf", ".ttc" }) {
            std::filesystem::path candidate =
                std::filesystem::path(fontsDir) / (fontName + ext);
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
                return candidate.string();
        }
        return "";
    }

    std::string PresentationCore::GetActiveFontFilePath() const
    {
        std::string fontName;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            fontName = m_ActiveFontName;
        }
        return ResolveFontFilePath(fontName);
    }

    void PresentationCore::SetSelection(const LibrarySelection& selection, bool fromQueue)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_CurrentSelection    = selection;
            m_SelectionFromQueue  = fromQueue;
            m_ActiveSlideIndex    = -1;
        }

        if (selection.type != ItemType::Song && selection.type != ItemType::Bible)
            return;

        std::string styleName;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            auto it = m_CategoryDefaultStyles.find(static_cast<int>(selection.type));
            if (it == m_CategoryDefaultStyles.end() || it->second.empty())
                return;
            styleName = it->second;
        }

        SavedStyle s;
        if (!GetSavedStyle(styleName, s))
            return;

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        ApplySavedStyleToState(s, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::ApplyStyleByName(const std::string& styleName)
    {
        if (styleName.empty()) return;

        SavedStyle style;
        if (!GetSavedStyle(styleName, style)) return;

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::ApplyStyleSnapshot(const SavedStyle& style)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::WireNetworkServerProviders(NetworkStreamServer& srv)
    {
        srv.SetSnapshotProvider([this]() -> StreamSnapshot
        {
            PresentationState st = GetState();

            StreamSnapshot snap;

snap.isProjecting  = st.isProjecting || st.showLanQuickNote;

            if (st.showLanQuickNote) {
                snap.currentText = st.lanQuickNoteText;
                snap.showText    = true;
            } else {
                snap.currentText = st.currentText;
                snap.showText    = st.showText;
            }

            switch (GetLanContentMode()) {
                case OutputContentMode::ClockOnly: {
                    std::time_t now = std::time(nullptr);
                    std::tm lt{};
#ifdef _WIN32
                    localtime_s(&lt, &now);
#else
                    localtime_r(&now, &lt);
#endif
                    char buf[16];
                    std::strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
                    snap.currentText  = buf;
                    snap.showText     = true;
                    snap.isProjecting = true;
                    break;
                }
                case OutputContentMode::Blank:
                    snap.currentText  = "";
                    snap.showText     = false;
                    snap.isProjecting = false;
                    break;
                case OutputContentMode::Live:
                default:
                    break;
            }

            snap.transitionTrigger  = st.transitionTrigger;
            snap.transitionType     = st.transitionType;
            snap.transitionDuration = st.transitionDuration;
            snap.isBgVideo          = (st.bgType == PresentationState::BackgroundType::Video);
            snap.version            = m_StreamVersion.load();
            snap.hasFrame           = m_FrameProviderActive.load();

            snap.refW = m_ProjectorWidth;
            snap.refH = m_ProjectorHeight;

            for (int i = 0; i < 3; i++) snap.bgColor[i] = st.bgColor[i];

            std::string lanStyleName;
            bool hasLanColorOverride = false;
            float lanColorOverride[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                if (st.showLanQuickNote) {
                    lanStyleName = m_LiveQuickNoteLANStyleName;
                    hasLanColorOverride = m_HasLiveQuickNoteLANColorOverride;
                    if (hasLanColorOverride) {
                        for (int i = 0; i < 4; i++) lanColorOverride[i] = m_LiveQuickNoteLANColorOverride[i];
                    }
                }
            }

            SavedStyle lanStyle;
            bool hasLanStyle = false;
            if (st.showLanQuickNote && !lanStyleName.empty()) {
                hasLanStyle = GetSavedStyle(lanStyleName, lanStyle);
            }

            if (st.showLanQuickNote && hasLanStyle) {
                snap.textSize      = lanStyle.lyrics.textSize;
                snap.textAlignment = lanStyle.lyrics.hAlign;
                snap.vAlignment    = lanStyle.lyrics.vAlign;
                snap.autoScale     = lanStyle.lyrics.autoScale;
                snap.margins[0]    = (lanStyle.lyrics.posX - lanStyle.lyrics.sizeW * 0.5f) * 1920.0f;
                snap.margins[1]    = (lanStyle.lyrics.posY - lanStyle.lyrics.sizeH * 0.5f) * 1080.0f;
                snap.margins[2]    = (1.0f - (lanStyle.lyrics.posX + lanStyle.lyrics.sizeW * 0.5f)) * 1920.0f;
                snap.margins[3]    = (1.0f - (lanStyle.lyrics.posY + lanStyle.lyrics.sizeH * 0.5f)) * 1080.0f;
                snap.fontFamily    = lanStyle.lyrics.fontName.empty() ? "Predeterminada" : lanStyle.lyrics.fontName;

                if (hasLanColorOverride) {
                    for (int i = 0; i < 4; i++) snap.textColor[i] = lanColorOverride[i];
                } else {
                    for (int i = 0; i < 4; i++) snap.textColor[i] = lanStyle.lyrics.color[i];
                }
            } else {
                snap.textSize      = st.textSize;
                snap.textAlignment = st.textAlignment;
                snap.vAlignment    = st.vAlignment;
                snap.autoScale     = st.autoScale;
                for (int i = 0; i < 4; i++) snap.margins[i] = st.margins[i];

                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    snap.fontFamily = m_ActiveFontName;
                }

                if (st.showLanQuickNote && hasLanColorOverride) {
                    for (int i = 0; i < 4; i++) snap.textColor[i] = lanColorOverride[i];
                } else {
                    for (int i = 0; i < 4; i++) snap.textColor[i] = st.textColor[i];
                }
            }

            snap.fontVersion = std::hash<std::string>{}(snap.fontFamily);

            return snap;
        });

        srv.SetFrameProvider([this]() -> std::vector<uint8_t>
        {
            std::lock_guard<std::mutex> lk(m_FrameMutex);
            return m_LatestFrame;
        });

        srv.SetFontPathProvider([this]() -> std::string
        {
            PresentationState st = GetState();
            std::string fontName;
            if (st.showLanQuickNote) {
                std::string lanStyleName;
                {
                    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                    lanStyleName = m_LiveQuickNoteLANStyleName;
                }
                if (!lanStyleName.empty()) {
                    SavedStyle lanStyle;
                    if (GetSavedStyle(lanStyleName, lanStyle) && !lanStyle.lyrics.fontName.empty()) {
                        fontName = lanStyle.lyrics.fontName;
                    }
                }
            }
            if (fontName.empty()) {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                fontName = m_ActiveFontName;
            }
            return ResolveFontFilePath(fontName);
        });
    }

    void PresentationCore::ToggleNetworkStream(bool enable, int port)
    {
        if (enable)
        {
            if (m_NetworkServer && m_NetworkServer->IsRunning())
            {

                std::lock_guard<std::recursive_mutex> lk(m_Mutex);
                m_State.isStreamingNet = true;
                m_State.networkURL     = m_NetworkServer->GetBaseURL();
                return;
            }

            m_NetworkServer = std::make_unique<NetworkStreamServer>();
            m_NetworkServer->SetChatStore(&m_ChatMessageStore);
            WireNetworkServerProviders(*m_NetworkServer);

            if (!m_NetworkServer->Start(port))
            {
                m_NetworkServer.reset();
                std::cerr << "[NetworkStream] No se pudo iniciar en puerto " << port << ".\n";
                return;
            }

            std::lock_guard<std::recursive_mutex> lk(m_Mutex);
            m_State.isStreamingNet = true;
            m_State.networkURL     = m_NetworkServer->GetBaseURL();
        }
        else
        {
            {
                std::lock_guard<std::recursive_mutex> lk(m_Mutex);
                m_State.isStreamingNet = false;
                m_State.networkURL.clear();
            }

            m_FrameProviderActive.store(false);
            {
                std::lock_guard<std::mutex> lk(m_FrameMutex);
                m_LatestFrame.clear();
            }

            if (m_NetworkServer && !IsChatRunning())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsStreamingNet() const
    {
        std::lock_guard<std::recursive_mutex> lk(m_Mutex);
        return m_State.isStreamingNet;
    }

    void PresentationCore::ToggleChatServer(bool enable, int port)
    {
        if (enable)
        {
            if (m_NetworkServer && m_NetworkServer->IsRunning())
            {

                m_NetworkServer->SetChatStore(&m_ChatMessageStore);
                std::lock_guard<std::recursive_mutex> lk(m_Mutex);
                m_State.isChatRunning = true;
                m_State.chatURL       = m_NetworkServer->GetBaseURL() + "/chat";
                return;
            }

            m_NetworkServer = std::make_unique<NetworkStreamServer>();
            m_NetworkServer->SetChatStore(&m_ChatMessageStore);
            WireNetworkServerProviders(*m_NetworkServer);

            if (!m_NetworkServer->Start(port))
            {
                m_NetworkServer.reset();
                std::cerr << "[ChatServer] No se pudo iniciar en puerto " << port << ".\n";
                return;
            }

            std::lock_guard<std::recursive_mutex> lk(m_Mutex);
            m_State.isChatRunning = true;
            m_State.chatURL       = m_NetworkServer->GetBaseURL() + "/chat";
        }
        else
        {
            {
                std::lock_guard<std::recursive_mutex> lk(m_Mutex);
                m_State.isChatRunning = false;
                m_State.chatURL.clear();
            }

            if (m_NetworkServer && !IsStreamingNet())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsChatRunning() const
    {
        std::lock_guard<std::recursive_mutex> lk(m_Mutex);
        return m_State.isChatRunning;
    }

    void PresentationCore::EnsureFBO(int w, int h)
    {
        if (m_FBO != 0 && m_FBOWidth == w && m_FBOHeight == h) return;

        DestroyFBO();

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        glGenTextures(1, &m_FBOTex);
        glBindTexture(GL_TEXTURE_2D, m_FBOTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_FBOTex, 0);

        glGenRenderbuffers(1, &m_FBORenderBuf);
        glBindRenderbuffer(GL_RENDERBUFFER, m_FBORenderBuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, m_FBORenderBuf);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "[FBO] Framebuffer incompleto: " << status << "\n";

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenBuffers(2, m_PBO);
        for (int i = 0; i < 2; i++)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER,
                         static_cast<GLsizeiptr>(w) * h * 3,
                         nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        m_PBOIndex = 0;

        m_FBOWidth  = w;
        m_FBOHeight = h;
    }

    void PresentationCore::DestroyFBO()
    {
        if (m_FBO)          { glDeleteFramebuffers(1,  &m_FBO);          m_FBO          = 0; }
        if (m_FBOTex)       { glDeleteTextures(1,       &m_FBOTex);      m_FBOTex       = 0; }
        if (m_FBORenderBuf) { glDeleteRenderbuffers(1,  &m_FBORenderBuf); m_FBORenderBuf = 0; }
        if (m_PBO[0] || m_PBO[1])
        {
            glDeleteBuffers(2, m_PBO);
            m_PBO[0] = m_PBO[1] = 0;
        }
        m_FBOWidth  = 0;
        m_FBOHeight = 0;
    }

    bool PresentationCore::RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB)
    {
        if (w <= 0 || h <= 0) return false;
        if (!m_Impl)          return false;
        if (!IsStreamingNet()) return false;

        static constexpr double kMinCaptureIntervalSec = 1.0 / 15.0;
        double now = glfwGetTime();
        if (now - m_LastFBOCaptureTime < kMinCaptureIntervalSec)
            return false;
        m_LastFBOCaptureTime = now;

        EnsureFBO(w, h);
        if (m_FBO == 0) return false;

        GLint prevFBO         = 0;
        GLint prevViewport[4] = {};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glGetIntegerv(GL_VIEWPORT,            prevViewport);

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, w, h);
outRGB.resize(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
        {
            std::lock_guard<std::recursive_mutex> lk(m_Mutex);
            glClearColor(m_State.bgColor[0], m_State.bgColor[1], m_State.bgColor[2], 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (GetLanContentMode() == OutputContentMode::Live)
        {

            if (ShouldShowLoadingScreen()) {
                m_Impl->background.RenderLogo(
                    static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                    m_LoadingLogoW, m_LoadingLogoH, w, h);
            } else {
                m_Impl->background.Render(w, h);
            }
        }

        outRGB.resize(static_cast<size_t>(w) * h * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        int nextIndex = (m_PBOIndex + 1) % 2;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[m_PBOIndex]);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, 0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[nextIndex]);
GLubyte* ptr = static_cast<GLubyte*>(
    glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
if (ptr)
{

    const size_t rowBytes = static_cast<size_t>(w) * 3;
    for (int row = 0; row < h; ++row)
    {
        const GLubyte* srcRow = ptr + static_cast<size_t>(row) * rowBytes;
        uint8_t* dstRow = outRGB.data() + static_cast<size_t>(h - 1 - row) * rowBytes;
        std::memcpy(dstRow, srcRow, rowBytes);
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}

        m_PBOIndex = nextIndex;

        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1],
                   prevViewport[2], prevViewport[3]);

        return true;
    }

    unsigned int PresentationCore::RenderPublicCompositeToTexture(int w, int h)
    {
        if (w <= 0 || h <= 0 || !m_Impl) return 0;

        EnsureFBO(w, h);
        if (m_FBO == 0) return 0;

        GLint prevFBO         = 0;
        GLint prevViewport[4] = {};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glGetIntegerv(GL_VIEWPORT,            prevViewport);

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, w, h);
        {
            std::lock_guard<std::recursive_mutex> lk(m_Mutex);
            glClearColor(m_State.bgColor[0], m_State.bgColor[1], m_State.bgColor[2], 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (ShouldShowLoadingScreen()) {
            m_Impl->background.RenderLogo(
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                m_LoadingLogoW, m_LoadingLogoH, w, h);
        } else {
            m_Impl->background.Render(w, h);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

        return m_FBOTex;
    }

    void PresentationCore::RequestNavigate(int delta)
    {
        m_PendingNavigateDelta.fetch_add(delta);
    }

    int PresentationCore::ConsumeNavigateCue()
    {
        return m_PendingNavigateDelta.exchange(0);
    }

}
