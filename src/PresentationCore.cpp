#include <GL/glew.h> 
#include "PresentationCore.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <windows.h>
#include <atomic>   

static std::filesystem::path GetAppDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

namespace ProyecThor::Core {

    // ========================================================================
    // IMPLEMENTACIÓN DE VLCVideoLayer (Motor de Renderizado)
    // ========================================================================

    VLCVideoLayer::VLCVideoLayer(uint32_t w, uint32_t h) {
        m_Context.width  = w;
        m_Context.height = h;
        m_Context.pixels = new uint8_t[w * h * 4]();
        m_Context.needs_gpu_update = false;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        std::vector<uint8_t> black(w * h * 4, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, black.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_Context.gpu_texture_id = (void*)(intptr_t)tex;

        const char* args[] = { "--no-xlib", "--quiet", "--no-video-title-show" };
        m_VlcInstance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
        if (!m_VlcInstance) {
            std::cerr << "[VLC] CRÍTICO: Falló libvlc_new.\n";
            return;
        }

        m_MediaPlayer = libvlc_media_player_new(m_VlcInstance);
        if (!m_MediaPlayer) {
            std::cerr << "[VLC] CRÍTICO: Falló libvlc_media_player_new.\n";
            return;
        }

        libvlc_video_set_format(m_MediaPlayer, "RGBA", w, h, w * 4);
        libvlc_video_set_callbacks(m_MediaPlayer,
            lock_frame, unlock_frame, display_frame, &m_Context);
    }

    VLCVideoLayer::~VLCVideoLayer() {
        Stop();
        if (m_MediaPlayer) libvlc_media_player_release(m_MediaPlayer);
        if (m_VlcInstance) libvlc_release(m_VlcInstance);
        delete[] m_Context.pixels;

        GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
        if (tex != 0) glDeleteTextures(1, &tex);
    }

    void VLCVideoLayer::PlayVideo(const std::string& path) {
        if (!m_MediaPlayer || !m_VlcInstance) {
            std::cerr << "[VLC] PlayVideo: instancia no válida.\n";
            return;
        }
        Stop();

        std::string cleanPath = path;
        std::replace(cleanPath.begin(), cleanPath.end(), '/', '\\');
        std::cout << "[VLC] PlayVideo → " << cleanPath << "\n";

        libvlc_media_t* media = libvlc_media_new_path(m_VlcInstance, cleanPath.c_str());
        if (!media) {
            std::cerr << "[VLC] ERROR: libvlc_media_new_path devolvió null para: " << cleanPath << "\n";
            return;
        }
        libvlc_media_add_option(media, "input-repeat=65535");
        libvlc_media_player_set_media(m_MediaPlayer, media);
        libvlc_media_release(media);

        int result = libvlc_media_player_play(m_MediaPlayer);
        if (result != 0)
            std::cerr << "[VLC] ERROR: libvlc_media_player_play falló (código " << result << ")\n";
    }

    void VLCVideoLayer::Stop() {
        if (m_MediaPlayer && libvlc_media_player_is_playing(m_MediaPlayer))
            libvlc_media_player_stop(m_MediaPlayer);
    }

    void VLCVideoLayer::UpdateTexture() {
        if (!m_Context.needs_gpu_update.load(std::memory_order_acquire))
            return;

        std::lock_guard<std::mutex> lock(m_Context.mutex);

        if (!m_Context.needs_gpu_update.load(std::memory_order_relaxed))
            return;

        GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            m_Context.width, m_Context.height,
            GL_RGBA, GL_UNSIGNED_BYTE, m_Context.pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_Context.needs_gpu_update.store(false, std::memory_order_release);
    }

    void    VLCVideoLayer::SetMute(bool mute)       { if (m_MediaPlayer) libvlc_audio_set_mute(m_MediaPlayer, mute ? 1 : 0); }
    void    VLCVideoLayer::SetPause(bool pause)     { if (m_MediaPlayer) libvlc_media_player_set_pause(m_MediaPlayer, pause ? 1 : 0); }
    void    VLCVideoLayer::SetVolume(int volume)    { if (m_MediaPlayer) libvlc_audio_set_volume(m_MediaPlayer, std::clamp(volume, 0, 100)); }
    int     VLCVideoLayer::GetVolume() const        { return m_MediaPlayer ? libvlc_audio_get_volume(m_MediaPlayer) : 0; }
    float   VLCVideoLayer::GetPosition()            { return m_MediaPlayer ? libvlc_media_player_get_position(m_MediaPlayer) : 0.0f; }
    void    VLCVideoLayer::SetPosition(float pos)   { if (m_MediaPlayer) libvlc_media_player_set_position(m_MediaPlayer, pos); }
    int64_t VLCVideoLayer::GetLength()              { return m_MediaPlayer ? libvlc_media_player_get_length(m_MediaPlayer) : 0; }
    int64_t VLCVideoLayer::GetTime()                { return m_MediaPlayer ? libvlc_media_player_get_time(m_MediaPlayer) : 0; }

    std::vector<VLCVideoLayer::AudioDevice> VLCVideoLayer::GetAvailableAudioDevices() {
        std::vector<AudioDevice> devices;
        if (!m_MediaPlayer) return devices;
        libvlc_audio_output_device_t* devList = libvlc_audio_output_device_enum(m_MediaPlayer);
        for (auto* cur = devList; cur != nullptr; cur = cur->p_next)
            if (cur->psz_device && cur->psz_description)
                devices.push_back({ cur->psz_device, cur->psz_description });
        if (devList) libvlc_audio_output_device_list_release(devList);
        return devices;
    }

    void VLCVideoLayer::SetAudioDevice(const std::string& deviceId) {
        if (m_MediaPlayer)
            libvlc_audio_output_device_set(m_MediaPlayer, nullptr, deviceId.c_str());
    }

    void* VLCVideoLayer::lock_frame(void* data, void** p_pixels) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->mutex.lock();
        *p_pixels = ctx->pixels;
        return nullptr;
    }

