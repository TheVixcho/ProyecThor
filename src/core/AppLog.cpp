#include "AppLog.h"
#include "AppPaths.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace ProyecThor::Core {

AppLog& AppLog::Get() {
    static AppLog s_Instance;
    return s_Instance;
}

AppLog::AppLog() {
    try {
        std::string dir = ProyecThor::GetAppDataRoot() + "/logs";
        std::filesystem::create_directories(dir);
        m_LogFilePath = dir + "/app.log";
    } catch (...) {
        m_LogFilePath = "app.log";
    }
}

void AppLog::Log(LogLevel level, const std::string& tag, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif

    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << bt.tm_hour << ":"
       << std::setw(2) << bt.tm_min << ":"
       << std::setw(2) << bt.tm_sec << "."
       << std::setw(3) << ms.count();

    LogEntry entry;
    entry.timestamp = ss.str();
    entry.level     = level;
    entry.tag       = tag.empty() ? "App" : tag;
    entry.message   = message;

    const char* lvlStr = "INFO";
    switch (level) {
        case LogLevel::Debug:   lvlStr = "DEBUG"; break;
        case LogLevel::Info:    lvlStr = "INFO";  break;
        case LogLevel::Warning: lvlStr = "WARN";  break;
        case LogLevel::Error:   lvlStr = "ERROR"; break;
    }

    std::string line = "[" + entry.timestamp + "] [" + lvlStr + "] [" + entry.tag + "] " + entry.message;

    // Salida estándar a consola si está adjunta
    std::cout << line << std::endl;

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Entries.size() >= kMaxEntries) {
            m_Entries.erase(m_Entries.begin(), m_Entries.begin() + 200);
        }
        m_Entries.push_back(entry);

        if (!m_LogFilePath.empty()) {
            std::ofstream f(m_LogFilePath, std::ios::app);
            if (f.is_open()) {
                f << line << "\n";
            }
        }
    }
}

void AppLog::Dbg(const std::string& tag, const std::string& message) {
    Get().Log(LogLevel::Debug, tag, message);
}

void AppLog::Inf(const std::string& tag, const std::string& message) {
    Get().Log(LogLevel::Info, tag, message);
}

void AppLog::Warn(const std::string& tag, const std::string& message) {
    Get().Log(LogLevel::Warning, tag, message);
}

void AppLog::Err(const std::string& tag, const std::string& message) {
    Get().Log(LogLevel::Error, tag, message);
}

std::vector<LogEntry> AppLog::GetEntries() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Entries;
}

void AppLog::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Entries.clear();
}

std::string AppLog::GetLogFilePath() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_LogFilePath;
}

size_t AppLog::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Entries.size();
}

} // namespace ProyecThor::Core
