#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <cstdint>
#include <vlc/vlc.h>

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
    };

    class VLCVideoLayer {
    private:
        libvlc_instance_t* m_VlcInstance = nullptr;
        libvlc_media_player_t* m_MediaPlayer = nullptr;
        
        struct VideoContext {
            std::mutex mutex;
            uint8_t* pixels = nullptr;
            unsigned int width, height;
            bool needs_gpu_update = false;
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
        void SetTextureID(void* id) { m_Context.gpu_texture_id = id; }

        void SetMute(bool mute); 
        void SetPause(bool pause);
        float GetPosition();
        void SetPosition(float pos);
        int64_t GetLength();
        int64_t GetTime();
    };

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }

        PresentationCore(const PresentationCore&) = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;

        void SetProjecting(bool state);
        void SetTargetMonitor(int index);
        
        void SetLayer0_Color(float r, float g, float b);
        void SetBackgroundMedia(const std::string& path, bool isVideo);
        void StopBackgroundMedia(); 

        void SetLayer2_Text(const std::string& text);
        void UpdateTextStyle(float size, const float color[4], int align, const float margins[4], bool autoScale);
        void ClearLayer2();

        void Update(); 
        void RenderProjectorWindow(); 

        PresentationState GetState();
        
        void SetSelection(const LibrarySelection& selection);
        LibrarySelection GetSelection();

        void* GetBackgroundTexture();

    private:
        PresentationCore(); 
        ~PresentationCore() = default;

        PresentationState m_State;
        LibrarySelection m_Selection;
        std::mutex m_Mutex;

        std::unique_ptr<VLCVideoLayer> m_VideoLayer;
    };

}