    void VLCVideoLayer::unlock_frame(void* data, void* /*id*/, void* const* /*p_pixels*/) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->mutex.unlock();
    }

    void VLCVideoLayer::display_frame(void* data, void* /*id*/) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->needs_gpu_update.store(true, std::memory_order_release);
    }


    // ========================================================================
    // IMPLEMENTACIÓN DE PresentationCore
    // ========================================================================

    PresentationCore::PresentationCore() {
        m_VideoLayer    = std::make_unique<VLCVideoLayer>(1920, 1080);  // Capa 0: fondo video
        m_OverlayPlayer = std::make_unique<VLCVideoLayer>(1920, 1080);  // Capa 1: overlay video
    }

    void PresentationCore::Update() {
        if (m_VideoLayer)    m_VideoLayer->UpdateTexture();
        if (m_OverlayPlayer) m_OverlayPlayer->UpdateTexture();
    }

    // --- CAPA 0: FONDO (video loop o color sólido) ---
    void PresentationCore::SetBackgroundMedia(const std::string& path, bool isVideo) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = path;
        m_State.bgType = isVideo
            ? PresentationState::BackgroundType::Video
            : PresentationState::BackgroundType::SolidColor;

        std::string absPath = std::filesystem::absolute(path).string();

        // Llamar a VLC fuera lógicamente pero el lock ya protege el estado
        if (isVideo)
            m_VideoLayer->PlayVideo(absPath);
        else
            m_VideoLayer->Stop();
    }

    void PresentationCore::StopBackgroundMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgType = PresentationState::BackgroundType::SolidColor;
            m_State.bgPath = "";
        }
        if (m_VideoLayer) m_VideoLayer->Stop();
    }

    // --- CAPA 1: OVERLAY / VIDEO PANTALLA COMPLETA ---
    void PresentationCore::SetOverlayMedia(const std::string& path) {
        if (path.empty()) return;

        std::filesystem::path finalPath;
        std::filesystem::path p(path);

        if (std::filesystem::exists(p)) {
            finalPath = std::filesystem::absolute(p);
        } else {
            std::vector<std::filesystem::path> searchLocations = {
                GetAppDir() / "assets" / "videos" / p.filename(),
                GetAppDir() / "videos"            / p.filename(),
                std::filesystem::current_path() / "assets" / "videos" / p.filename(),
            };
            for (const auto& loc : searchLocations) {
                std::cout << "[Core] Buscando en: " << loc << "\n";
                if (std::filesystem::exists(loc)) {
                    finalPath = std::filesystem::absolute(loc);
                    break;
                }
            }
        }

        if (finalPath.empty()) {
            std::cerr << "[Core] ERROR: No se encontró '" << path << "'\n";
            return;
        }

        std::cout << "[Core] Overlay cargando: " << finalPath << "\n";
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = finalPath.string();
        }
        if (m_OverlayPlayer) m_OverlayPlayer->PlayVideo(finalPath.string());
    }

    void PresentationCore::StopOverlayMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = "";
        }
        if (m_OverlayPlayer) m_OverlayPlayer->Stop();
    }

    void* PresentationCore::GetBackgroundTexture() {
        return m_VideoLayer ? m_VideoLayer->GetTextureID() : nullptr;
    }

    void* PresentationCore::GetOverlayTexture() {
        return m_OverlayPlayer ? m_OverlayPlayer->GetTextureID() : nullptr;
    }

    void PresentationCore::SetLayer2_Text(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = text;
        m_State.showText    = !text.empty();
    }

    void PresentationCore::ClearLayer2() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = "";
        m_State.showText    = false;
    }

    void PresentationCore::SetProjecting(bool state) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.isProjecting = state;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = index;
    }

    void PresentationCore::SetLayer0_Color(float r, float g, float b) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgColor[0] = std::clamp(r, 0.0f, 1.0f);
        m_State.bgColor[1] = std::clamp(g, 0.0f, 1.0f);
        m_State.bgColor[2] = std::clamp(b, 0.0f, 1.0f);
        m_State.bgType     = PresentationState::BackgroundType::SolidColor;
        if (m_VideoLayer) m_VideoLayer->Stop();
    }

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State;
    }

    void PresentationCore::SetSelection(const LibrarySelection& selection) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Selection = selection;
    }

    LibrarySelection PresentationCore::GetSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Selection;
    }

    void PresentationCore::UpdateTextStyle(float size, const float color[4], int align,
                                            const float margins[4], bool autoScale) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.textSize      = size;
        m_State.textAlignment = align;
        m_State.autoScale     = autoScale;
        for (int i = 0; i < 4; i++) {
            m_State.textColor[i] = std::clamp(color[i], 0.0f, 1.0f);
            if (margins) m_State.margins[i] = margins[i];
        }
    }

    // ========================================================================
    //  RenderProjectorWindow
    //
    //  Debe llamarse con el contexto OpenGL de projectorWindow activo.
    //  El swap de buffers lo hace main.cpp (glfwSwapBuffers).
    //
    //  ORDEN DE CAPAS (painter's algorithm — de atrás hacia adelante):
    //
    //  ┌─────────────────────────────────────────────────┐
    //  │  CAPA 0 – FONDO                                  │
    //  │    • SolidColor → glClearColor                   │
    //  │    • Video      → quad con textura m_VideoLayer  │
    //  ├─────────────────────────────────────────────────┤
    //  │  CAPA 1 – OVERLAY (video pantalla completa)      │
    //  │    • Solo si overlayPath != ""                   │
    //  │    • quad con textura m_OverlayPlayer + alpha    │
    //  ├─────────────────────────────────────────────────┤
    //  │  CAPA 2 – TEXTO (letras)                         │
    //  │    • Solo si showText == true                    │
    //  │    • TODO: integrar FreeType/ImGui para texto    │
    //  └─────────────────────────────────────────────────┘
    //
    //  NOTA IMPORTANTE sobre el overlay:
    //    Al llamar StopOverlayMedia(), overlayPath = "".
    //    → La capa 1 desaparece y el FONDO (capa 0) queda visible.
    //    → La ventana proyector SIGUE ABIERTA (isProjecting sigue true).
    //    Esto es correcto: detener el overlay no corta la proyección,
    //    solo limpia esa capa. Solo "Dejar de Proyectar" cierra la ventana.
    // ========================================================================
    void PresentationCore::RenderProjectorWindow() {

        PresentationState state = GetState();

        // Obtener tamaño real del framebuffer del proyector
        // Para usarlo en el viewport. Se pasa desde fuera o se usa el default 1920x1080.
        // Como no tenemos acceso directo a la ventana aquí, usamos los valores del state
        // si los tienes, o los defaults. El viewport se puede ajustar desde main si es necesario.
        int vpW = m_ProjectorWidth  > 0 ? m_ProjectorWidth  : 1920;
        int vpH = m_ProjectorHeight > 0 ? m_ProjectorHeight : 1080;

        // ====================================================================
        //  CAPA 0 – FONDO
        //  Siempre se renderiza primero. Define el "fondo" de toda la escena.
        // ====================================================================
        glViewport(0, 0, vpW, vpH);

        if (state.bgType == PresentationState::BackgroundType::Video) {
            // Fondo de video: limpiar a negro y luego pintar el quad con la textura
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            void* texPtr = GetBackgroundTexture();
            if (texPtr) {
                GLuint tex = (GLuint)(intptr_t)texPtr;

                glEnable(GL_TEXTURE_2D);
                glDisable(GL_BLEND); // El fondo no necesita blend, cubre todo
                glBindTexture(GL_TEXTURE_2D, tex);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

                // Quad NDC que cubre EXACTAMENTE toda la pantalla (-1 a +1)
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f); // top-left
                    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f); // top-right
                    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f); // bottom-right
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f); // bottom-left
                glEnd();

                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
        } else {
            // Fondo de color sólido (negro por defecto si no se configuró)
            glClearColor(state.bgColor[0], state.bgColor[1], state.bgColor[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // ====================================================================
        //  CAPA 1 – OVERLAY (video pantalla completa)
        //  Solo se pinta si hay un overlay activo. Va SOBRE el fondo.
        //  Al detener el overlay (StopOverlayMedia), overlayPath = "" y esta
        //  capa simplemente no se dibuja → queda el fondo visible.
        // ====================================================================
        if (!state.overlayPath.empty()) {
            void* texPtr = GetOverlayTexture();
            if (texPtr) {
                GLuint tex = (GLuint)(intptr_t)texPtr;

                glEnable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBindTexture(GL_TEXTURE_2D, tex);

                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
                glEnd();

                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_BLEND);
                glDisable(GL_TEXTURE_2D);
            }
        }

        // ====================================================================
        //  CAPA 2 – TEXTO
        //  Va por ENCIMA de ambas capas de video.
        //
        //  ⚠️ OpenGL legacy no tiene renderizado de texto nativo de calidad.
        //  Esta sección es un placeholder; integra FreeType o un atlas de
        //  fuentes para producción real.
        // ====================================================================
        if (state.showText && !state.currentText.empty()) {
            // TODO: integrar FreeType o ImGui drawlist para texto real en el proyector.
            // Por ahora esta capa existe arquitecturalmente pero no renderiza visualmente.
            // El texto se gestiona desde el sistema de fuentes de tu proyecto.
        }

        // ====================================================================
        //  Estado de OpenGL limpio para el próximo frame
        // ====================================================================
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);

        // NOTA: glfwSwapBuffers(projectorWindow) lo llama main.cpp
        // NO llamarlo aquí.
    }

    // Setter para que main.cpp informe el tamaño real del proyector al cambiar de monitor
    void PresentationCore::SetProjectorSize(int w, int h) {
        m_ProjectorWidth  = w;
        m_ProjectorHeight = h;
    }

} // namespace ProyecThor::Core
