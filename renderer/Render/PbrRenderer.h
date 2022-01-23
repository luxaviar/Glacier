#pragma once

#include <memory>
#include "renderer.h"

namespace glacier {
namespace render {

class GfxDriver;
class Program;

class PbrRenderer : public Renderer
{
public:
    PbrRenderer(GfxDriver* gfx, MSAAType msaa = MSAAType::kNone);
    void Setup() override;

protected:
    void AddLightingPass();
    void SetCommonBinding(MaterialTemplate* mat);

    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);

    std::shared_ptr<MaterialTemplate> pbr_template_;
};

}
}
