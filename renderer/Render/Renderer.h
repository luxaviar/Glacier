#pragma once

#include <memory>
#include <unordered_map>
#include "Camera.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Base/Renderable.h"
#include "Editor/Editor.h"
#include "Render/PerfStats.h"
#include "Render/PostProcess.h"
#include "Algorithm/LowDiscrepancySequence.h"

namespace glacier {
namespace render {

class GfxDriver;
class CascadedShadowManager;
class OldPointLight;
class Light;

struct PerFrameData {
    Matrix4x4 _View;
    Matrix4x4 _InverseView;
    Matrix4x4 _Projection;
    Matrix4x4 _InverseProjection;
    Matrix4x4 _ViewProjection;
    Matrix4x4 _PrevViewProjection;
    Matrix4x4 _UnjitteredViewProjection;
    Vector4 _ScreenParam;
    Vector4 _CameraParams;
    Vector4 _ZBufferParams;
};

class Renderer {
public:
    static void PostProcess(const std::shared_ptr<RenderTarget>& dst, Material* mat);

    Renderer(GfxDriver* gfx, AntiAliasingType aa = AntiAliasingType::kNone);
    virtual ~Renderer() {}
    virtual void Setup();

    RenderGraph& render_graph() { return render_graph_; }
    Editor& editor() { return editor_; }

    virtual std::shared_ptr<RenderTarget>& GetLightingRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetHdrRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetLdrRenderTarget() { return ldr_render_target_; }

    Camera* GetMainCamera() const;
    PostProcessManager& GetPostProcessManager() { return post_process_manager_; }

    void CaptureScreen();
    void CaptureShadowMap();

    virtual void PreRender();
    void Render();
    virtual void PostRender();

    virtual bool OnResize(uint32_t width, uint32_t height);

    const std::vector<Renderable*>& GetVisibles() const { return visibles_; }

    GfxDriver* driver() { return gfx_; }
    void OptionWindow(bool* open);

protected:
    virtual void DrawOptionWindow() {}

    virtual void UpdatePerFrameData();
    virtual void InitRenderTarget();

    virtual void DoTAA() {}
    virtual void ResolveMSAA() {}
    virtual void HdrPostProcess() {}

    virtual void DoToneMapping();
    virtual void LdrPostProcess() {}

    virtual void DoFXAA();

    void FilterVisibles();
    void BindLightingTarget();

    void InitMaterial();

    void AddShadowPass();

    void AddCubeShadowMap(GfxDriver* gfx, OldPointLight& light);

    GfxDriver* gfx_;
    uint64_t frame_count_ = 0;
    bool init_ = false;
    uint32_t sample_count_ = 1;
    uint32_t quality_level_ = 0;

    constexpr static std::array<const char*, 4> kAADesc = { "None", "MSAA", "FXAA", "TAA" };
    RenderGraph render_graph_;
    AntiAliasingType anti_aliasing_ = AntiAliasingType::kNone;

    ConstantParameter<PerFrameData> per_frame_param_;

    //linear hdr render target
    std::shared_ptr<RenderTarget> hdr_render_target_;
    //linear ldr render target
    std::shared_ptr<RenderTarget> ldr_render_target_;

    //hardware render target
    std::shared_ptr<RenderTarget> present_render_target_;

    std::shared_ptr<CascadedShadowManager> csm_manager_;
    std::vector<Renderable*> visibles_;

    std::shared_ptr<Material> tonemapping_mat_;

    Editor editor_;
    PostProcessManager post_process_manager_;
    std::unique_ptr<PerfStats> stats_;

    Matrix4x4 prev_view_projection_;

    static constexpr int kTAASampleCount = 8;
    std::array<Vector2, kTAASampleCount> halton_sequence_;
};

}
}
