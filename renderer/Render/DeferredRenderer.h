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
    DeferredRenderer(GfxDriver* gfx, AntiAliasingType aa = AntiAliasingType::kFXAA);
    void Setup() override;

    std::shared_ptr<Material> CreateLightingMaterial(const char* name) override;

    void PreRender(CommandBuffer* cmd_buffer) override;

    bool OnResize(uint32_t width, uint32_t height) override;

    void SetupBuiltinProperty(Material* mat) override;

protected:
    struct FXAAParam {
        float contrast_threshold;
        float relative_threshold;
        float subpixel_blending;
        float padding;
    };

    struct TAAParam {
        float static_blending;
        float dynamic_blending;
        float motion_amplify;
        float padding;
    };

    void DrawOptionWindow() override;

    void InitRenderTarget() override;

    void DoTAA(CommandBuffer* cmd_buffer) override;
    void DoFXAA(CommandBuffer* cmd_buffer) override;

    void AddGPass();
    void AddLightingPass();
    void AddGtaoPass();

    void InitRenderGraph(GfxDriver* gfx);

    AntiAliasingType option_aa_;
    std::shared_ptr<Program> gpass_program_;
    std::shared_ptr<Material> lighting_mat_;

    std::shared_ptr<Material> fxaa_mat_;
    ConstantParameter<FXAAParam> fxaa_param_;
    ConstantParameter<TAAParam> taa_param_;

    std::shared_ptr<Material> taa_mat_;
    std::shared_ptr<Texture> temp_hdr_texture_;
    std::shared_ptr<Texture> prev_hdr_texture_;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;
};

}
}
