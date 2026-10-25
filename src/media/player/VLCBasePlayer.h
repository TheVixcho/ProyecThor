#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_event_t;

namespace ProyecThor::Core {

    class VLCBasePlayer {
    public:

        struct AudioDevice {
            std::string id;
            std::string description;
        };

        VLCBasePlayer(int decodeThreads = 0, bool useHardwareDecode = true,
                     bool forceSilent = false, bool nativeWindowOutput = false);
        ~VLCBasePlayer();

        VLCBasePlayer(const VLCBasePlayer&)            = delete;
        VLCBasePlayer& operator=(const VLCBasePlayer&) = delete;

        static void SetDefaultHwDecoder(const std::string& dec);
        static std::string GetDefaultHwDecoder();

        static void SetDefaultVideoOutput(const std::string& vout);
        static std::string GetDefaultVideoOutput();

        static void SetDefaultDeinterlace(const std::string& deint);
        static std::string GetDefaultDeinterlace();

        void ApplyDeinterlace(const std::string& mode);

        void Play(const std::string& path, bool loop = false, bool startMuted = false);
        void Stop();

        void BlockPath(const std::string& path);
        void UnblockPath();

        void GetAudioLevels(float& left, float& right);

        void SetPause(bool paused);
        bool IsPaused() const { return m_Paused.load(std::memory_order_relaxed); }

        void SetMute(bool mute);
        void SetVolume(int volume);
        void SetSoftwareVolume(float percent);

        void SetAudioActive(bool active);
        void EnforceSilenceIfNeeded();
        bool IsForceSilent() const { return m_ForceSilent.load(std::memory_order_relaxed); }

        void SetForceSilent(bool v) { m_ForceSilent.store(v, std::memory_order_relaxed); }

        static constexpr int kEqualizerBands = 10;
        void SetEqualizerEnabled(bool enabled);
        void SetEqualizerPreamp(float preampDb);
        void SetEqualizerBand(int index, float ampDb);
        bool IsEqualizerEnabled() const { return m_EqEnabled; }

        void SetPosition(float pos);
        float GetPosition() const;
        bool  IsPlaying() const;

        int64_t GetTime() const;
        int64_t GetLength() const;

        void* GetTextureID();
        void  GetVideoSize(int& width, int& height);

        bool  UpdateTexture();

        bool HasVideoFrame() const;

        void AttachNativeWindow(void* nativeHandle);
        void DetachNativeWindow();
        void* GetNativeWindowHandle() const { return m_NativeWindowHandle; }

        void Reinit();

        std::vector<AudioDevice> GetAvailableAudioDevices();

        void SetAudioDevice(const std::string& deviceId);
        std::string GetCurrentAudioDeviceId() const { return m_AudioDeviceId; }

        const std::string& GetCurrentPath() const { return m_CurrentPath; }

        enum class LoadState { Idle, Opening, Buffering, Ready, Error };
        LoadState GetLoadState() const;
        bool IsLoading() const;

        bool ConsumeEndReached();

        bool ConsumeHadError();

    private:

        int  m_DecodeThreads    = 0;
        bool m_UseHardwareDecode = true;
        bool m_NativeWindowOutput = false;
        void* m_NativeWindowHandle = nullptr;

        libvlc_instance_t*       m_Instance    = nullptr;
        libvlc_media_player_t*   m_MediaPlayer = nullptr;
        void* m_VideoCtx = nullptr;
        void* m_AudioCtx = nullptr;

        mutable std::mutex m_MediaSwapMutex;

        std::atomic<float> m_VolumeMultiplier{1.0f};
        std::atomic<bool>  m_Muted{false};
        std::atomic<bool>  m_EndReached{false};
        std::atomic<bool>  m_HadError{false};
        std::atomic<bool>  m_Paused{false};
        std::atomic<bool>  m_AudioActive{true};
        std::atomic<bool>  m_ForceSilent{false};
        std::atomic<bool>  m_SilenceEnforced{false};
        std::atomic<bool>  m_AudioNeedsSync{false};

        bool  m_EqEnabled = false;
        float m_EqPreamp  = 0.0f;
        float m_EqBands[kEqualizerBands] = { 0.0f };
        void  ApplyEqualizer();

        std::atomic<bool>  m_HasEverPlayed{false};
        std::atomic<bool>  m_VlcIsPlaying{false};
        std::atomic<bool>  m_LoadHasError{false};
        unsigned int m_TextureID = 0;
        int          m_VideoW    = 0;
        int          m_VideoH    = 0;

        std::string m_AudioDeviceId;

        bool                    m_PathBlocked       = false;
        std::string             m_BlockedPath;

        std::string m_CurrentPath;

        std::atomic<uint64_t> m_LoadGeneration{0};
        int m_InstanceId = -1;
        static std::string s_HwDecoder;
        static std::string s_VideoOutput;
        static std::string s_Deinterlace;
        void InitVLC();
        void DestroyVLC();
        void EnsureTexture(int w, int h);
        void CreatePersistentPlayer();

        void LoadAndPlay(const std::string& path, bool loop, bool startMuted, uint64_t myGeneration);

        static void OnVlcEvent(const libvlc_event_t* evt, void* userData);
    };

}
