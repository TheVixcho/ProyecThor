#include "SongPlayStats.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ProyecThor::UI {
namespace {

std::filesystem::path GetAppDataDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) {
        return std::filesystem::path(".");
    }
    return std::filesystem::path(appData) / "ProyecThor";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        return std::filesystem::path(".");
    }
    return std::filesystem::path(home) / ".config" / "ProyecThor";
#endif
}

std::filesystem::path GetStatsFilePath() {
    std::filesystem::path dirPath = GetAppDataDir();
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    return dirPath / "song_play_stats.txt";
}

std::filesystem::path GetPerformanceHistoryPath() {
    std::filesystem::path dirPath = GetAppDataDir();
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    return dirPath / "app_performance_history.txt";
}

std::string NormalizeSongTitle(const std::string& title) {
    std::string normalized = title;
    if (normalized.size() > 1 && normalized.back() == '\n') {
        normalized.pop_back();
    }
    return normalized;
}

} // namespace

void RecordSongProjection(const std::string& songTitle, int stanzaCount) {
    if (songTitle.empty() || stanzaCount < 2) {
        return;
    }

    const std::filesystem::path statsPath = GetStatsFilePath();
    std::ifstream input(statsPath);
    std::vector<std::pair<std::string, int>> entries;

    if (input.is_open()) {
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            const size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            entries.emplace_back(line.substr(0, pos), std::stoi(line.substr(pos + 1)));
        }
    }

    const std::string normalized = NormalizeSongTitle(songTitle);
    bool found = false;
    for (auto& entry : entries) {
        if (entry.first == normalized) {
            entry.second += 1;
            found = true;
            break;
        }
    }

    if (!found) {
        entries.emplace_back(normalized, 1);
    }

    std::ofstream output(statsPath, std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    for (const auto& entry : entries) {
        output << entry.first << '=' << entry.second << '\n';
    }
}

std::vector<std::pair<std::string, int>> GetTopSongPlayStats(size_t limit) {
    std::vector<std::pair<std::string, int>> entries;
    const std::filesystem::path statsPath = GetStatsFilePath();
    std::ifstream input(statsPath);

    if (input.is_open()) {
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            const size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            entries.emplace_back(line.substr(0, pos), std::stoi(line.substr(pos + 1)));
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });

    if (entries.size() > limit) {
        entries.resize(limit);
    }

    return entries;
}

int GetSongPlayCount(const std::string& songTitle) {
    if (songTitle.empty()) {
        return 0;
    }

    const std::filesystem::path statsPath = GetStatsFilePath();
    std::ifstream input(statsPath);
    if (!input.is_open()) {
        return 0;
    }

    const std::string normalized = NormalizeSongTitle(songTitle);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        if (line.substr(0, pos) == normalized) {
            return std::stoi(line.substr(pos + 1));
        }
    }

    return 0;
}

int GetTotalSongProjections() {
    const std::filesystem::path statsPath = GetStatsFilePath();
    std::ifstream input(statsPath);
    if (!input.is_open()) {
        return 0;
    }

    int total = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        total += std::stoi(line.substr(pos + 1));
    }
    return total;
}

void RecordPerformanceSample(int fps) {
    if (fps <= 0) {
        return;
    }

    static auto lastSample = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastSample).count() < 1.0f) {
        return;
    }
    lastSample = now;

    std::time_t nowTime = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &nowTime);
#else
    localtime_r(&nowTime, &tm);
#endif

    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "%H:%M", &tm);

    const std::filesystem::path performancePath = GetPerformanceHistoryPath();
    std::ifstream input(performancePath);
    std::vector<std::pair<std::string, int>> entries;

    if (input.is_open()) {
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            const size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            entries.emplace_back(line.substr(0, pos), std::stoi(line.substr(pos + 1)));
        }
    }

    entries.emplace_back(stamp, fps);
    if (entries.size() > 12) {
        entries.erase(entries.begin(), entries.begin() + (entries.size() - 12));
    }

    std::ofstream output(performancePath, std::ios::trunc);
    if (!output.is_open()) {
        return;
    }
    for (const auto& entry : entries) {
        output << entry.first << '=' << entry.second << '\n';
    }
}

std::vector<std::pair<std::string, int>> GetRecentPerformanceHistory(size_t limit) {
    std::vector<std::pair<std::string, int>> entries;
    const std::filesystem::path performancePath = GetPerformanceHistoryPath();
    std::ifstream input(performancePath);
    if (!input.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        entries.emplace_back(line.substr(0, pos), std::stoi(line.substr(pos + 1)));
    }

    if (entries.size() > limit) {
        entries.erase(entries.begin(), entries.begin() + (entries.size() - limit));
    }
    return entries;
}

std::pair<int, int> GetPerformanceSummary() {
    const auto history = GetRecentPerformanceHistory(12);
    if (history.empty()) {
        return {0, 0};
    }

    int sum = 0;
    int peak = 0;
    for (const auto& entry : history) {
        sum += entry.second;
        peak = std::max(peak, entry.second);
    }
    return {sum / static_cast<int>(history.size()), peak};
}

} // namespace ProyecThor::UI
