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
    duration<double> frameTime = last - old;
    return frameTime.count();
}

double Timer::Peek() const noexcept
{
    return duration<double>(ClockType::now() - last).count();
}

}
