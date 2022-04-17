#include "asynclogging.h"
#include <string.h>
#include "common/log.h"
#include "logger.h"

namespace glacier {

AsyncLogging::AsyncLogging() :
    Logging(),
    flush_(false),
    running_(true),
    thread_(&AsyncLogging::Run, this)
{
}

AsyncLogging::~AsyncLogging() {
    running_ = false;
    thread_.join();
}

void AsyncLogging::Run() {
    while (running_) {
        bool empty = !queue_.Pop([this](LogLine& ll) {
            if (ll.lvl < 0) {
                logger_->Write(ll.stream);
            }
            else {
                logger_->Append(ll.lvl, ll.stream);
            }
        });

        if (empty) {
            std::this_thread::yield();
        }

        if (flush_) {
            flush_ = false;
            Logging::Flush();
        }
    }

    while (queue_.Pop([this](LogLine& ll) {
            if (ll.lvl < 0) {
                logger_->Write(ll.stream);
            }
            else {
                logger_->Append(ll.lvl, ll.stream);
            }
        }))
    {}

    Logging::Flush();
}

void AsyncLogging::Flush() {
    flush_ = true;
}

void AsyncLogging::Log(int8_t lvl, ByteStream& stream) {
    if (!logger_ || lvl < g_log_level) return;

    queue_.Push([lvl, &stream](LogLine& ll) {
        ll.stream.Reset();
        ll.lvl = lvl;
        ll.stream << stream;
    });
}

void AsyncLogging::Write(ByteStream& stream) {
    queue_.Push([&stream](LogLine& ll) {
        ll.stream.Reset();
        ll.lvl = -1;
        ll.stream << stream;
        });
}

}
