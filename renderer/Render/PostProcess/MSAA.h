#pragma once

#include <memory>
#include "Math/Vec4.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTarget.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;
class Renderer;
class Material;

class MSAA {
public:
    MSAA(MSAAType msaa);

    void Setup(Renderer* renderer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);

    void PreRender(Renderer* renderer, CommandBuffer* cmd_buffer);
    void OnResize(uint32_t width, uint32_t height);

    std::shared_ptr<RenderTarget>& GetRenderTarget() { return msaa_render_target_; }

    void DrawOptionWindow();

protected:
    void OnChangeMSAA(Renderer* renderer);

    constexpr static std::array<const char*, 4> kMsaaDesc = { "None", "2x", "4x", "8x" };
    MSAAType msaa_ = MSAAType::kNone;
    MSAAType option_msaa_ = MSAAType::kNone;

    uint32_t sample_count_ = 1;
    uint32_t quality_level_ = 0;

    //intermediate (MSAA) render target
    std::shared_ptr<RenderTarget> msaa_render_target_;
    //for msaa resolve
    std::array<std::shared_ptr<Material>, (int)MSAAType::kMax> msaa_resolve_mat_; // 0 is no use
};

}
}
