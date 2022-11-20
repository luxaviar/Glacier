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
class DeferredRenderer;
class Renderer;
class Material;

struct TAAParam {
    float static_blending;
    float dynamic_blending;
    float motion_amplify;
    float padding;
};

class TAA {
public:
    void Setup(DeferredRenderer* renderer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);

    void UpdatePerFrameData(uint64_t frame_count, Matrix4x4& projection, float rcp_width, float rcp_height);
    void OnResize(uint32_t width, uint32_t height);

    void DrawOptionWindow();

protected:
    static constexpr int kTAASampleCount = 8;
    std::array<Vector2, kTAASampleCount> halton_sequence_;

    ConstantParameter<TAAParam> taa_param_;
    std::shared_ptr<Material> taa_mat_;

    std::shared_ptr<Texture> temp_hdr_texture_;
    std::shared_ptr<Texture> prev_hdr_texture_;
};

}
}
