#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "backend/shaders/CompositePostChain.h"
#include "frontend/panels/stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "backend/settings/SettingsManager.h"
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
#include "frontend/views/Audio.h"

namespace ProyecThor::Core {

   class PresentationCoreImpl {
    public:
        // background: layer de fondo/decorativo. Por requisito de
        // producto NUNCA debe emitir audio real, sin importar el estado
        // de m_IsLiveToPublic, m_TargetMuted, ni ninguna llamada a
        // SetLiveVolume/SetLiveMute. Se construye forceSilent=true por la
        // misma razon que preview: es una garantia estructural dentro de
        // VLCBasePlayer (ver m_ForceSilent), no una convencion que
        // dependa de que el resto del codigo se comporte bien.
        BackgroundLayer background{ false };

        // preview: instancia separada usada por los paneles de biblioteca
        // para scrubbing/preview. Se construye forceSilentAudio=true, asi
        // que estructuralmente NUNCA puede sonar, sin importar que boton
        // de UI la toque (ver VLCBasePlayer::m_ForceSilent).
        BackgroundLayer preview{ true };

        // Ver PreviewLoadWorker.h: saca el Play()/Stop() del Preview del
        // hilo principal, para que una carga lenta ahi nunca le robe
        // tiempo al hilo que actualiza/dibuja el video en vivo al publico.
        PreviewLoadWorker previewLoader;

        // Post-proceso del composite completo de "ProjectorLive" (CRT/
        // Grano/FXAA) — ver CompositePostChain.h. Vive aca (no dentro de
        // background) porque corre en un punto distinto del pipeline (sobre
        // el ImDrawData ya compuesto, no sobre una textura de fondo).
        Shaders::CompositePostChain compositeFX;

        // Una instancia INDEPENDIENTE de post-FX por cada viewport de
        // monitor extra activo (ver PresentationCore::RegisterExtra
        // ProjectorViewport) -- compositeFX de arriba sigue siendo la unica
        // instancia del monitor PRIMARIO, sin cambios.
        std::unordered_map<ImGuiID, std::unique_ptr<Shaders::CompositePostChain>> extraCompositeFX;

        // Overlay (ver SetOverlayMedia/ClearOverlay) -- un PNG estatico con
        // transparencia, no necesita nada del aparato de BackgroundLayer
        // (VLC/crossfade/audio): se carga una vez con stb_image, se sube a
        // una sola textura GL y listo.
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        LibrarySelection sel  = m_CurrentSelection;
        m_CurrentSelection.title = "";
        m_CurrentSelection.type  = ItemType::None;
        m_CurrentSelection.contentData.clear();
        return sel;
    }

    LibrarySelection PresentationCore::PeekSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_CurrentSelection;
    }

void PresentationCore::SetLiveQuickNote(const std::string& text, const float* /*colorOverride*/) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText   = text;
    m_State.showText      = !text.empty();
    m_State.showQuickNote = true;
    m_State.isProjecting  = true;
    // FIX: esto muta currentText (el mismo campo que las letras/Layer2), asi
    // que dispara textTransitionTrigger, no transitionTrigger (ese es solo
    // para fondo/video — ver PresentationState). Antes compartian un unico
    // contador y un cambio de fondo animaba el texto sin que este hubiera
    // cambiado, y viceversa.
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::ClearQuickNote() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText   = "";
    m_State.showText      = false;
    m_State.showQuickNote = false;
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

    void PresentationCore::SetLiveQuickNoteLAN(const std::string& text, const float* colorOverride, const std::string& styleName) {
        std::lock_guard<std::mutex> lock(m_Mutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.lanQuickNoteText = "";
        m_State.showLanQuickNote = false;
        m_LiveQuickNoteLANStyleName.clear();
        m_HasLiveQuickNoteLANColorOverride = false;
        ++m_StreamVersion;
    }

    void PresentationCore::PushRemoteClockTitle(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_PendingClockTitles.push_back(text);
    }

    std::vector<std::string> PresentationCore::DrainRemoteClockTitles() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::vector<std::string> out;
        out.swap(m_PendingClockTitles);
        return out;
    }

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State;
    }
