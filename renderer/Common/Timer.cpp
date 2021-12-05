#include "timer.h"

using namespace std::chrono;

namespace glacier {

Timer::Timer() noexcept
{
    last = steady_clock::now();
}

double Timer::Mark() noexcept
{
    const auto old = last;
    last = steady_clock::now();
    const duration<double> frameTime = last - old;
    return frameTime.count();
}

double Timer::Peek() const noexcept
{
    return duration<double>(steady_clock::now() - last).count();
}

}
