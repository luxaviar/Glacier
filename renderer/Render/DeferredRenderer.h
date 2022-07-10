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

    struct GtaoParam {
        float temporal_cos_angle = 1.0f;
        float temporal_sin_angle = 0.0f;
        float temporal_offset = 0.0f;
        float temporal_direction = 0.0f;
        float radius = 5.0f;
        float fade_to_radius = 2.0f;
        float thickness = 1.0f;
        float fov_scale;
        float intensity = 1.0f;
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
    ConstantParameter<GtaoParam> gtao_param_;

    std::shared_ptr<Material> taa_mat_;
    std::shared_ptr<Texture> temp_hdr_texture_;
    std::shared_ptr<Texture> prev_hdr_texture_;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;

    std::shared_ptr<Material> gtao_mat_;
    std::shared_ptr<Material> gtao_filter_x_mat_;
    std::shared_ptr<Material> gtao_filter_y_mat_;
    ConstantParameter<Vector4> gtao_filter_x_param_;
    ConstantParameter<Vector4> gtao_filter_y_param_;

    std::shared_ptr<RenderTarget> ao_render_target_;
    std::shared_ptr<RenderTarget> ao_tmp_render_target_;
};

}
}
