#pragma once

#include <memory>
#include <unordered_map>
#include "Camera.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Base/Renderable.h"
#include "Editor/Editor.h"
#include "Render/Base/PerfStats.h"
#include "Render/PostProcess.h"

namespace glacier {
namespace render {

class GfxDriver;
class CascadedShadowManager;
class OldPointLight;
class Light;

class Renderer {
public:
    Renderer(GfxDriver* gfx, MSAAType msaa = MSAAType::kNone);
    virtual ~Renderer() {}
    virtual void Setup();

    RenderGraph& render_graph() { return render_graph_; }
    Editor& editor() { return editor_; }

    std::shared_ptr<RenderTarget>& render_target() { return render_target_; }
    std::shared_ptr<RenderTarget>& intermediate_target() { return intermediate_target_; }

    Camera* GetMainCamera() const;
    PostProcessManager& GetPostProcessManager() { return post_process_manager_; }

    void GrabScreen();

    void PreRender();
    void Render();
    void PostRender();

    void OnResize(uint32_t width, uint32_t height);

    const std::vector<Renderable*>& GetVisibles() const { return visibles_; }

    GfxDriver* driver() { return gfx_; }

protected:
    void InitMSAA();
    void InitPostProcess();

    void ResolveMSAA();
    void FilterVisibles();
    void RestoreCommonBindings();

    void InitMaterial();
    void InitRenderTarget();

    void AddSolidPass();
    void AddShadowPass();
    void AddPhongPass();
    void AddPbrPass();

    void AddCubeShadowMap(GfxDriver* gfx, OldPointLight& light);

    GfxDriver* gfx_;
    bool show_imgui_demo_ = false;
    bool show_gui_ = true;
    bool show_gizmo_ = true;
    MSAAType msaa_ = MSAAType::kNone;
    uint32_t sample_count_ = 1;
    uint32_t quality_level_ = 0;

    RenderGraph render_graph_;

    std::shared_ptr<RenderTarget> render_target_;
    //intermediate render target
    std::shared_ptr<RenderTarget> intermediate_target_;

    //for msaa resolve
    std::shared_ptr<Material> msaa_resolve_mat_[4]; // 0 is no use

    std::shared_ptr<CascadedShadowManager> csm_manager_;
    std::vector<Renderable*> visibles_;

    Editor editor_;
    PostProcessManager post_process_manager_;
    std::unique_ptr<PerfStats> stats_;
};

}
}
