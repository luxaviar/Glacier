#pragma once

#include "render/base/gfxdriver.h"

namespace glacier {
namespace render {

class RenderTarget;
class Camera;
class PipelineState;
class Renderable;
class Material;

class Picking {
public:
    Picking(GfxDriver* gfx);
    int Detect(int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt);

private:
    GfxDriver* gfx_;
    //RasterState rs_;
    std::shared_ptr<ConstantBuffer> color_buf_;
    std::shared_ptr<Material> mat_;
};

}
}