void PresentationCore::SetGlobalMute(bool mute) {
    std::lock_guard<std::mutex> lock(m_Mutex);
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
        // Mutua exclusion: los dos son upscalers de la misma etapa, no
        // tiene sentido correr ambos (ver PostProcessorNIS.h).
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
        if (m_Impl) m_Impl->background.SetUseNativeEngine(engine != 0);
    }
    int PresentationCore::GetVideoRenderEngine() const {
        return (m_Impl && m_Impl->background.GetUseNativeEngine()) ? 1 : 0;
    }

    // NOTA multi-monitor: cada setter de aca abajo, ademas de aplicar al
    // primario (compositeFX), tambien aplica el mismo valor a CADA instancia
    // de m_Impl->extraCompositeFX (monitores de salida extra) -- asi un
    // cambio en Ajustes > Proyeccion se refleja igual en todos los
    // monitores (ver RegisterExtraProjectorViewport, que siembra cada
    // instancia nueva con los valores actuales).
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

        // El backend multi-viewport de esta app es GLFW (ver imgui_impl_glfw),
        // no el backend nativo Win32 -- PlatformHandleRaw puede quedar en
        // null segun la version; PlatformHandle SI es siempre el GLFWwindow*
        // real (eso es lo que crea/gestiona el backend GLFW), asi que se
        // resuelve el HWND desde ahi, mismo mecanismo que AIWebViewPanel::
        // NavigateTo usa para la ventana principal.
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
        // Copia los 14 ajustes actuales del primario para que el monitor
        // extra arranque con la MISMA configuracion (paridad total, ver
        // los 14 setters de arriba, que a partir de ahora tambien escriben
        // en este mapa para que se mantenga sincronizado).
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
            // Pantalla de carga: se muestra el logo en vez del fondo mientras
            // algo esta cargando, para que el publico nunca vea un frame
            // entrecortado o desactualizado (ver Ajustes > Proyeccion > Logo).
            m_Impl->background.RenderLogo(
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                m_LoadingLogoW, m_LoadingLogoH, m_ProjectorWidth, m_ProjectorHeight);
        } else {
            m_Impl->background.Render(m_ProjectorWidth, m_ProjectorHeight);
        }
    }
}
    // ── Ventanas secundarias, API generica ──────────────────────────────
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

        SecondaryOutput& out = m_SecondaryWindows[id]; // crea si no existe
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
        m_SecondaryWindows.erase(id); // el destructor de SecondaryOutputWindow limpia la ventana
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
        // Copia de punteros bajo lock, render fuera del lock: RenderFrame
        // hace MakeContextCurrent + swap, no queremos tener el mutex
        // tomado durante llamadas GL potencialmente bloqueantes (vsync).
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

    // ── Atajos con nombre fijo: Proyector ────────────────────────────────
    bool PresentationCore::CreateProjectorWindow(int monitorIndex)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex >= 0 && monitorIndex < monitorCount) {
        if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[monitorIndex])) {
            SetProjectorSize(vm->width, vm->height); // <-- clave
        }
    }

    bool ok = CreateSecondaryWindow(kProjectorId, monitorIndex, "ProyecThor - Proyector",
        [this](int w, int h) {
            SetProjectorSize(w, h);   // también usar el tamaño real que llega al renderFn
            RenderProjectorWindow();
        });

        if (ok) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.targetMonitorIndex = monitorIndex;
        }
        return ok;
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

// Unico lugar que escribe m_State.bgType — ver comentario en el header.
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

void PresentationCore::SetBackgroundMedia(const std::string& path, bool /*isVideo*/, bool allowAudio) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = path;
        SetBgTypeLocked(PresentationState::BackgroundType::Video);
        ++m_State.transitionTrigger;   // NUEVO
        ++m_StreamVersion;
    }
    if (m_Impl)
        m_Impl->background.SetVideo(path, allowAudio);
}

