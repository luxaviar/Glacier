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
    Renderer(GfxDriver* gfx);
    virtual ~Renderer() {}
    virtual void Setup();

    RenderGraph& render_graph() { return render_graph_; }
    Editor& editor() { return editor_; }

    std::shared_ptr<RenderTarget>& render_target() { return render_target_; }

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
    virtual void InitToneMapping();
    virtual void InitRenderTarget();
    virtual void ResolveMSAA() {}

    void FilterVisibles();
    void RestoreCommonBindings();

    void InitMaterial();

    void AddShadowPass();

    void AddCubeShadowMap(GfxDriver* gfx, OldPointLight& light);

    GfxDriver* gfx_;
    bool init_ = false;
    bool show_imgui_demo_ = false;
    bool show_gui_ = true;
    bool show_gizmo_ = true;
    uint32_t sample_count_ = 1;
    uint32_t quality_level_ = 0;

    RenderGraph render_graph_;

    //linear render target
    std::shared_ptr<RenderTarget> render_target_;

    //hardware render target
    std::shared_ptr<RenderTarget> present_render_target_;

    std::shared_ptr<CascadedShadowManager> csm_manager_;
    std::vector<Renderable*> visibles_;

    Editor editor_;
    PostProcessManager post_process_manager_;
    std::unique_ptr<PerfStats> stats_;
};

}
}
