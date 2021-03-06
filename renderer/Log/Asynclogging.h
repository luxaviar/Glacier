#pragma once

#include <string>
#include <thread>
#include "Common/Singleton.h"
#include "concurrent/ringbuffer.h"
#include "logging.h"

namespace glacier {

class AsyncLogging : public Logging, public Singleton<AsyncLogging> {
public:
    struct LogLine {
        int8_t lvl;
        ByteStream stream;
    };

    AsyncLogging();
    ~AsyncLogging();

    void Flush() override;

    void Log(int8_t lvl, ByteStream& stream) override;
    void Write(ByteStream& stream) override;

private:
    void Run();

protected:
    bool flush_;
    bool running_;

    concurrent::RingBuffer<LogLine, 2048> queue_;
    std::thread thread_;
};

}
