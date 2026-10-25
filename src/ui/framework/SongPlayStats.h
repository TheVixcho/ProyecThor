#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ProyecThor::UI {

void RecordSongProjection(const std::string& songTitle, int stanzaCount);
std::vector<std::pair<std::string, int>> GetTopSongPlayStats(size_t limit = 5);
int GetSongPlayCount(const std::string& songTitle);
int GetTotalSongProjections();

void RecordPerformanceSample(int fps);
std::vector<std::pair<std::string, int>> GetRecentPerformanceHistory(size_t limit = 8);
std::pair<int, int> GetPerformanceSummary();

} // namespace ProyecThor::UI
