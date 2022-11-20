#pragma once

#include <memory>
#include "Math/Vec4.h"
#include "Math/Vec2.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTarget.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;
class DeferredRenderer;
class Renderer;
class Material;

struct FXAAParam {
    float contrast_threshold;
    float relative_threshold;
    float subpixel_blending;
    float padding;
};

class FXAA {
public:
    void Setup(DeferredRenderer* renderer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);

    void DrawOptionWindow();

protected:
    std::shared_ptr<Material> fxaa_mat_;
    ConstantParameter<FXAAParam> fxaa_param_;

};

}
}
