#include "timer.h"

using namespace std::chrono;

namespace glacier {

Timer::Timer() noexcept
{
    last = ClockType::now();
}

double Timer::Mark() noexcept
{
    auto old = last;
    last = ClockType::now();
    auto frameTime = duration_cast<nanoseconds>(last - old);
    return frameTime.count() / 1e9;
}

double Timer::Peek() const noexcept
{
    return duration_cast<nanoseconds>(ClockType::now() - last).count() / 1e9;
}

}
