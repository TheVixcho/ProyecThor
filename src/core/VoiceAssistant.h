#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include "ui/views/biblia/BibleTypes.h"

namespace ProyecThor::Core {

enum class VoiceMatchType {
    None,
    SongVerse,
    BibleVerse
};

enum class VoiceMatchScope {
    All = 0,               // Buscar entre todos los datos (Biblia y Canciones)
    BibleOnly = 1,         // Solo Biblia (citas y versículos)
    SelectedSongOnly = 2   // Solo canción seleccionada (salto de estrofas de la canción activa)
};

struct VoiceMatchResult {
    VoiceMatchType type = VoiceMatchType::None;
    std::string    title;       // Título de la canción o referencia (ej. "Romanos 1:3")
    std::string    body;        // Texto de la estrofa o versículo
    std::string    ref;         // Referencia complementaria
    std::string    rawSpoken;   // Texto exacto reconocido por el micrófono
    int            slideIndex   = -1;
    int            bibleBookIdx = -1;
    int            bibleChapIdx = -1;
    int            bibleVerseIdx = -1;
    float          confidence   = 0.0f;
};

struct AudioInputDeviceInfo {
    std::string id;
    std::string name;
    bool        isDefault = false;
    bool        isDesktopAudio = false;
    bool        isRenderDevice = false;
};

struct CachedVoiceSong {
    std::string filename;
    std::string title;
    std::vector<std::string> stanzas;
    std::vector<std::vector<std::string>> stanzaWords;
};

class VoiceAssistant {
public:
    static VoiceAssistant& Get();

    bool Start();
    void Stop();
    bool IsRunning() const { return m_Running.load(); }

    // Dispositivos de micrófono y audio del escritorio
    std::vector<AudioInputDeviceInfo> GetAvailableMicrophones();
    void SetSelectedMicrophone(const std::string& deviceId);
    std::string GetSelectedMicrophone() const;
    bool HasDesktopAudioDevice();
    std::string GetDesktopAudioDeviceId();
    bool SelectDesktopAudioDevice();
    bool IsDesktopAudioSelected();

    void SetAutoProject(bool enable) { m_AutoProject = enable; }
    bool GetAutoProject() const { return m_AutoProject; }

    void SetMatchScope(VoiceMatchScope scope) { m_MatchScope = scope; }
    VoiceMatchScope GetMatchScope() const { return m_MatchScope; }

    void SetSongsEnabled(bool enable) { m_SongsEnabled = enable; }
    bool GetSongsEnabled() const { return m_SongsEnabled; }

    void SetBibleEnabled(bool enable) { m_BibleEnabled = enable; }
    bool GetBibleEnabled() const { return m_BibleEnabled; }

    void SetAudioFixEnabled(bool enable) { m_AudioFixEnabled = enable; }
    bool GetAudioFixEnabled() const { return m_AudioFixEnabled; }
    std::string ApplyAudioFix(const std::string& text) const;

    void SetConfidenceThreshold(float t) { m_ConfidenceThreshold = t; }
    float GetConfidenceThreshold() const { return m_ConfidenceThreshold; }

    std::string GetLastHeardText() const;
    std::string GetLiveHypothesis() const;
    bool        IsReceivingAudio() const;
    std::string GetLastError() const { std::lock_guard<std::mutex> lk(m_Mutex); return m_LastError; }

    bool HasPendingMatch() const;
    VoiceMatchResult GetPendingMatch() const;
    void ConfirmPendingMatch();
    void DismissPendingMatch();

    // Procesa una cadena de texto hablado (llamado internamente por el reconocedor o para simulación)
    void ProcessSpokenText(const std::string& rawText);

    // Debe llamarse en el hilo principal (Update en cada frame de la UI)
    void Update();

private:
    VoiceAssistant();
    ~VoiceAssistant();

    void EnsureBibleLoaded();
    void EnsureSongsCached();
    std::wstring BuildOfflineGrammarXml(const std::wstring& langTag);
    std::vector<std::string> BuildFallbackWordsList();
    void RunRecognitionLoop();   // Supervisor del reconocimiento continuo (hilo de fondo)
    bool TryMatchBible(const std::string& rawSpoken, VoiceMatchResult& outMatch);
    bool TryMatchSong(const std::string& rawSpoken, VoiceMatchResult& outMatch);
    bool TryMatchActiveSong(const std::string& rawSpoken, VoiceMatchResult& outMatch);

    std::thread       m_WorkerThread;
    std::atomic<bool> m_Running{ false };
    VoiceMatchScope   m_MatchScope           = VoiceMatchScope::All;
    bool              m_AutoProject          = true;
    bool              m_SongsEnabled         = true;
    bool              m_BibleEnabled         = true;
    bool              m_AudioFixEnabled      = true;
    float             m_ConfidenceThreshold  = 0.50f;

    mutable std::mutex           m_Mutex;
    std::recursive_mutex         m_BibleMutex;
    std::mutex                   m_SongsMutex;
    std::string                  m_SelectedMicrophoneId;
    std::string                  m_LastHeard;
    std::string                  m_LiveHypothesis;
    std::string                  m_PendingTextToProcess;
    bool                         m_PendingTextIsFinal    = false;
    double                       m_LastHypothesisTime    = 0.0;
    double                       m_LastAudioDetectedTime = 0.0;
    std::string                  m_LastError;
    VoiceMatchResult             m_PendingMatch;
    bool                         m_HasPendingMatch = false;
    double                       m_PendingMatchTimestamp = 0.0;

    // Cooldown para no disparar la misma proyección repetidamente
    std::string                  m_LastProjectedKey;
    double                       m_LastProjectedTime = 0.0;

    UI::BibleData                m_BibleData;
    bool                         m_BibleLoaded = false;

    std::vector<CachedVoiceSong> m_CachedSongs;
    double                       m_LastSongsCacheTime = 0.0;
};

} // namespace ProyecThor::Core