void PresentationCore::StopBackgroundMedia() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath     = "";
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
        m_State.bgColor[0] = 0.0f; m_State.bgColor[1] = 0.0f; m_State.bgColor[2] = 0.0f;
        ++m_State.transitionTrigger;   // NUEVO
        ++m_StreamVersion;
    }
    if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
}

// Ver comentario en el header (junto a la declaracion) para el porque.
void PresentationCore::SetBackgroundAudio() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = "";
        SetBgTypeLocked(PresentationState::BackgroundType::Audio);
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }
    // Mismo criterio que StopBackgroundMedia: un video de fondo previo no
    // debe seguir sonando por debajo del audio que se acaba de mandar en vivo.
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

        std::lock_guard<std::mutex> lock(m_Mutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_OverlayPath.clear();
        m_HasOverlay = false;
    }

    bool PresentationCore::HasOverlay() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_HasOverlay;
    }

    std::string PresentationCore::GetOverlayPath() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OverlayPath;
    }

    void* PresentationCore::GetOverlayTexture() {
        if (!m_Impl || !m_Impl->overlayTex) return nullptr;
        return (void*)(intptr_t)m_Impl->overlayTex;
    }

    void PresentationCore::SetOverlayClockLayer(bool hasClock, const ProyecThor::UI::OverlayLayer& layer,
                                                 int canvasW, int canvasH) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_HasOverlayClockLayer = hasClock;
        m_OverlayClockLayer    = layer;
        m_OverlayClockCanvasW  = canvasW;
        m_OverlayClockCanvasH  = canvasH;
    }

    bool PresentationCore::HasOverlayClockLayer() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_HasOverlayClockLayer;
    }

    ProyecThor::UI::OverlayLayer PresentationCore::GetOverlayClockLayer() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OverlayClockLayer;
    }

    int PresentationCore::GetOverlayClockCanvasW() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OverlayClockCanvasW;
    }

    int PresentationCore::GetOverlayClockCanvasH() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OverlayClockCanvasH;
    }

    void PresentationCore::SetLiveOverlayClockText(const std::string& text, const float* colorOverride) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_LiveOverlayClockText = text;
        m_HasLiveOverlayClockColorOverride = (colorOverride != nullptr);
        if (colorOverride) {
            for (int i = 0; i < 4; i++) m_LiveOverlayClockColorOverride[i] = colorOverride[i];
        }
    }

    std::string PresentationCore::GetLiveOverlayClockText() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_LiveOverlayClockText;
    }

    bool PresentationCore::HasLiveOverlayClockColorOverride() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_HasLiveOverlayClockColorOverride;
    }

    void PresentationCore::GetLiveOverlayClockColorOverride(float outRGBA[4]) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (int i = 0; i < 4; i++) outRGBA[i] = m_LiveOverlayClockColorOverride[i];
    }

    void PresentationCore::PreloadNextBackgroundMedia(const std::string& path, bool allowAudio) {
        // A proposito NO toca m_State/transitionTrigger: este preload debe
        // ser invisible para el operador y para TransitionPanel — solo
        // adelanta la carga en standby (ver BackgroundLayer::Prefetch).
        if (m_Impl) m_Impl->background.Prefetch(path, allowAudio);
    }

    void PresentationCore::CommitNextBackgroundMedia(const std::string& path, bool /*isVideo*/, bool allowAudio) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
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

    float PresentationCore::GetBackgroundBlendProgress() const {
        return m_Impl ? m_Impl->background.GetTransitionProgress() : 1.0f;
    }

    bool PresentationCore::IsBackgroundStandbyReady() {
        return m_Impl && m_Impl->background.StandbyHasFrame();
    }

    void PresentationCore::SetLoadingLogoPath(const std::string& path) {
        if (path == m_LoadingLogoPath) return; // sin cambios, no recargar cada frame

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
        // Se elimino el logo/pantalla de carga: sumado al preflight de la
        // cola, era una fuente constante de cortes y arranques lentos —
        // BackgroundLayer::Render() ya sigue mostrando el frame actual de
        // Active() mientras un swap esta en curso (asi funciona el
        // crossfade), asi que nunca hace falta tapar la salida con un logo.
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgColor[0] = r; m_State.bgColor[1] = g; m_State.bgColor[2] = b;
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
        m_State.bgPath     = "";
        ++m_State.transitionTrigger;   // NUEVO
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

    // Definida mas abajo en este archivo (junto a ApplySavedStyleToState);
    // espeja una caja de Letras hacia los campos planos legacy de
    // PresentationState. Forward-declarada aca porque UpdateLyricsBoxStyle
    // la necesita antes en el archivo.
    static void ApplyLyricsBoxToState(const TextBoxStyle& box, PresentationState& state,
                                       std::string& activeFontName);

    void PresentationCore::UpdateLyricsBoxStyle(const TextBoxStyle& box) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplyLyricsBoxToState(box, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::UpdateIndexBoxStyle(const TextBoxStyle& box, bool enabled) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.indexBox     = box;
        m_State.indexEnabled = enabled;
        ++m_StreamVersion;
    }

    void PresentationCore::SetCurrentRef(const std::string& ref) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentRef = ref;
        ++m_StreamVersion;
    }

    TextEffectsData PresentationCore::GetTextEffects() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.effects;
    }

