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

    void AddGPass(GraphResourceHandle albedo, GraphResourceHandle normal,
        GraphResourceHandle ao_metal_roughness, GraphResourceHandle emissive, GraphResourceHandle velocity);
    void AddLightingPass(GraphResourceHandle albedo, GraphResourceHandle normal,
        GraphResourceHandle ao_metal_roughness, GraphResourceHandle emissive,
        GraphResourceHandle occlusion, GraphResourceHandle shadow_map);
    void AddTaaPass();

    void InitRenderGraph(GfxDriver* gfx);

    AntiAliasingType option_aa_;
    std::shared_ptr<Program> gpass_program_;
    std::shared_ptr<Material> lighting_mat_;
    GraphResourceHandle gbuffer_velocity_;

    TAA taa_;
    FXAA fxaa_;
    //the TAA node, switched by the AA option before the graph runs (T3.1)
    PassNode* taa_pass_ = nullptr;

    std::shared_ptr<RenderTarget> gbuffer_render_target_;
};

}
}
