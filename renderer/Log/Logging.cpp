#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "common/log.h"
#include "logger.h"

#ifdef NDEBUG
uint8_t g_log_level = L_LOG;
#else
uint8_t g_log_level = L_DEBUG;
#endif

namespace glacier {

Logging::Logging() : stream_(4096) {
    pid_ = GetCurrentProcessId();
    if (IsDebuggerPresent()) {
        logger_ = std::make_unique<ConsoleLogger>();
    }
    else {
        logger_ = std::make_unique<FileLogger>("game.log");
    }
}

Logging::~Logging() {
    Flush();
}

void Logging::Flush() {
    logger_->Flush();
}

void Logging::Log(uint8_t lvl, ByteStream& stream) {
    if (lvl < g_log_level) return;

    logger_->Append(lvl, stream);

    if (lvl >= L_WARNING) {
        logger_->Flush();
    }

    if (lvl == L_FATAL) {
        abort();
    }
}

}

