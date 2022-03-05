#pragma once

#include <string>
#include <time.h>
#include <chrono>
#include "fmt/format.h"
#include "fmt/chrono.h"
#include "Common/Singleton.h"
#include "Concurrent/SpinLock.h"
#include "Common/ByteStream.h"
#include "Logger.h"

namespace glacier {

class Logging : public Singleton<Logging> {
public:
    Logging();
    virtual ~Logging();

    int pid() const { return pid_; }

    virtual void Flush();

    virtual void Log(uint8_t lvl, ByteStream& stream);
    
    template <typename S, typename... Args>
    void Print(uint8_t lvl, const S& format, Args&&... args) {
        auto now = std::chrono::system_clock::now();
        concurrent::SpinLockGuard guard(lock_);
        stream_.Reset();

        VPrint("{:%Y-%m-%d %H:%M:%S} ", now);
        VPrint(format, std::forward<Args>(args)...);

        stream_ << '\n';
        Log(lvl, stream_);
    }

    template <typename S, typename... Args>
    void Print(uint8_t lvl, const char* file, int line, const char* func, const S& format, Args&&... args) {
        auto now = std::chrono::system_clock::now();
        concurrent::SpinLockGuard guard(lock_);
        stream_.Reset();

        VPrint("{:%Y-%m-%d %H:%M:%S} <{}:{}, {}>", now, file, line, func);
        VPrint(format, std::forward<Args>(args)...);

        stream_ << '\n';
        Log(lvl, stream_);
    }

protected:
    template<typename... Args>
    void VPrint(fmt::string_view format, Args&&... args) {
        auto result = fmt::vformat_to_n((char*)stream_.wbegin(), stream_.WritableBytes(),
            format, fmt::make_args_checked<Args...>(format, args...));
        stream_.Fill(result.size);
    }

    uint32_t pid_;
    std::unique_ptr<BaseLogger> logger_;
    ByteStream stream_;
    concurrent::SpinLock lock_;
};

}
