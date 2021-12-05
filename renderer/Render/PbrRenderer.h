#pragma once

#include <memory>
#include "renderer.h"

namespace glacier {
namespace render {

class GfxDriver;

class PbrRenderer : public Renderer
{
public:
    PbrRenderer(GfxDriver* gfx);
    void Setup() override;

protected:
    void InitRenderGraph(GfxDriver* gfx);
    void InitHelmetPbr(GfxDriver* gfx);
    void InitFloorPbr(GfxDriver* gfx);
    void InitDefaultPbr(GfxDriver* gfx);
};

}
}
