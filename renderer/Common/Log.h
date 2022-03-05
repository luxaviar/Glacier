#pragma once

#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include "fmt/format.h"
#include "Log/Logging.h"

constexpr uint8_t L_DEBUG = 0;
constexpr uint8_t L_LOG = 1;
constexpr uint8_t L_WARNING = 2;
constexpr uint8_t L_ERROR = 3;
constexpr uint8_t L_FATAL = 4;

extern uint8_t g_log_level;

constexpr const char* g_log_level_desc[] = { "DEBUG", "LOG", "WARNING", "ERROR", "FATAL" };

#define MARK_DEBUG(format, ...) \
if (g_log_level <= L_DEBUG) { \
    glacier::Logging::Instance()->Print(L_DEBUG, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define MARK_LOG(format, ...) \
if (g_log_level <= L_LOG) { \
    glacier::Logging::Instance()->Print(L_LOG, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_DEBUG(format, ...) \
if (g_log_level <= L_DEBUG) { \
    glacier::Logging::Instance()->Print(L_DEBUG, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_LOG(format, ...) \
if (g_log_level <= L_LOG) { \
    glacier::Logging::Instance()->Print(L_LOG, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_WARN(format, ...) \
if (g_log_level <= L_WARN) { \
    glacier::Logging::Instance()->Print(L_WARN, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_ERR(format, ...) \
if (g_log_level <= L_ERROR) { \
    glacier::Logging::Instance()->Print(L_ERROR, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define LOG_FATAL(format, ...) \
if (g_log_level <= L_FATAL) { \
    glacier::Logging::Instance()->Print(L_FATAL, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(format), __VA_ARGS__); \
}

#define ASSERT(cond) \
if (!(cond)) { \
    LOG_FATAL("Assert failed: ", #cond); \
}