void PresentationCore::SetLayer2_Text(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText = text;
    m_State.showText    = !text.empty();
    // Limpia la referencia biblica: solo BibleView/SyncServer la vuelven a
    // poner (con SetCurrentRef) justo despues de llamar esto para un
    // versiculo -- para cualquier otro contenido (canciones, media, notas)
    // no debe quedar una referencia vieja pegada en pantalla.
    m_State.currentRef.clear();
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::ClearLayer2() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText = "";
    m_State.currentRef  = "";
    m_State.showText    = false;
    m_State.nextText    = "";
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::SetClockStyleCue(const std::string& styleName) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingClockStyleCue = styleName;
    m_HasClockStyleCue     = true;
}

std::string PresentationCore::ConsumeClockStyleCue() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasClockStyleCue) return {};
    m_HasClockStyleCue = false;
    return m_PendingClockStyleCue;
}

void PresentationCore::SetPendingTransitionOverride(const std::string& name, float duration) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingTransitionName     = name;
    m_PendingTransitionDuration = duration;
    m_HasTransitionOverride     = true;
}

bool PresentationCore::ConsumePendingTransitionOverride(std::string& outName, float& outDuration) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasTransitionOverride) return false;
    m_HasTransitionOverride = false;
    outName     = m_PendingTransitionName;
    outDuration = m_PendingTransitionDuration;
    return true;
}

void PresentationCore::RequestSongEditorOpen(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingSongEditorOpenFile = filename;
    m_HasSongEditorOpenRequest  = true;
}

bool PresentationCore::ConsumeSongEditorOpenRequest(std::string& outFilename) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasSongEditorOpenRequest) return false;
    m_HasSongEditorOpenRequest = false;
    outFilename = m_PendingSongEditorOpenFile;
    return true;
}

