#pragma once

#include <string>
#include "gizmos.h"
//#include "picking.h"

namespace glacier {

class GameObject;

namespace render {

class Material;
class Renderer;
class RenderTarget;

class Editor {
public:
    Editor(GfxDriver* gfx);

    bool IsSelected(GameObject* go) { return go == selected_go_; }
    bool IsSelected(Material* mat) { return mat == selected_mat_; }

    GameObject* GetSelected() { return selected_go_; }
    void SetSelected(GameObject* go) { selected_go_ = go; }

    void Pick(int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt);

    void RegisterHighLightPass(GfxDriver* gfx, Renderer* renderer);

    void DrawGizmos();
    void DrawPanel();

private:
    void DrawInspectorPanel();
    void DrawScenePanel();

    GfxDriver* gfx_;

    int width_;
    int height_;

    //Picking pick_;
    std::shared_ptr<ConstantBuffer> color_buf_;
    std::shared_ptr<Material> mat_;

    //std::shared_ptr<RenderTarget> pick_rt_;
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
