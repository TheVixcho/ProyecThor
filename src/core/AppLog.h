#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace ProyecThor::Core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry {
    std::string timestamp; // "HH:MM:SS"
    LogLevel    level = LogLevel::Info;
    std::string tag;
    std::string message;
};

class AppLog {
public:
    static AppLog& Get();

    void Log(LogLevel level, const std::string& tag, const std::string& message);
    static void Dbg(const std::string& tag, const std::string& message);
    static void Inf(const std::string& tag, const std::string& message);
    static void Warn(const std::string& tag, const std::string& message);
    static void Err(const std::string& tag, const std::string& message);

    std::vector<LogEntry> GetEntries() const;
    void Clear();

    std::string GetLogFilePath() const;
    size_t GetEntryCount() const;

private:
    AppLog();
    ~AppLog() = default;
    AppLog(const AppLog&) = delete;
    AppLog& operator=(const AppLog&) = delete;

    mutable std::mutex   m_Mutex;
    std::vector<LogEntry> m_Entries;
    static constexpr size_t kMaxEntries = 2500;
    std::string          m_LogFilePath;
};

} // namespace ProyecThor::Core
