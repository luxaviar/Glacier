#pragma once

#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include "fmt/format.h"
#include "Log/Logging.h"

extern uint8_t g_log_level;

#define MARK_DEBUG(format, ...) \
if (g_log_level <= glacier::Logging::L_DEBUG) { \
    glacier::Logging::Instance()->Print(glacier::Logging::L_DEBUG, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define MARK_LOG(format, ...) \
if (g_log_level <= glacier::Logging::L_LOG) { \
    glacier::Logging::Instance()->Print(glacier::Logging::L_LOG, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_DEBUG(format, ...) \
if (g_log_level <= glacier::Logging::L_DEBUG) { \
    glacier::Logging::Instance()->Print(glacier::Logging::L_DEBUG, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_LOG(format, ...) \
if (g_log_level <= glacier::Logging::L_LOG) { \
    glacier::Logging::Instance()->Print(glacier::Logging::L_LOG, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_WARN(format, ...) \
if (g_log_level <= glacier::Logging::L_WARNING) { \
    glacier::Logging::Instance()->Print(glacier::Logging::L_WARNING, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_ERR(format, ...) \
if (g_log_level <= glacier::Logging::L_ERROR) { \
    glacier::Logging::Instance()->PrintMark(glacier::Logging::L_ERROR, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_FATAL(format, ...) \
if (g_log_level <= glacier::Logging::L_FATAL) { \
    glacier::Logging::Instance()->PrintMark(glacier::Logging::L_FATAL, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define ASSERT(cond) \
if (!(cond)) { \
    LOG_FATAL("Assert failed: ", #cond); \
}

