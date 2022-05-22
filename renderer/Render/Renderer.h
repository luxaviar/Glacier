#pragma once

#include <memory>
#include <unordered_map>
#include "Camera.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Base/Renderable.h"
#include "Editor/Editor.h"
#include "Render/PerfStats.h"
#include "Render/PostProcess.h"

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

    Renderer(GfxDriver* gfx);
    virtual ~Renderer() {}
    virtual void Setup();

    RenderGraph& render_graph() { return render_graph_; }
    Editor& editor() { return editor_; }

    virtual std::shared_ptr<RenderTarget>& GetLightingRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetHdrRenderTarget() { return hdr_render_target_; }
    std::shared_ptr<RenderTarget>& GetLdrRenderTarget() { return ldr_render_target_; }

    Camera* GetMainCamera() const;
    PostProcessManager& GetPostProcessManager() { return post_process_manager_; }

    void GrabScreen();

    virtual void PreRender();
    void Render();
    virtual void PostRender();

    virtual bool OnResize(uint32_t width, uint32_t height);

    const std::vector<Renderable*>& GetVisibles() const { return visibles_; }

    GfxDriver* driver() { return gfx_; }

protected:
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
    bool show_imgui_demo_ = false;
    bool show_gui_ = true;
    bool show_gizmo_ = true;
    uint32_t sample_count_ = 1;
    uint32_t quality_level_ = 0;

    RenderGraph render_graph_;

    ConstantParameter<PerFrameData> per_frame_param_;
    //PerFrameData per_frame_data_;
    //std::shared_ptr<Buffer> per_frame_cbuffer_;

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
};

}
}
