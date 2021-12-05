#pragma once

#include <time.h>
#include <stdint.h>
#include <type_traits>

#if _MSC_VER

#include <chrono>

enum clockid_t {
    CLOCK_REALTIME,
    CLOCK_REALTIME_COARSE,
    CLOCK_MONOTONIC,
    CLOCK_MONOTONIC_COARSE
};

int clock_gettime(clockid_t clocktype, timespec* ts);

#define localtime_r(a,b) localtime_s(b,a)

#endif
