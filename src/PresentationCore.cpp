#include <GL/glew.h> 
#include "PresentationCore.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace ProyecThor::Core {

    VLCVideoLayer::VLCVideoLayer(uint32_t w, uint32_t h) {
        m_Context.width = w;
        m_Context.height = h;
        m_Context.pixels = new uint8_t[w * h * 4];

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        m_Context.gpu_texture_id = (void*)(intptr_t)tex;

        const char* args[] = { "--no-xlib", "--quiet" };
        m_VlcInstance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
        
        if (!m_VlcInstance) {
            std::cerr << "CRÍTICO: Falló libvlc_new. Faltan las dependencias o plugins de VLC.\n";
            return; 
        }

        m_MediaPlayer = libvlc_media_player_new(m_VlcInstance);
        if (!m_MediaPlayer) return;

        libvlc_video_set_format(m_MediaPlayer, "RGBA", w, h, w * 4);
        libvlc_video_set_callbacks(m_MediaPlayer, lock_frame, unlock_frame, display_frame, &m_Context);
    }

    VLCVideoLayer::~VLCVideoLayer() {
        Stop();
        if (m_MediaPlayer) libvlc_media_player_release(m_MediaPlayer);
        if (m_VlcInstance) libvlc_release(m_VlcInstance);
        delete[] m_Context.pixels;
        
        GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
        glDeleteTextures(1, &tex);
    }

    void VLCVideoLayer::SetMute(bool mute) {
        if (m_MediaPlayer) {
            libvlc_audio_set_mute(m_MediaPlayer, mute ? 1 : 0);
        }
    }

    void VLCVideoLayer::SetPause(bool pause) {
        if (m_MediaPlayer) libvlc_media_player_set_pause(m_MediaPlayer, pause ? 1 : 0);
    }

    float VLCVideoLayer::GetPosition() {
        return m_MediaPlayer ? libvlc_media_player_get_position(m_MediaPlayer) : 0.0f;
    }

    void VLCVideoLayer::SetPosition(float pos) {
        if (m_MediaPlayer) libvlc_media_player_set_position(m_MediaPlayer, pos);
    }

    int64_t VLCVideoLayer::GetLength() {
        return m_MediaPlayer ? libvlc_media_player_get_length(m_MediaPlayer) : 0;
    }

    int64_t VLCVideoLayer::GetTime() {
        return m_MediaPlayer ? libvlc_media_player_get_time(m_MediaPlayer) : 0;
    }

    void VLCVideoLayer::PlayVideo(const std::string& path) {
        Stop();
        libvlc_media_t* media = libvlc_media_new_path(m_VlcInstance, path.c_str());
        if (media) {

            libvlc_media_add_option(media, "input-repeat=65535"); 

            libvlc_media_player_set_media(m_MediaPlayer, media);
            libvlc_media_release(media);
            libvlc_media_player_play(m_MediaPlayer);
        }
    }

    void VLCVideoLayer::Stop() {
        if (m_MediaPlayer && libvlc_media_player_is_playing(m_MediaPlayer))
            libvlc_media_player_stop(m_MediaPlayer);
    }

    void VLCVideoLayer::UpdateTexture() {
        if (m_Context.needs_gpu_update) {
            std::lock_guard<std::mutex> lock(m_Context.mutex);
            GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Context.width, m_Context.height, GL_RGBA, GL_UNSIGNED_BYTE, m_Context.pixels);
            m_Context.needs_gpu_update = false;
        }
    }

    void* VLCVideoLayer::lock_frame(void* data, void** p_pixels) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->mutex.lock(); 
        *p_pixels = ctx->pixels;
        return nullptr;
    }

    void VLCVideoLayer::unlock_frame(void* data, void* id, void* const* p_pixels) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->mutex.unlock();
    }

    void VLCVideoLayer::display_frame(void* data, void* id) {
        auto* ctx = static_cast<VideoContext*>(data);
        ctx->needs_gpu_update = true;
    }

    PresentationCore::PresentationCore() {
        m_VideoLayer = std::make_unique<VLCVideoLayer>(1920, 1080);
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
        m_State.bgType = PresentationState::BackgroundType::SolidColor;
    }

    void PresentationCore::SetBackgroundMedia(const std::string& path, bool isVideo) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = path;
        m_State.bgType = PresentationState::BackgroundType::Video; 
        
        std::string absolutePath = std::filesystem::absolute(path).string();
        m_VideoLayer->PlayVideo(absolutePath); 
    }

    void PresentationCore::SetLayer2_Text(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = text;
        m_State.showText = !text.empty();
    }

    void PresentationCore::UpdateTextStyle(float size, const float color[4], int align, const float margins[4], bool autoScale) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.textSize = size;
        m_State.textAlignment = align;
        for (int i = 0; i < 4; i++) {
            m_State.textColor[i] = std::clamp(color[i], 0.0f, 1.0f);
            if (margins) m_State.margins[i] = margins[i];
        }
        m_State.autoScale = autoScale;
    }

    void PresentationCore::ClearLayer2() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = "";
        m_State.showText = false;
    }

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State; 
    }

    void PresentationCore::Update() {
        if (m_VideoLayer) {
            m_VideoLayer->UpdateTexture(); 
        }
    }

    void* PresentationCore::GetBackgroundTexture() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_VideoLayer) {
            return m_VideoLayer->GetTextureID();
        }
        return nullptr;
    }

    void PresentationCore::RenderProjectorWindow() {
     
    }

    void PresentationCore::SetSelection(const LibrarySelection& selection) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Selection = selection;
    }

    LibrarySelection PresentationCore::GetSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Selection; 
    }

    void PresentationCore::StopBackgroundMedia() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.bgColor[0] = 0.0f;
    m_State.bgColor[1] = 0.0f;
    m_State.bgColor[2] = 0.0f;
    m_State.bgType = PresentationState::BackgroundType::SolidColor;
    m_State.bgPath = "";
    
    if (m_VideoLayer) {
        m_VideoLayer->Stop();
    }
}
} 