void PresentationCore::SetNextText(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.nextText = text;
    ++m_StreamVersion;
}

    void PresentationCore::SetProjecting(bool projecting) {
        int monitorIndex;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.isProjecting = projecting;
            ++m_StreamVersion;
            monitorIndex = m_State.targetMonitorIndex;
        }

        // Unico punto que habilita/corta el audio real hacia el publico
        // (y, con el motor "VLC ventana nativa", tambien la ventana de
        // video en si — ver BackgroundLayer::SetPubliclyLive). Fuera del
        // lock: BackgroundLayer solo toca atomicos de los players (mas la
        // ventana nativa, que vive en el hilo principal igual que esto),
        // no hace falta serializarlo con m_State.
        if (m_Impl)
            m_Impl->background.SetPubliclyLive(projecting, monitorIndex);
    }

    bool PresentationCore::IsProjecting() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isProjecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = index;
        // Refresca los monitores adicionales desde Settings en el mismo
        // golpe -- este es el unico punto donde arranca la proyeccion
        // publica, asi que no hace falta que cada llamador (ToggleAudience,
        // MonitorQueueEngine) se acuerde de hacerlo por su cuenta.
        m_State.extraTargetMonitors =
            ProyecThor::Settings::SettingsManager::Get().GetSettings().projection.extraMonitors;
    }

    void PresentationCore::SetStaging(bool active, int monitorIndex) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.isStaging = active;
        if (monitorIndex >= 0)
            m_State.stageMonitorIndex = monitorIndex;
        if (active)
            m_State.extraStageMonitors =
                ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay.extraMonitors;
        ++m_StreamVersion;
    }

    bool PresentationCore::IsStaging() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isStaging;
    }

    void PresentationCore::SetProjectorSize(int w, int h) {
        m_ProjectorWidth  = w;
        m_ProjectorHeight = h;
    }

    VLCBasePlayer* PresentationCore::GetBackgroundPlayer() {
        return m_Impl ? m_Impl->background.GetPlayer() : nullptr;
    }

    float PresentationCore::GetLivePosition() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.livePosition;
    }

    void PresentationCore::SetLivePosition(float pos) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.livePosition = pos;
        }
        if (m_Impl) {
            VLCBasePlayer* player = m_Impl->background.GetPlayer();
            if (player) player->SetPosition(pos);
        }
    }

    int PresentationCore::GetLiveVolume() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveVolume;
    }

    bool PresentationCore::GetLiveMute() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveMuted;
    }

    void PresentationCore::SetLiveVolume(int volume) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.liveVolume = volume;
        }
        if (m_Impl)
            m_Impl->background.SetLiveVolume(volume);
    }

    void PresentationCore::SetLiveMute(bool mute) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveLoop;
    }

    void PresentationCore::SetLiveLoop(bool loop) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.liveLoop = loop;
    }
