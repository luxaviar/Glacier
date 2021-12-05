#include "clock.h"

#if _MSC_VER
int clock_gettime(clockid_t clocktype, timespec *ts) {
    if (clocktype == CLOCK_REALTIME || clocktype == CLOCK_REALTIME_COARSE) {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        auto sec = ns / (1000 * 1000 * 1000);
        ts->tv_sec = sec;
        ts->tv_nsec = ns - sec * 1000 * 1000 * 1000;
    }
    else {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        auto sec = ns / (1000 * 1000 * 1000);
        ts->tv_sec = sec;
        ts->tv_nsec = ns - sec * 1000 * 1000 * 1000;
    }
    return 0;
}
#endif
