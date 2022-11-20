#pragma once

#include <memory>
#include "Math/Vec4.h"
#include "Math/Vec2.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTarget.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;
class Renderer;
class Material;

struct alignas(16) ToneMapParam {
    float rcp_width;
    float rcp_height;
    float bloom_strength;
    float paperwhite_ratio;
    float max_brightness;
};

class ToneMapping {
public:
    void Setup(Renderer* renderer, const std::shared_ptr<Buffer>& exposure_buffer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);

    void OnResize(uint32_t width, uint32_t height);
    void DrawOptionWindow();

protected:
    std::shared_ptr<Material> tonemapping_mat_;
    ConstantParameter<ToneMapParam> tonemap_buf_;
};

}
}
