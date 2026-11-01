#include "HuginStatsManager.h"
#include "core/AppPaths.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <numeric>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ProyecThor::Core {

static std::string GetHuginStatsFilePath() {
    return GetAssetsPath() + "/hugin_stats.json";
}

HuginStatsManager& HuginStatsManager::Get() {
    static HuginStatsManager s_Instance;
    return s_Instance;
}

HuginStatsManager::HuginStatsManager()
    : m_StartTime(std::chrono::steady_clock::now())
{
    LoadStats();
}

void HuginStatsManager::RecordSongProjection(const std::string& title) {
    if (title.empty()) return;
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_SongCounts[title]++;
    m_TotalProjections++;
    SaveStats();
}

SongProjectionStat HuginStatsManager::GetMostProjectedSong() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_SongCounts.empty()) {
        return { "Ninguna canción proyectada aún", 0 };
    }

    std::string topTitle;
    int maxCount = -1;
    for (const auto& [title, count] : m_SongCounts) {
        if (count > maxCount) {
            maxCount = count;
            topTitle = title;
        }
    }
    return { topTitle, maxCount };
}

std::vector<SongProjectionStat> HuginStatsManager::GetTopProjectedSongs(int limit) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<SongProjectionStat> list;
    list.reserve(m_SongCounts.size());
    for (const auto& [title, count] : m_SongCounts) {
        list.push_back({ title, count });
    }

    std::sort(list.begin(), list.end(), [](const SongProjectionStat& a, const SongProjectionStat& b) {
        return a.count > b.count;
    });

    if ((int)list.size() > limit) {
        list.resize(limit);
    }
    return list;
}

int HuginStatsManager::GetTotalSongProjections() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_TotalProjections;
}

void HuginStatsManager::ClearSongStats() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_SongCounts.clear();
    m_TotalProjections = 0;
    SaveStats();
}

void HuginStatsManager::UpdateFPS(float currentFps) {
    if (currentFps <= 0.0f) return;
    std::lock_guard<std::mutex> lock(m_Mutex);

    m_CurrentFPS = currentFps;
    m_FpsBuffer[m_FpsBufferHead] = currentFps;
    m_FpsBufferHead = (m_FpsBufferHead + 1) % kFpsBufferSize;
    if (m_FpsBufferCount < kFpsBufferSize) m_FpsBufferCount++;

    float sum = 0.0f;
    float minVal = 9999.0f;
    for (size_t i = 0; i < m_FpsBufferCount; i++) {
        float f = m_FpsBuffer[i];
        sum += f;
        if (f < minVal) minVal = f;
    }

    m_AverageFPS = (m_FpsBufferCount > 0) ? (sum / (float)m_FpsBufferCount) : currentFps;
    m_MinFPS     = (m_FpsBufferCount > 0) ? minVal : currentFps;
}

float HuginStatsManager::GetFrameTimeMs() const {
    float fps = m_CurrentFPS;
    return (fps > 0.001f) ? (1000.0f / fps) : 0.0f;
}

double HuginStatsManager::GetSessionSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - m_StartTime).count();
}

void HuginStatsManager::LoadStats() {
    std::string path = GetHuginStatsFilePath();
    if (!fs::exists(path)) return;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return;
        json j;
        file >> j;

        m_TotalProjections = j.value("totalProjections", 0);
        if (j.contains("songs") && j["songs"].is_object()) {
            for (auto& [title, val] : j["songs"].items()) {
                if (val.is_number_integer()) {
                    m_SongCounts[title] = val.get<int>();
                }
            }
        }
    } catch (...) {
        // Fallback silencioso si el archivo está corrupto
    }
}

void HuginStatsManager::SaveStats() {
    std::string path = GetHuginStatsFilePath();
    try {
        fs::path parent = fs::path(path).parent_path();
        if (!parent.empty() && !fs::exists(parent)) {
            fs::create_directories(parent);
        }

        json j;
        j["totalProjections"] = m_TotalProjections;
        json songsObj = json::object();
        for (const auto& [title, count] : m_SongCounts) {
            songsObj[title] = count;
        }
        j["songs"] = songsObj;

        std::ofstream file(path, std::ios::trunc);
        if (file.is_open()) {
            file << j.dump(2);
        }
    } catch (...) {
        // Fallback silencioso
    }
}

} // namespace ProyecThor::Core
