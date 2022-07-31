#pragma once

#include <memory>
#include "renderer.h"

namespace glacier {
namespace render {

class GfxDriver;
class Program;

class ForwardRenderer : public Renderer {
public:
    ForwardRenderer(GfxDriver* gfx, MSAAType msaa = MSAAType::kNone);
    void Setup() override;

    std::shared_ptr<Material> CreateLightingMaterial(const char* name) override;

    virtual std::shared_ptr<RenderTarget>& GetLightingRenderTarget() { return msaa_render_target_; }
    bool OnResize(uint32_t width, uint32_t height) override;

    void PreRender(CommandBuffer* cmd_buffer) override;

protected:
    void DrawOptionWindow() override;
    void InitMSAA();
    void OnChangeMSAA();

    void InitRenderTarget() override;
    void ResolveMSAA(CommandBuffer* cmd_buffer) override;

    void AddPreDepthPass();
    void AddDepthNormalPass();
    void AddLightingPass();
    
    void InitRenderGraph(GfxDriver* gfx);

    constexpr static std::array<const char*, 4> kMsaaDesc = { "None", "2x", "4x", "8x" };
    MSAAType msaa_ = MSAAType::kNone;
    MSAAType option_msaa_ = MSAAType::kNone;

    std::shared_ptr<Program> pbr_program_;

    //predepth pass
    std::shared_ptr<RenderTarget> prepass_render_target_;
    std::shared_ptr<Texture> depthnormal_;

    std::shared_ptr<Material> prepass_mat_;
    std::shared_ptr<Material> depthnormal_mat_;
    //std::shared_ptr<Material> ao_mat_;

    //intermediate (MSAA) render target
    std::shared_ptr<RenderTarget> msaa_render_target_;
    //for msaa resolve
    std::array<std::shared_ptr<Material>, (int)MSAAType::kMax> msaa_resolve_mat_; // 0 is no use
};

}
}
