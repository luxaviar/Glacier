#pragma once

#include <memory>
#include "renderer.h"

namespace glacier {
namespace render {

class GfxDriver;
class Program;

class ForwardRenderer : public Renderer
{
public:
    ForwardRenderer(GfxDriver* gfx, MSAAType msaa = MSAAType::kNone);
    void Setup() override;

    bool OnResize(uint32_t width, uint32_t height) override;

    void PreRender() override;

    std::shared_ptr<RenderTarget>& intermediate_target() { return msaa_target_; }

protected:
    void InitMSAA();

    void InitRenderTarget() override;
    void InitToneMapping() override;
    void ResolveMSAA() override;

    void AddLightingPass();
    
    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);

    MSAAType msaa_ = MSAAType::kNone;

    std::shared_ptr<MaterialTemplate> pbr_template_;

    //intermediate (MSAA) render target
    std::shared_ptr<RenderTarget> msaa_target_;

    //for msaa resolve
    std::shared_ptr<Material> msaa_resolve_mat_[4]; // 0 is no use
};

}
}
