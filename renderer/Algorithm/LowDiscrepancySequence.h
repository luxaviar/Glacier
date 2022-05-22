#pragma once

#include <cstdint>
#include <cmath>

namespace glacier {

struct LowDiscrepancySequence {

    static constexpr float Halton(uint32_t i, uint32_t b) {
        assert(i > 0);
        float f = 1.0f;
        float r = 0.0f;
    
        while (i > 0) {
            f /= static_cast<float>(b);
            r = r + f * static_cast<float>(i % b);
            i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
        }
    
        return r;
    }

};

}
