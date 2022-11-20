#pragma once

#include <memory>
#include "Math/Vec4.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTarget.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;
class Renderer;
class Material;

struct alignas(16) GtaoParam {
    float temporal_cos_angle = 1.0f;
    float temporal_sin_angle = 0.0f;
    float temporal_offset = 0.0f;
    float temporal_direction = 0.0f;
    float radius = 5.0f;
    float fade_to_radius = 2.0f;
    float thickness = 1.0f;
    float fov_scale;
    Vec4f render_param;
    float intensity = 1.0f;
    float sharpness = 0.2f;
    int debug_ao = 0;
    int debug_ro = 0;
};

class GTAO {
public:
    void Setup(Renderer* renderer, const std::shared_ptr<Texture>& depth_buffer, const std::shared_ptr<Texture>& normal_buffer);
    void Execute(Renderer* renderer, CommandBuffer* cmd_buffer);
    void OnResize(uint32_t width, uint32_t height);

    void SetupBuiltinProperty(Material* mat);
    void DrawOptionWindow();

protected:
    bool half_ao_res_ = false;

    ConstantParameter<GtaoParam> gtao_param_;
    std::shared_ptr<Material> gtao_mat_;
    std::shared_ptr<Material> gtao_upsampling_mat_;
    std::shared_ptr<Material> gtao_filter_x_mat_;
    std::shared_ptr<Material> gtao_filter_y_mat_;
    ConstantParameter<Vector4> gtao_filter_x_param_;
    ConstantParameter<Vector4> gtao_filter_y_param_;

    std::shared_ptr<RenderTarget> ao_half_render_target_;
    std::shared_ptr<RenderTarget> ao_full_render_target_;
    std::shared_ptr<RenderTarget> ao_spatial_render_target_;
};

}
}
