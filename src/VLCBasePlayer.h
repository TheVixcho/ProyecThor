#pragma once
#include <GL/glew.h>
#include <vlc/vlc.h>
#include <mutex>
#include <atomic>
#include <string>
#include <iostream>
#include <algorithm>
#include <vector>

namespace ProyecThor::Core {

    struct VideoContext {
        std::mutex mutex;
        uint8_t* pixels = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        std::atomic<bool> needs_gpu_update{false};
        void* gpu_texture_id = nullptr;
    };

    class VLCBasePlayer {
    protected:
        libvlc_instance_t* m_VlcInstance = nullptr;
        libvlc_media_player_t* m_MediaPlayer = nullptr;
        VideoContext m_Context;

    public:
        VLCBasePlayer(uint32_t w = 1920, uint32_t h = 1080) {
            m_Context.width = w;
            m_Context.height = h;
            m_Context.pixels = new uint8_t[w * h * 4]();

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            m_Context.gpu_texture_id = (void*)(intptr_t)tex;

            const char* args[] = { "--intf=dummy", "--no-xlib", "--quiet", "--no-video-title-show" };
            m_VlcInstance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
            
            if (!m_VlcInstance) {
                std::cerr << "[VLC] CRÍTICO: Falló libvlc_new. ¿Faltan DLLs o plugins en la carpeta del ejecutable?\n";
                return;
            }

            m_MediaPlayer = libvlc_media_player_new(m_VlcInstance);
            libvlc_video_set_format(m_MediaPlayer, "RGBA", w, h, w * 4);
            libvlc_video_set_callbacks(m_MediaPlayer, lock_frame, unlock_frame, display_frame, &m_Context);
        }

        virtual ~VLCBasePlayer() {
            Stop();
            if (m_MediaPlayer) libvlc_media_player_release(m_MediaPlayer);
            if (m_VlcInstance) libvlc_release(m_VlcInstance);
            delete[] m_Context.pixels;
            GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
            if (tex != 0) glDeleteTextures(1, &tex);
        }
struct AudioDevice { 
        std::string id; 
        std::string description; 
    };

    void SetMute(bool mute) { if (m_MediaPlayer) libvlc_audio_set_mute(m_MediaPlayer, mute ? 1 : 0); }
    void SetPause(bool pause) { if (m_MediaPlayer) libvlc_media_player_set_pause(m_MediaPlayer, pause ? 1 : 0); }
    void SetVolume(int volume) { if (m_MediaPlayer) libvlc_audio_set_volume(m_MediaPlayer, std::clamp(volume, 0, 100)); }
    int GetVolume() const { return m_MediaPlayer ? libvlc_audio_get_volume(m_MediaPlayer) : 0; }
    
    float GetPosition() { return m_MediaPlayer ? libvlc_media_player_get_position(m_MediaPlayer) : 0.0f; }
    void SetPosition(float pos) { if (m_MediaPlayer) libvlc_media_player_set_position(m_MediaPlayer, pos); }
    int64_t GetLength() { return m_MediaPlayer ? libvlc_media_player_get_length(m_MediaPlayer) : 0; }
    int64_t GetTime() { return m_MediaPlayer ? libvlc_media_player_get_time(m_MediaPlayer) : 0; }

    std::vector<AudioDevice> GetAvailableAudioDevices() {
        std::vector<AudioDevice> devices;
        if (!m_MediaPlayer) return devices;
        libvlc_audio_output_device_t* devList = libvlc_audio_output_device_enum(m_MediaPlayer);
        for (auto* cur = devList; cur != nullptr; cur = cur->p_next)
            if (cur->psz_device && cur->psz_description)
                devices.push_back({ cur->psz_device, cur->psz_description });
        if (devList) libvlc_audio_output_device_list_release(devList);
        return devices;
    }

    void SetAudioDevice(const std::string& deviceId) {
        if (m_MediaPlayer)
            libvlc_audio_output_device_set(m_MediaPlayer, nullptr, deviceId.c_str());
    }
        void Play(std::string path, bool loop = false) {
            if (!m_VlcInstance || !m_MediaPlayer) return;
            Stop();
            std::replace(path.begin(), path.end(), '/', '\\'); // Saneamiento básico
            libvlc_media_t* media = libvlc_media_new_path(m_VlcInstance, path.c_str());
            if (!media) return;
            
            if (loop) libvlc_media_add_option(media, "input-repeat=65535");
            libvlc_media_player_set_media(m_MediaPlayer, media);
            libvlc_media_release(media);
            libvlc_media_player_play(m_MediaPlayer);
        }

        void Stop() {
            if (m_MediaPlayer && libvlc_media_player_is_playing(m_MediaPlayer))
                libvlc_media_player_stop(m_MediaPlayer);
        }

        void UpdateTexture() {
            if (!m_Context.needs_gpu_update.load(std::memory_order_acquire)) return;
            std::lock_guard<std::mutex> lock(m_Context.mutex);
            GLuint tex = (GLuint)(intptr_t)m_Context.gpu_texture_id;
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Context.width, m_Context.height, GL_RGBA, GL_UNSIGNED_BYTE, m_Context.pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
            m_Context.needs_gpu_update.store(false, std::memory_order_release);
        }

        void* GetTextureID() { return m_Context.gpu_texture_id; }

    private:
        static void* lock_frame(void* data, void** p_pixels) {
            auto* ctx = static_cast<VideoContext*>(data);
            ctx->mutex.lock();
            *p_pixels = ctx->pixels;
            return nullptr;
        }
        static void unlock_frame(void* data, void*, void* const*) { static_cast<VideoContext*>(data)->mutex.unlock(); }
        static void display_frame(void* data, void*) { static_cast<VideoContext*>(data)->needs_gpu_update.store(true, std::memory_order_release); }
    };
}