void PresentationCore::SetTransitionConfig(int type, float durationSeconds) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.transitionType     = type;
    m_State.transitionDuration = std::max(0.05f, durationSeconds);
}
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
                        ImFont* font = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), baseFontSize);
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

        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize);
        if (font)
            m_ImGuiFonts[fontName] = font;
    }

    // -------------------------------------------------------------------------
    //  ThemesDirPath — multiplataforma.
    //  En Windows usa la carpeta AppData del usuario (via SHGetFolderPathW).
    //  En Linux sigue la convencion XDG: usa $XDG_CONFIG_HOME si esta definida,
    //  o $HOME/.config en caso contrario.
    // -------------------------------------------------------------------------
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

    // Empaqueta/desempaqueta TextEffectsData como una sola linea CSV en el
    // archivo .theme -- evita 7 bloques de bool/color/intensidad repetidos
    // (uno por efecto) en el formato "key=value" de este archivo. Orden fijo:
    // bg(enabled,r,g,b,a) border(enabled,r,g,b,a,width) shadow(enabled,r,g,b,a,intensity)
    // chromaticAberration(enabled,intensity) glow(enabled,r,g,b,a,intensity)
    // neon(enabled,r,g,b,a,intensity) underline(enabled,r,g,b,a,thickness)
    // text3d(enabled,r,g,b,a,depth) gradient(enabled,Ar,Ag,Ab,Aa,Br,Bg,Bb,Ba,angle)
    // opacityGradient(enabled,angle,strength) -- los ultimos 3 bloques se
    // agregaron despues; UnpackTextEffects los trata como opcionales para
    // que un .theme viejo (37 floats) siga cargando el resto sin resetear.
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
        if (f.size() < 37) return; // linea corrupta/vieja -- deja los defaults

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

        // Campos nuevos (3D + degradados), opcionales -- ver comentario de
        // PackTextEffects. Si el archivo es viejo (solo 37 floats) se dejan
        // los defaults de TextEffectsData en vez de fallar.
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

    // Convierte margenes planos (L,T,R,B en px @1920x1080) al rect
    // centro-relativo normalizado de TextBoxStyle -- inversa exacta de
    // ApplyLyricsBoxToState. Usada solo como fallback de migracion al leer
    // un .theme guardado antes de la reforma a cajas.
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

            // Claves legacy (planas) -- se conservan solo por compatibilidad
            // hacia atras / fallback de migracion, ver abajo.
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

            // Claves nuevas (cajas independientes Letras/Indice).
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

        // Fallback de migracion: un .theme guardado antes de la reforma a
        // cajas no tiene las claves "lyrics*"/"index*" -- se deriva una caja
        // inicial desde los campos legacy ya leidos arriba, para no
        // resetear estilos guardados por el usuario. El indice arranca
        // deshabilitado (los estilos viejos no tenian este concepto).
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

        // Claves legacy -- se derivan de la caja de Letras para que un
        // .theme guardado con el editor nuevo siga siendo legible por
        // codigo viejo/externo que solo conozca el formato plano.
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

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_SavedStyles[style.name] = style;
    }

    void PresentationCore::DeleteStyle(const std::string& name)
    {
        std::error_code ec;
        std::filesystem::remove(
            std::filesystem::path(ThemesDirPath()) / (name + ".theme"), ec);

        std::lock_guard<std::mutex> lock(m_Mutex);
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
            std::lock_guard<std::mutex> lock(m_Mutex);
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

    // Compartido entre PresentationCore::UpdateLyricsBoxStyle (que llama a
    // esto ya con el mutex tomado) y ApplySavedStyleToState -- centraliza el
    // espejo hacia los campos planos legacy de PresentationState (ver
    // comentario en PresentationState::lyricsBox, PresentationCore.h).
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
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CategoryDefaultStyles[static_cast<int>(category)] = styleName;
        }
        SaveCategoryStyles();
    }

    std::string PresentationCore::GetCategoryDefaultStyle(ItemType category) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CategoryDefaultStyles.find(static_cast<int>(category));
        if (it != m_CategoryDefaultStyles.end())
            return it->second;
        return {};
    }

    void PresentationCore::LoadCategoryStyles()
    {
        std::ifstream f(CategoryStylesFilePath());
        if (!f.is_open()) return;

        std::lock_guard<std::mutex> lock(m_Mutex);
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

        std::lock_guard<std::mutex> lock(m_Mutex);
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

    ImFont* PresentationCore::GetImGuiFont(const std::string& fontName, float /*size*/) {
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
            std::lock_guard<std::mutex> lock(m_Mutex);
            fontName = m_ActiveFontName;
        }
        return ResolveFontFilePath(fontName);
    }

    void PresentationCore::SetSelection(const LibrarySelection& selection, bool fromQueue)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CurrentSelection    = selection;
            m_SelectionFromQueue  = fromQueue;
        }

        if (selection.type != ItemType::Song && selection.type != ItemType::Bible)
            return;

        std::string styleName;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_CategoryDefaultStyles.find(static_cast<int>(selection.type));
            if (it == m_CategoryDefaultStyles.end() || it->second.empty())
                return;
            styleName = it->second;
        }

        SavedStyle s;
        if (!GetSavedStyle(styleName, s))
            return;

        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplySavedStyleToState(s, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::ApplyStyleByName(const std::string& styleName)
    {
        if (styleName.empty()) return;

        SavedStyle style;
        if (!GetSavedStyle(styleName, style)) return;

        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    void PresentationCore::ApplyStyleSnapshot(const SavedStyle& style)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    // Arma los providers de un NetworkStreamServer recien creado. Lo llaman
    // tanto ToggleNetworkStream como ToggleChatServer cuando les toca ser
    // los que crean el server compartido (el primero de los dos en pedirlo).
    void PresentationCore::WireNetworkServerProviders(NetworkStreamServer& srv)
    {
        srv.SetSnapshotProvider([this]() -> StreamSnapshot
        {
            PresentationState st = GetState();

            StreamSnapshot snap;
            // El cliente web usa "isProjecting" solo para decidir si oculta el overlay
// de idle y muestra el texto. No debe confundirse con el "isProjecting"
// real que controla el proyector principal y el audio publico — por eso
// aqui se OR-ea con showLanQuickNote: si hay una nota SOLO-LAN activa,
// el cliente de red debe mostrarla aunque la pantalla principal este idle.
snap.isProjecting  = st.isProjecting || st.showLanQuickNote;

            if (st.showLanQuickNote) {
                snap.currentText = st.lanQuickNoteText;
                snap.showText    = true;
            } else {
                snap.currentText = st.currentText;
                snap.showText    = st.showText;
            }

            // OutputContentMode de LAN (ver ViewPanel::RenderContent, pestaña
            // "Inalambrica") -- pisa lo de arriba SI el operador clavo esta
            // salida en "Solo reloj"/"En blanco", independiente de que este
            // en vivo Publico/Stage en este momento.
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
                std::lock_guard<std::mutex> lock(m_Mutex);
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
                    std::lock_guard<std::mutex> lock(m_Mutex);
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
                    std::lock_guard<std::mutex> lock(m_Mutex);
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
                std::lock_guard<std::mutex> lock(m_Mutex);
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
                // Ya esta corriendo (lo pudo haber arrancado el Chat) —
                // Streaming solo se "suma" como usuario, no reinicia nada.
                std::lock_guard<std::mutex> lk(m_Mutex);
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

            std::lock_guard<std::mutex> lk(m_Mutex);
            m_State.isStreamingNet = true;
            m_State.networkURL     = m_NetworkServer->GetBaseURL();
        }
        else
        {
            {
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isStreamingNet = false;
                m_State.networkURL.clear();
            }

            m_FrameProviderActive.store(false);
            {
                std::lock_guard<std::mutex> lk(m_FrameMutex);
                m_LatestFrame.clear();
            }

            // El server entero solo se apaga si Chat tampoco lo esta usando
            // — si esta activo, se queda arriba para el (sin video: dejamos
            // de pushear frames arriba, asi que /frame y /stream vuelven a
            // quedar "vacios" para quien mire el video por LAN).
            if (m_NetworkServer && !IsChatRunning())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsStreamingNet() const
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        return m_State.isStreamingNet;
    }

    void PresentationCore::ToggleChatServer(bool enable, int port)
    {
        if (enable)
        {
            if (m_NetworkServer && m_NetworkServer->IsRunning())
            {
                // Ya esta corriendo (lo pudo haber arrancado Streaming) —
                // solo conectamos el store de mensajes si todavia no estaba.
                m_NetworkServer->SetChatStore(&m_ChatMessageStore);
                std::lock_guard<std::mutex> lk(m_Mutex);
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

            std::lock_guard<std::mutex> lk(m_Mutex);
            m_State.isChatRunning = true;
            m_State.chatURL       = m_NetworkServer->GetBaseURL() + "/chat";
        }
        else
        {
            {
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isChatRunning = false;
                m_State.chatURL.clear();
            }

            // Igual que del otro lado: el server entero solo se apaga si
            // Streaming tampoco lo esta usando.
            if (m_NetworkServer && !IsStreamingNet())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsChatRunning() const
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
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
            std::lock_guard<std::mutex> lk(m_Mutex);
            glClearColor(m_State.bgColor[0], m_State.bgColor[1], m_State.bgColor[2], 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // OutputContentMode de LAN (ver SetLanContentMode): "Solo reloj"/"En
        // blanco" no deben dejar pasar el fondo real (video/imagen en vivo)
        // -- ya se limpio a negro arriba, alcanza con NO pintar nada mas; el
        // texto del reloj lo agrega el cliente web (ver snap.currentText en
        // WireNetworkServerProviders), este FBO solo aporta los pixeles de fondo.
        if (GetLanContentMode() == OutputContentMode::Live)
        {
            // Mismo criterio que RenderProjectorWindow(): el stream de red
            // tampoco debe mostrar un frame entrecortado mientras algo carga.
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
    // glReadPixels entrega fila 0 = abajo de la pantalla. JPEG/PNG
    // esperan fila 0 = arriba. Invertimos filas aca, una sola vez,
    // antes de que el buffer salga hacia el compresor JPEG.
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
            std::lock_guard<std::mutex> lk(m_Mutex);
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

} // namespace ProyecThor::Core