#pragma once

#include <memory>
#include "renderer.h"
#include "Algorithm/LowDiscrepancySequence.h"

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
    struct FXAAParam {
        Vector4 config;
    };

    void UpdatePerFrameData() override;
    void InitRenderTarget() override;
    void DoTAA() override;
    void DoFXAA() override;

    void AddGPass();
    void AddLightingPass();

    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);

    PostAAType aa_;
    std::shared_ptr<MaterialTemplate> gpass_template_;
    std::shared_ptr<Material> lighting_mat_;

    std::shared_ptr<Material> fxaa_mat_;
    ConstantParameter<FXAAParam> fxaa_param_;

    std::shared_ptr<Material> taa_mat_;
    std::shared_ptr<Texture> temp_hdr_texture_;
    std::shared_ptr<Texture> prev_hdr_texture_;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;

    static constexpr int kTAASampleCount = 8;
    std::array<Vector2, kTAASampleCount> halton_sequence_;
};

}
}
