#pragma once

#include <chrono>

namespace glacier {

class Timer
{
public:
    Timer() noexcept;
    double Mark() noexcept;
    double Peek() const noexcept;
private:
    std::chrono::steady_clock::time_point last;
};

}