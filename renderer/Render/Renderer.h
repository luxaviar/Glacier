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
#include "PostProcess/GTAO.h"
#include "PostProcess/Exposure.h"
#include "PostProcess/ToneMapping.h"
#include "PostProcess/Bloom.h"

namespace glacier {
namespace render {

class GfxDriver;
class CascadedShadowManager;
class Light;
class CommandBuffer;
class PassNode;

struct alignas(16) PerFrameData {
    Matrix4x4 _View;
    Matrix4x4 _InverseView;
    Matrix4x4 _Projection;
    Matrix4x4 _InverseProjection;
    Matrix4x4 _UnjitteredInverseProjection;
    Matrix4x4 _ViewProjection;
    Matrix4x4 _UnjitteredViewProjection;
    Matrix4x4 _InverseViewProjection;
    Matrix4x4 _UnjitteredInverseViewProjection;
    Matrix4x4 _PrevViewProjection;
    Vector4 _ScreenParam;
    Vector4 _CameraParams;
    Vector4 _ZBufferParams;
    float _DeltaTime;
    Vector3 padding;
};

class Renderer {
public:
    static void PostProcess(CommandBuffer* cmd_buffer, const std::shared_ptr<RenderTarget>& dst, Material* mat, bool color_only = false);

    Renderer(GfxDriver* gfx, AntiAliasingType aa = AntiAliasingType::kNone);
    virtual ~Renderer() {}
    virtual void Setup();

    RenderGraph& render_graph() { return render_graph_; }
    Editor& editor() { return editor_; }
    void SetAntiAliasingType(AntiAliasingType v) { anti_aliasing_ = v; }
    auto frame_count() const { return frame_count_; }

    virtual std::shared_ptr<RenderTarget>& GetLightingRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetHdrRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetLdrRenderTarget() { return ldr_render_target_; }
    std::shared_ptr<RenderTarget>& GetPresentRenderTarget() { return present_render_target_; }

    // The imported targets the passes of the graph declare (see
    // .docs/render-graph-plan.md T1.7)
    GraphResourceHandle hdr_color_handle() const { return hdr_color_; }
    GraphResourceHandle hdr_depth_handle() const { return hdr_depth_; }

    Camera* GetMainCamera() const;
    PostProcessManager& GetPostProcessManager() { return post_process_manager_; }

    virtual void PreRender(CommandBuffer* cmd_buffer);
    void Render(float delta_time);
    virtual void PostRender(CommandBuffer* cmd_buffer);

    virtual std::shared_ptr<Material> CreateLightingMaterial(const char* name) = 0;
    virtual bool OnResize(uint32_t width, uint32_t height);

    virtual void SetupBuiltinProperty(Material* mat);

    const std::vector<Renderable*>& GetVisibles() const { return visibles_; }

    GfxDriver* driver() { return gfx_; }
    //what the last frames rendered: the frame time, the gpu time and what the
    //draws of the frame came to (see PerfStats)
    PerfStats* stats() { return stats_.get(); }
    void OptionWindow(bool* open);

    void CaptureScreen();
    void CaptureShadowMap();
    void BindLightingTarget(CommandBuffer* cmd_buffer);
    void BindMainCamera(CommandBuffer* cmd_buffer);

protected:
    virtual void DrawOptionWindow() {}

    virtual void UpdatePerFrameData();
    virtual void JitterProjection(Matrix4x4& projection) {}
    virtual void InitRenderTarget();

    virtual void DoTAA(CommandBuffer* cmd_buffer) {}
    virtual void ResolveMSAA(CommandBuffer* cmd_buffer) {}
    
    virtual void HdrPostProcess(CommandBuffer* cmd_buffer);
    virtual void LdrPostProcess(CommandBuffer* cmd_buffer) {}

    virtual void DoFXAA(CommandBuffer* cmd_buffer);

    void FilterVisibles();

    void InitMaterial();

    void AddShadowPass(GraphResourceHandle shadow_map);
    void AddGtaoPass(GraphResourceHandle depth, GraphResourceHandle normal, GraphResourceHandle occlusion);

    void InitFloorPbr(CommandBuffer* cmd_buffer);
    void InitDefaultPbr(CommandBuffer* cmd_buffer);

    GfxDriver* gfx_;
    uint64_t frame_count_ = 0;
    float delta_time_ = 1.0 / 60;

    constexpr static std::array<const char*, 4> kAADesc = { "None", "MSAA", "FXAA", "TAA" };
    RenderGraph render_graph_;
    AntiAliasingType anti_aliasing_ = AntiAliasingType::kNone;

    ConstantParameter<PerFrameData> per_frame_param_;

    //linear hdr render target
    std::shared_ptr<RenderTarget> hdr_render_target_;
    //linear ldr render target
    std::shared_ptr<RenderTarget> ldr_render_target_;

    //what the graph borrows from the renderer (see RenderGraph::Import)
    GraphResourceHandle hdr_color_;
    GraphResourceHandle hdr_depth_;

    //the transients of the AO chain (T2.4), registered by AddGtaoPass
    GraphResourceHandle ao_half_;
    GraphResourceHandle ao_spatial_;
    //the upsample node of the chain, switched by the half resolution option
    //before the graph of a frame runs (T2.2)
    PassNode* gtao_upsample_pass_ = nullptr;

    //hardware render target
    std::shared_ptr<RenderTarget> present_render_target_;

    std::shared_ptr<CascadedShadowManager> csm_manager_;
    std::vector<Renderable*> visibles_;

    Exposure exposure_;
    GTAO gtao_;
    ToneMapping tonemapping_;
    Bloom bloom_;

    Editor editor_;
    PostProcessManager post_process_manager_;
    std::unique_ptr<PerfStats> stats_;

    Matrix4x4 prev_view_projection_;

};

}
}
