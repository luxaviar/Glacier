#include "timer.h"

using namespace std::chrono;

namespace glacier {

Timer::Timer() noexcept {
    Mark();
}

void Timer::Mark() noexcept {
    prev_time_ = ClockType::now();
}

double Timer::DeltaTime() noexcept {
    auto elapse_time = ClockType::now() - prev_time_;
    return duration_cast<nanoseconds>(elapse_time).count() / 1e9;
}

}
