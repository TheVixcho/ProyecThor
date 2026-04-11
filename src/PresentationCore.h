#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <cstdint>
#include <vlc/vlc.h>
#include <atomic>

namespace ProyecThor::Core {

    enum class ItemType { None, Song, Video, Image, Bible };

    struct LibrarySelection {
        ItemType type = ItemType::None;
        std::string title = "";
        std::vector<std::string> contentData; 
    };

    struct PresentationState {
        bool isProjecting = false;
        
        enum class BackgroundType { SolidColor, Image, Video, None };
        BackgroundType bgType = BackgroundType::SolidColor;
        
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };
        std::string bgPath = "";
        
        bool showText = false;
        std::string currentText = "";
        float textSize = 60.0f;
        float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        int textAlignment = 1; 
        
        float margins[4] = { 100.0f, 50.0f, 100.0f, 50.0f }; 
        bool autoScale = true;

        int targetMonitorIndex = 1;
    std::string overlayPath = "";
    };

    class VLCVideoLayer {
    private:
        libvlc_instance_t* m_VlcInstance = nullptr;
        libvlc_media_player_t* m_MediaPlayer = nullptr;
        
        struct VideoContext {
            std::mutex mutex;
            uint8_t* pixels = nullptr;
            unsigned int width, height;
            std::atomic<bool> needs_gpu_update { false };
            void* gpu_texture_id = nullptr; 
        } m_Context;

        static void* lock_frame(void* data, void** p_pixels);
        static void unlock_frame(void* data, void* id, void* const* p_pixels);
        static void display_frame(void* data, void* id);

    public:
        VLCVideoLayer(uint32_t w, uint32_t h);
        ~VLCVideoLayer();

        void PlayVideo(const std::string& path);
        void Stop();
        void UpdateTexture();
        void* GetTextureID() const { return m_Context.gpu_texture_id; }

        void SetMute(bool mute); 
        void SetPause(bool pause);
        float GetPosition();
        void SetPosition(float pos);
        int64_t GetLength();
        int64_t GetTime();
        void SetVolume(int volume); 
        int GetVolume() const;
        void SetAudioDevice(const std::string& deviceId);

        struct AudioDevice {
            std::string id;
            std::string description;
        };
        std::vector<AudioDevice> GetAvailableAudioDevices();
    };

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }

        PresentationCore(const PresentationCore&) = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;
        void SetProjectorSize(int w, int h);
        // Métodos de Control General
        void SetProjecting(bool state);
        void SetTargetMonitor(int index);
        void Update(); 
        void RenderProjectorWindow(); 

        // Capa 0: Fondo (Background)
        void SetLayer0_Color(float r, float g, float b);
        void SetBackgroundMedia(const std::string& path, bool isVideo);
        void StopBackgroundMedia(); 
        void* GetBackgroundTexture();

        // Capa 1: Video Independiente (Overlay)
        void SetOverlayMedia(const std::string& path);
        void StopOverlayMedia();
        void* GetOverlayTexture();
        VLCVideoLayer* GetOverlayPlayer() { return m_OverlayPlayer.get(); }

        // Capa 2: Texto
        void SetLayer2_Text(const std::string& text);
        void UpdateTextStyle(float size, const float color[4], int align, const float margins[4], bool autoScale);
        void ClearLayer2();

        // Estado y Selección
        PresentationState GetState();
        void SetSelection(const LibrarySelection& selection);
        LibrarySelection GetSelection();

    private:
        PresentationCore(); 
        ~PresentationCore() = default;
int m_ProjectorWidth  = 1920;
int m_ProjectorHeight = 1080;

        PresentationState m_State;
        LibrarySelection m_Selection;
        std::mutex m_Mutex;

        std::unique_ptr<VLCVideoLayer> m_VideoLayer;    // Para el fondo
        std::unique_ptr<VLCVideoLayer> m_OverlayPlayer;  // Para el video en vivo
    };

}