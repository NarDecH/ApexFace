#include "Logger.h"
#include "Platform.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
std::string nowStamp(bool withMs) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    std::time_t t = clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (withMs) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        os << '.' << std::setw(3) << std::setfill('0') << (int)ms.count();
    }
    return os.str();
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    return out;
}

std::string sanitizeTag(const std::string& s) {
    std::string out;
    for (char c : s) {
        out += (isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    }
    return out;
}
} // namespace

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

const char* Logger::levelName(LogLevel lv) {
    switch (lv) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

fs::path Logger::openRotated(const char* suffix) {
    // apexface_<tag>_<yyyymmdd_hhmmss>.log
    std::string base = "apexface_" + sanitizeTag(cfg_.sessionTag) + "_" + nowStamp(false);
    std::replace(base.begin(), base.end(), ' ', '_');
    std::replace(base.begin(), base.end(), ':', '-');
    fs::create_directories(cfg_.dir);
    fs::path p = cfg_.dir / (base + suffix);
    int alt = 1;
    while (fs::exists(p)) {
        p = cfg_.dir / (base + "_" + std::to_string(alt++) + suffix);
    }
    return p;
}

bool Logger::start(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mtx_);
    cfg_ = cfg;
    minLevel_.store(cfg.fileLevel, std::memory_order_relaxed);
    try {
        logPath_ = openRotated(".log");
        file_.open(logPath_, std::ios::out | std::ios::app | std::ios::binary);
        if (cfg_.jsonl) {
            jsonlPath_ = openRotated(".events.jsonl");
            jsonl_.open(jsonlPath_, std::ios::out | std::ios::app | std::ios::binary);
        }
        started_ = file_.is_open();
        ring_.clear();
        ringVersion_.fetch_add(1);
        return started_;
    } catch (const std::exception& e) {
        fprintf(stderr, "Logger start failed: %s\n", e.what());
        return false;
    }
}

void Logger::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_.is_open()) file_.flush();
    if (jsonl_.is_open()) jsonl_.flush();
    file_.close();
    jsonl_.close();
    started_ = false;
}

void Logger::log(LogLevel lv, const std::string& cat, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string stamp = nowStamp(true);
    std::ostringstream line;
    line << stamp << " [" << levelName(lv) << "] [" << cat << "] " << msg;

    if (file_.is_open()) {
        file_ << line.str() << '\n';
        if (lv >= LogLevel::Warn) file_.flush();
    }
    if (cfg_.consoleEcho && static_cast<int>(lv) >= static_cast<int>(cfg_.consoleLevel)) {
        fprintf(lv >= LogLevel::Warn ? stderr : stdout, "%s\n", line.str().c_str());
    }

    Entry e;
    e.lv = lv;
    e.stamp = stamp;
    e.cat = cat;
    e.msg = msg;
    ring_.push_back(std::move(e));
    if (ring_.size() > 800) ring_.erase(ring_.begin(), ring_.begin() + (ring_.size() - 800));
    ringVersion_.fetch_add(1, std::memory_order_relaxed);
}

void Logger::event(const std::string& cat, const std::string& name, const std::string& jsonFields) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!jsonl_.is_open()) return;
    jsonl_ << "{\"ts\":\"" << nowStamp(true) << "\",\"cat\":\"" << jsonEscape(cat)
           << "\",\"event\":\"" << jsonEscape(name) << "\"";
    if (!jsonFields.empty()) {
        jsonl_ << ',' << jsonFields;
    }
    jsonl_ << "}\n";
    if ((long long)jsonl_.tellp() > 64LL * 1024 * 1024) jsonl_.flush();
}

std::vector<Logger::Entry> Logger::snapshotGui(uint64_t& versionOut) const {
    std::lock_guard<std::mutex> lock(mtx_);
    versionOut = ringVersion_.load(std::memory_order_relaxed);
    return ring_;
}
