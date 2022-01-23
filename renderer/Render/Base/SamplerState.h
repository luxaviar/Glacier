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

//struct SamplerBuilder {
//    SamplerBuilder& SetFilter(FilterMode filter_) { filter = filter_; return *this; }
//    SamplerBuilder& SetWarp(WarpMode warp_) { warp = warp_; return *this; }
//    SamplerBuilder& SetCompare(CompareFunc cmp_) { cmp = cmp_; return *this; }
//    SamplerBuilder& SetMinLod(float min_lod_) { min_lod = min_lod_; return *this; }
//    SamplerBuilder& SetMaxLod(float max_lod_) { max_lod = max_lod_; return *this; }
//    SamplerBuilder& SetLodBias(float bias) { lod_bias = bias; return *this; }
//    SamplerBuilder& SetBorderColor(Color color) { border_color = color; return* this; }
//
//    SamplerBuilder& SetMaxAnisotropy(uint8_t max_ansi_) {
//        max_anisotropy = math::Clamp<uint8_t>(max_ansi_, 1, 16);
//        return *this;
//    }
//
//    WarpMode warp = WarpMode::kClamp;
//    FilterMode filter = FilterMode::kTrilinear;
//    CompareFunc cmp = CompareFunc::kNever;
//    uint8_t max_anisotropy = 1;
//
//    float min_lod = -FLT_MAX;
//    float max_lod = FLT_MAX;
//    float lod_bias = 0.0f;
//
//    Color border_color = Color::kWhite;
//};

