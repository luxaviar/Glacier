#pragma once

#include <string>
#include "gizmos.h"

namespace glacier {

class GameObject;

namespace render {

class Material;
class Renderer;
class RenderTarget;
class CommandBuffer;

class Editor {
public:
    Editor(GfxDriver* gfx);

    bool IsSelected(GameObject* go) { return go == selected_go_; }
    bool IsSelected(Material* mat) { return mat == selected_mat_; }

    GameObject* GetSelected() { return selected_go_; }
    void SetSelected(GameObject* go) { selected_go_ = go; }

    void OnResize(uint32_t width, uint32_t height);

    void Pick(CommandBuffer* cmd_buffer, int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt);

    void RegisterHighLightPass(GfxDriver* gfx, Renderer* renderer);

    bool ShowStats() const { return show_windows_ && show_stats_; }

    void Render(CommandBuffer* cmd_buffer);

    void DrawGizmos(CommandBuffer* cmd_buffer);
    void DrawPanel();

private:
    void DrawMainMenu();
    void DrawScenePanel();
    void DrawInspectorPanel();

    GfxDriver* gfx_;

    int width_;
    int height_;

    bool show_windows_ = true;
    bool show_scene_hierachy_ = true;
    bool show_inspector_ = true;
    bool show_stats_ = true;
    bool show_imgui_demo_ = false;

    bool enable_gizmos_ = true;
    bool scene_gizmos_ = true;
    bool scene_bvh_ = false;
    bool physics_gizmos_ = false;

    bool renderer_option_window_ = false;

    std::shared_ptr<Buffer> color_buf_;
    std::shared_ptr<Material> mat_;

    GameObject* selected_go_ = nullptr;
    Material* selected_mat_ = nullptr;

    struct BlurParam {
        alignas(16) uint32_t nTaps;
        alignas(16) float coefficients[16]; //the last one is just padding
    };

    struct BlurDirection {
        uint32_t isHorizontal;
        float padding[3];
    };

    void SetKernelGauss(BlurParam& param, int radius, float sigma);

    BlurParam blur_param_;
    BlurDirection blur_dir_;
};

}
}
