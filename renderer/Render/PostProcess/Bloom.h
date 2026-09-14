#pragma once

#include <memory>
#include <vector>
#include "Math/Vec4.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTarget.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;
class Renderer;
class Material;
class Texture;

struct alignas(16) BloomParam {
    Vector4 _BloomThreshold;  //x: threshold, y: threshold - soft knee, z: soft knee * 2, w: 0.25 / soft knee
    Vector4 _BloomParams;     //x: scatter, y: intensity, zw: 1 / bloom texture size
    Vector4 _BloomTexelSize;  //xy: source texel size, zw: destination texel size
    Vector4 _BloomClamp;      //x: max brightness a single bloom texel can contribute
};

class Bloom {
public:
    void Setup(Renderer* renderer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);
    void OnResize(uint32_t width, uint32_t height);

    void DrawOptionWindow();

protected:
    static constexpr int kMaxMipCount = 6;
    static constexpr uint32_t kMinMipSize = 4;

    void ReallocMipChain(GfxDriver* gfx, uint32_t width, uint32_t height);
    void UpdateParam(uint32_t src_width, uint32_t src_height, uint32_t dst_width, uint32_t dst_height);

    bool enabled_ = true;

    float intensity_ = 1.0f;
    float threshold_ = 0.8f;
    float soft_knee_ = 0.5f;
    float scatter_ = 0.7f;
    float max_brightness_ = 50.0f;

    GfxDriver* gfx_ = nullptr;
    std::shared_ptr<Texture> hdr_texture_;

    ConstantParameter<BloomParam> bloom_param_;

    std::shared_ptr<Material> prefilter_mat_;
    std::shared_ptr<Material> composite_mat_;

    //the source texture of a mip step changes every pass, so the materials of
    //the mip chain can't be shared between the steps
    std::vector<std::shared_ptr<Material>> downsample_mats_;
    std::vector<std::shared_ptr<Material>> upsample_mats_;

    //mips_[0] is the half resolution bloom that is composited into the hdr frame
    std::vector<std::shared_ptr<Texture>> mips_;
    std::vector<std::shared_ptr<RenderTarget>> mip_render_targets_;
};

}
}
