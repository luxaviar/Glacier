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
    DeferredRenderer(GfxDriver* gfx, PostAAType aa = PostAAType::kFXAA);
    void Setup() override;
    void PreRender() override;

    bool OnResize(uint32_t width, uint32_t height) override;

protected:
    struct FrameData {
        Matrix4x4 inverse_projection;
        Matrix4x4 inverse_view;
    };

    struct FXAAParam {
        Vector4 config;
        Vector4 texel_size;
    };

    void InitRenderTarget() override;
    void DoFXAA() override;

    void AddGPass();
    void AddLightingPass();

    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);

    PostAAType aa_;
    std::shared_ptr<Buffer> frame_data_;
    std::shared_ptr<MaterialTemplate> gpass_template_;
    std::shared_ptr<Material> lighting_mat_;

    std::shared_ptr<Material> fxaa_mat_;
    std::shared_ptr<Buffer> fxaa_param_;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;
    //std::shared_ptr<RenderTarget> aa_render_target_;
};

}
}
