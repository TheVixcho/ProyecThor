#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace ProyecThor::Core {

struct SongProjectionStat {
    std::string title;
    int count = 0;
};

class HuginStatsManager {
public:
    static HuginStatsManager& Get();

    // ── Telemetría de Canciones ──
    void RecordSongProjection(const std::string& title);
    SongProjectionStat GetMostProjectedSong() const;
    std::vector<SongProjectionStat> GetTopProjectedSongs(int limit = 5) const;
    int GetTotalSongProjections() const;
    void ClearSongStats();

    // ── Telemetría de Rendimiento Gráfico (FPS) ──
    void UpdateFPS(float currentFps);
    float GetCurrentFPS() const { return m_CurrentFPS; }
    float GetAverageFPS() const { return m_AverageFPS; }
    float GetMinFPS() const { return m_MinFPS; }
    float GetFrameTimeMs() const;

    // ── Tiempo de Sesión ──
    double GetSessionSeconds() const;

private:
    HuginStatsManager();
    ~HuginStatsManager() = default;

    void LoadStats();
    void SaveStats();

    mutable std::mutex m_Mutex;
    std::unordered_map<std::string, int> m_SongCounts;
    int m_TotalProjections = 0;

    // FPS Rolling buffer
    static constexpr size_t kFpsBufferSize = 180;
    float m_FpsBuffer[kFpsBufferSize] = {};
    size_t m_FpsBufferHead = 0;
    size_t m_FpsBufferCount = 0;

    float m_CurrentFPS = 60.0f;
    float m_AverageFPS = 60.0f;
    float m_MinFPS     = 60.0f;

    std::chrono::steady_clock::time_point m_StartTime;
};

} // namespace ProyecThor::Core
