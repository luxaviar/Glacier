#pragma once

#include <chrono>

namespace glacier {

class Timer {
public:
    using ClockType = std::conditional<std::chrono::high_resolution_clock::is_steady,
        std::chrono::high_resolution_clock, std::chrono::steady_clock >::type;

    Timer() noexcept;
    double Mark() noexcept;
    double Peek() const noexcept;
private:
    ClockType::time_point last;
};

}

//using maxres_sys_or_steady = std::conditional<steady_clock::period::den <= system_clock::period::den,
//        system_clock, steady_clock>::type;

//using maxres_nonsleeping_clock = std::conditional<high_resolution_clock::is_steady, 
//    high_resolution_clock, maxres_sys_or_steady>::type;
