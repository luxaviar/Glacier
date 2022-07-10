#pragma once

#include "Resource.h"
#include "Common/Color.h"

namespace glacier {
namespace render {

struct SamplerState {
    SamplerState() noexcept {
        static_assert(sizeof(SamplerState) == sizeof(uint32_t),
            "SamplerState size not what was intended");
        warpU = WarpMode::kClamp;
        warpV = WarpMode::kClamp;
        warpW = WarpMode::kClamp;

        filter = FilterMode::kTrilinear;
        comp = CompareFunc::kNever;
        max_anisotropy = 0;
    }

    bool operator == (SamplerState rhs) const noexcept { return u == rhs.u; }
    bool operator != (SamplerState rhs) const noexcept { return u != rhs.u; }

    union {
        struct {
            WarpMode warpU : 2;
            WarpMode warpV : 2;
            WarpMode warpW : 2;
            FilterMode filter : 3;
            CompareFunc comp : 3;
            uint8_t max_anisotropy : 5;

        };
        uint32_t u = 0;
    };
};

class Sampler : public Resource {
public:
    Sampler(const SamplerState& ss) : state_(ss) {}

    const SamplerState& state() const { return state_; }

protected:
    SamplerState state_;
};

}
}

