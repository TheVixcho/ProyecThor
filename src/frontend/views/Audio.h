#pragma once

#include "IPanel.h"
#include "audio/AudioAlbumArt.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/SubtitleImporter.h"
#include <string>
#include <vector>
#include <cstdint>
#include <thread>
#include <mutex>
#include <optional>

namespace ProyecThor::UI {

struct AudioTrack {
    std::string filename;
    std::string displayName;
    std::string fullPath;

    float accentH = 0.0f;

    ProyecThor::Audio::AlbumArt coverArt;
    bool coverLoaded = false;

    std::string sourceUrl;
    std::string lyricsText;
    bool        lyricsEnabled = false;
};

enum class AudioRepeatMode { None, One, All };

struct SpinningDiscParams {
    float rotationAngle = 0.0f;
    float targetSpeed   = 0.0f;
    float currentSpeed  = 0.0f;
    float needleAngle   = -0.45f;
    bool  needleLifted  = true;
};

enum class AudioVisualStyle {
    Vinyl = 0,
    Minimal,
    Bars,
};

const char* AudioVisualStyleName(AudioVisualStyle style);

class AudioPanel : public IPanel {
public:
    AudioPanel();
    ~AudioPanel() override;

    static AudioPanel* GetActiveInstance();

    void Render() override;
    std::string GetName() const override { return "Audio"; }

    void Update();

    void RenderLibraryList();

    void RenderPlayerView();

    bool IsLiveBackground()      const { return m_IsLiveBackground; }
    void SetLiveBackground(bool v);

    bool PlayFileLive(const std::string& filename);

    void RenderLiveBackground(float x, float y, float w, float h);

    const std::vector<float>& GetWaveBars()  const { return m_WaveVec; }
    float                     GetAccentHue() const;
    float                     GetTime()      const { return m_LastTime; }
    bool                      GetIsPlaying() const { return m_IsPlaying && !m_IsPaused; }
    void                      StopIfPathMatches(const std::string& path);

private:
    void Play(int trackIndex);
    void PlayCurrent();
    void Stop();
    void Pause();
    void TogglePlayPause();
    void Next();
    void Previous();
    void SkipSeconds(int seconds);
    void SeekTo(float normalizedPosition);
    void SetVolume(int volume);
    void ApplyGain(float gainDb);
    void ApplyEqualizerToPlayer();

    void EnsureCoverLoaded(int trackIndex);

    void RefreshLibrary();
    void ImportAudioFile();

    void AddToQueue(int trackIndex);
    void AddToQueue(const std::string& fullPath);
    void PlayQueueIndex(int queueIdx);
    void RemoveFromQueue(int queueIdx);
    void MoveQueueItem(int from, int to);
    void ClearQueue();

    void RenderHeader();
    void RenderSpinningDisc(float cx, float cy, float radius);
    void RenderNowPlayingCard();
    void RenderProgressBar();
    void RenderTransportControls();
    void RenderVolumeRow();
    void RenderEqualizerButton();
    void RenderEqualizerPopup();
    void RenderPlaylist();
    void RenderQueueList();
    void RenderStylePopup();

    void RenderLyricsButton();
    void RenderLyricsPopup();
    void RequestLyricsImport(const std::string& url);
    void LoadTrackLyricsSidecar(AudioTrack& track) const;
    void SaveTrackLyricsSidecar(const AudioTrack& track) const;
    void RefreshLiveLyrics();

    std::string FormatTime(int64_t ms) const;
    static float DbToLinear(float dB);
    int  ComputeEffectiveVolume() const;
    static void ComputeTrackAccent(AudioTrack& track);
    int  FindTrackIndexByPath(const std::string& path) const;

    Core::VLCBasePlayer m_VlcPlayer;

    std::vector<AudioTrack>  m_Tracks;
    std::vector<std::string> m_AudioQueue;
    int  m_CurrentTrack       = -1;
    int  m_QueueCurrentIndex  = -1;
    bool m_ViewQueueTab       = false;
    bool m_IsPlaying          = false;
    bool m_IsPaused           = false;
    char m_SearchBuffer[128]  = {};

    float   m_Progress      = 0.0f;
    int64_t m_CurrentTimeMs = 0;
    int64_t m_TotalTimeMs   = 0;
    bool    m_IsSeeking     = false;

    int   m_Volume           = 80;
    float m_GainDb           = 0.0f;
    bool  m_Muted            = false;
    int   m_VolumeBeforeMute = 80;

    static constexpr int kEqBands = 10;
    float m_EqBands[kEqBands] = { 0.0f };
    float m_EqPreamp          = 0.0f;
    bool  m_EqEnabled         = false;

    static constexpr const char* kBandLabels[kEqBands] = {
        "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };

    AudioRepeatMode m_RepeatMode = AudioRepeatMode::None;
    bool            m_Shuffle    = false;

    SpinningDiscParams m_Disc;

    AudioVisualStyle m_VisualStyle    = AudioVisualStyle::Vinyl;
    bool             m_ShowStylePopup = false;

    bool m_ShowWaveform = true;

    bool        m_ShowLyricsPopup     = false;
    bool        m_LyricsImportRunning = false;
    char        m_LyricsUrlBuffer[512] = {};
    std::string m_LyricsImportError;
    std::thread m_LyricsImportThread;
    std::mutex  m_LyricsImportMutex;
    std::optional<Core::SubtitleFetchResult> m_LyricsImportResult;

    static constexpr int kWaveBars = 32;
    float m_WaveBars[kWaveBars]    = { 0.0f };
    float m_WaveTargets[kWaveBars] = { 0.0f };
    float m_WaveTimer              = 0.0f;

    std::vector<float> m_WaveVec;

    bool m_IsLiveBackground = false;

    std::string m_LastExternalSelection;

    float m_LastTime = 0.0f;
};

}

