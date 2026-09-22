#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y_%m_%d");
        return ss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard lock(mutex_);
        
        std::string current_date = GetFileTimeStamp();
        if (!log_file_.is_open() || current_date_ != current_date) {
            log_file_.close();
            current_date_ = current_date;
            std::string file_path = "/var/log/sample_log_" + current_date_ + ".log";
            log_file_.open(file_path, std::ios::app);
        }

        log_file_ << GetTimeStamp() << ": "s;
        (log_file_ << ... << args);
        log_file_ << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_date_;
};
