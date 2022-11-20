#pragma once

#include <memory>
#include "renderer.h"
#include "Render/PostProcess/TAA.h"
#include "Render/PostProcess/FXAA.h"

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
    const std::shared_ptr<RenderTarget>& GetGBufferRenderTarget() const { return gbuffer_render_target_; }

    void PreRender(CommandBuffer* cmd_buffer) override;

    bool OnResize(uint32_t width, uint32_t height) override;

    void SetupBuiltinProperty(Material* mat) override;

protected:
    virtual void JitterProjection(Matrix4x4& projection);

    void DrawOptionWindow() override;

    void InitRenderTarget() override;

    void DoTAA(CommandBuffer* cmd_buffer) override;
    void DoFXAA(CommandBuffer* cmd_buffer) override;

    void AddGPass();
    void AddLightingPass();

    void InitRenderGraph(GfxDriver* gfx);

    AntiAliasingType option_aa_;
    std::shared_ptr<Program> gpass_program_;
    std::shared_ptr<Material> lighting_mat_;

    TAA taa_;
    FXAA fxaa_;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;
};

}
}
