#pragma once

#include <string>
#include <time.h>
#include <chrono>
#include <format>
#include "Common/Singleton.h"
#include "Concurrent/SpinLock.h"
#include "Common/ByteStream.h"
#include "Logger.h"

namespace glacier {

class Logging : public Singleton<Logging> {
public:
    constexpr static int8_t L_DEBUG = 0;
    constexpr static int8_t L_LOG = 1;
    constexpr static int8_t L_WARNING = 2;
    constexpr static int8_t L_ERROR = 3;
    constexpr static int8_t L_FATAL = 4;

    constexpr  static const char* kLogLevelDesc[] = { "DEBUG", "LOG", "WARNING", "ERROR", "FATAL" };

    Logging();
    virtual ~Logging();

    int pid() const { return pid_; }

    virtual void Flush();

    virtual void Log(int8_t lvl, ByteStream& stream);
    virtual void Write(ByteStream& stream);

    template <typename... Args>
    void Write(std::format_string<Args...> format, Args&&... args) {
        stream_.Reset();
        VPrint(format, std::forward<Args>(args)...);
        Write(stream_);
    }

    template <typename... Args>
    void Print(uint8_t lvl, std::format_string<Args...> format, Args&&... args) {
        auto now = std::chrono::system_clock::now();
        auto now_duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now_duration);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_duration - seconds).count();

        concurrent::SpinLockGuard guard(lock_);
        stream_.Reset();

        VPrint("{:%Y-%m-%d %H:%M:%S}.{} {}:", now, ms, kLogLevelDesc[lvl]);
        VPrint(format, std::forward<Args>(args)...);

        stream_ << '\n';
        Log(lvl, stream_);
    }

    template <typename... Args>
    void PrintMark(uint8_t lvl, const char* file, int line, const char* func,
        std::format_string<Args...> format, Args&&... args) {
        auto now = std::chrono::system_clock::now();
        auto now_duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now_duration);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_duration - seconds).count();

        concurrent::SpinLockGuard guard(lock_);
        stream_.Reset();

        VPrint("{:%Y-%m-%d %H:%M:%S}.{} {}:<{}:{}, {}>\n", now, ms, kLogLevelDesc[lvl], file, line, func);
        VPrint(format, std::forward<Args>(args)...);

        stream_ << '\n';
        Log(lvl, stream_);
    }

protected:
    template<typename... Args>
    void VPrint(std::format_string<Args...> format, Args&&... args) {
        auto result = std::format_to_n(
            static_cast<char*>(stream_.wbegin()), stream_.WritableBytes(),
            format, std::forward<Args>(args)...);
        stream_.Fill(std::min(static_cast<size_t>(result.size),
            stream_.WritableBytes()));
    }

    uint32_t pid_;
    std::unique_ptr<BaseLogger> logger_;
    ByteStream stream_;
    concurrent::SpinLock lock_;
};

}
