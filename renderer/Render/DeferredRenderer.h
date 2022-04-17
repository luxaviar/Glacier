#pragma once

#include <memory>
#include "renderer.h"

namespace glacier {
namespace render {

class GfxDriver;
class Program;

class DeferredRenderer : public Renderer
{
public:
    DeferredRenderer(GfxDriver* gfx);
    void Setup() override;
    void PreRender() override;

    bool OnResize(uint32_t width, uint32_t height) override;

protected:
    struct FrameData {
        Matrix4x4 inverse_projection;
        Matrix4x4 inverse_view;
    };

    void InitRenderTarget() override;

    void AddGPass();
    void AddLightingPass();

    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);

    std::shared_ptr<Buffer> frame_data_;
    std::shared_ptr<MaterialTemplate> gpass_template_;
    std::shared_ptr<Material> lighting_mat_;

    std::shared_ptr<RenderTarget> gbuffer_target_;
    std::shared_ptr<RenderTarget> lighting_target_;
};

}
}
