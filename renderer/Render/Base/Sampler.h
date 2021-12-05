#pragma once

#include "Resource.h"
#include "Common/Color.h"
#include "SamplerState.h"

namespace glacier {
namespace render {

struct SamplerBuilder {
    SamplerBuilder& SetFilter(FilterMode filter_) { filter = filter_; return *this; }
    SamplerBuilder& SetWarp(WarpMode warp_) { warp = warp_; return *this; }
    SamplerBuilder& SetCompare(CompareFunc cmp_) { cmp = cmp_; return *this; }
    SamplerBuilder& SetMinLod(float min_lod_) { min_lod = min_lod_; return *this; }
    SamplerBuilder& SetMaxLod(float max_lod_) { max_lod = max_lod_; return *this; }
    SamplerBuilder& SetLodBias(float bias) { lod_bias = bias; return *this; }
    SamplerBuilder& SetBorderColor(Color color) { border_color = color; return* this; }

    SamplerBuilder& SetMaxAnisotropy(uint8_t max_ansi_) {
        max_anisotropy = math::Clamp<uint8_t>(max_ansi_, 1, 16);
        return *this;
    }

    WarpMode warp = WarpMode::kClamp;
    FilterMode filter = FilterMode::kTrilinear;
    CompareFunc cmp = CompareFunc::kNever;
    uint8_t max_anisotropy = 1;

    float min_lod = -FLT_MAX;
    float max_lod = FLT_MAX;
    float lod_bias = 0.0f;

    Color border_color = Color::kWhite;
};

class Sampler : public Resource {
public:
    static SamplerBuilder Builder() { return {}; }

    Sampler() {}
    virtual void Bind(ShaderType shader_type, uint16_t slot) = 0;
    virtual void UnBind(ShaderType shader_type, uint16_t slot) = 0;
};

}
}
