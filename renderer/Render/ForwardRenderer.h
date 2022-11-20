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
    bool OnResize(uint32_t width, uint32_t height) override;

    void PreRender(CommandBuffer* cmd_buffer) override;

protected:
    void DrawOptionWindow() override;

    void InitRenderTarget() override;
    void ResolveMSAA(CommandBuffer* cmd_buffer) override;

    void AddPreDepthPass();
    void AddDepthNormalPass();
    void AddLightingPass();
    
    void InitRenderGraph(GfxDriver* gfx);

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
