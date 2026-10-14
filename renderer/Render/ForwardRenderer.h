#pragma once

#include <memory>
#include "renderer.h"
#include "PostProcess/MSAA.h"

namespace glacier {
namespace render {

class GfxDriver;
class Program;

class ForwardRenderer : public Renderer {
public:
    ForwardRenderer(GfxDriver* gfx, MSAAType msaa = MSAAType::kNone);
    void Setup() override;

    std::shared_ptr<Material> CreateLightingMaterial(const char* name) override;

    virtual std::shared_ptr<RenderTarget>& GetLightingRenderTarget() { return msaa_.GetRenderTarget(); }
    //the lighting of this renderer draws into the MSAA frame, and the passes
    //it shares with the deferred one declare their writes on that (T3.2)
    GraphResourceHandle lighting_color_handle() const override { return msaa_color_; }
    GraphResourceHandle lighting_depth_handle() const override { return msaa_depth_; }
    bool OnResize(uint32_t width, uint32_t height) override;

    void PreRender(CommandBuffer* cmd_buffer) override;

protected:
    void DrawOptionWindow() override;

    void InitRenderTarget() override;
    void ResolveMSAA(CommandBuffer* cmd_buffer) override;

    void AddPreDepthPass(GraphResourceHandle prepass_depth);
    void AddDepthNormalPass(GraphResourceHandle prepass_depth, GraphResourceHandle depth_normal);
    void AddLightingPass(GraphResourceHandle shadow_map, GraphResourceHandle occlusion);
    void AddMsaaResolvePass();
    
    void InitRenderGraph(GfxDriver* gfx);

    //the lighting of this renderer goes to the MSAA frame, and the resolve
    //that puts it on the hdr frame is a node of the graph (T3.2)
    GraphResourceHandle msaa_color_;
    GraphResourceHandle msaa_depth_;
    PassNode* msaa_resolve_pass_ = nullptr;

    MSAA msaa_;
    std::shared_ptr<Program> pbr_program_;

    //predepth pass
    std::shared_ptr<RenderTarget> prepass_render_target_;
    std::shared_ptr<Texture> depthnormal_;

    std::shared_ptr<Material> prepass_mat_;
    std::shared_ptr<Material> depthnormal_mat_;
};

}
}
