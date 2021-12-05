#pragma once

#include <cstdint>
#include <random>

namespace glacier {

class Rand {
public:
    Rand() : rng_() {}

    void SetSeed(uint32_t sd) {
        rng_.seed(sd);
    }

    uint32_t Get() {
        uint32_t v = rng_();
        return v;
    }

    // random number between 0.0 and 1.0
    float GetFloat() {
        // take 23 bits of integer, and divide by 2^23-1
        uint32_t value = Get();
        return float(value & 0x007FFFFF) * (1.0f / 8388607.0f);
    }

private:
    std::minstd_rand rng_;
};

}