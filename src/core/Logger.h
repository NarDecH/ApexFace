// ApexFace - detailed logging facility (file + JSONL events + GUI ring buffer)
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum class LogLevel : int { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

class Logger {
public:
    struct Config {
        fs::path dir;                 // where log files are written
        std::string sessionTag;       // "gui" or "cli"
        LogLevel fileLevel = LogLevel::Trace;
        LogLevel consoleLevel = LogLevel::Info;
        bool jsonl = true;            // structured events file
        bool consoleEcho = false;     // CLI sets true
    };

    struct Entry {
        LogLevel lv;
        std::string stamp;   // "2026-10-06 00:12:34.567"
        std::string cat;     // "detect", "sharp", "scan", "gui", ...
        std::string msg;
    };

    static Logger& instance();

    // Opens <dir>/apexface_<tag>_<timestamp>.log and .events.jsonl
    bool start(const Config& cfg);
    void stop();

    bool enabled(LogLevel lv) const { return static_cast<int>(lv) >= static_cast<int>(minLevel_.load(std::memory_order_relaxed)); }
    void log(LogLevel lv, const std::string& cat, const std::string& msg);

    // Structured event for machine analysis: {"ts":"...","cat":...,"event":...,<fields>}
    void event(const std::string& cat, const std::string& name, const std::string& jsonFields);

    // GUI-facing ring buffer (latest entries), version bumps on every write
    std::vector<Entry> snapshotGui(uint64_t& versionOut) const;

    const fs::path& filePath() const { return logPath_; }
    const fs::path& eventsPath() const { return jsonlPath_; }
    bool started() const { return started_; }

    static const char* levelName(LogLevel lv);

private:
    Logger() = default;
    void writeRotated(std::ofstream& out, const fs::path& base, const std::string& line,
                      std::streamoff maxBytes, int keep);
    fs::path openRotated(const char* suffix);

    Config cfg_;
    std::ofstream file_;
    std::ofstream jsonl_;
    fs::path logPath_;
    fs::path jsonlPath_;
    mutable std::mutex mtx_;
    std::vector<Entry> ring_;
    std::atomic<uint64_t> ringVersion_{0};
    std::atomic<LogLevel> minLevel_{LogLevel::Trace};
    bool started_ = false;
};

#define AF_LOG(lv, cat, msg)                                                                     \
    do {                                                                                         \
        if (::Logger::instance().enabled(lv)) {                                                  \
            std::ostringstream _af_os_;                                                          \
            _af_os_ << msg;                                                                      \
            ::Logger::instance().log(lv, cat, _af_os_.str());                                    \
        }                                                                                        \
    } while (0)

#define AF_TRACE(cat, msg) AF_LOG(LogLevel::Trace, cat, msg)
#define AF_DEBUG(cat, msg) AF_LOG(LogLevel::Debug, cat, msg)
#define AF_INFO(cat, msg)  AF_LOG(LogLevel::Info, cat, msg)
#define AF_WARN(cat, msg)  AF_LOG(LogLevel::Warn, cat, msg)
#define AF_ERROR(cat, msg) AF_LOG(LogLevel::Error, cat, msg)
