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

struct alignas(16) ExposureAdaptParam {
    float min_exposure = -5;
    float max_exposure = 15;
    float rcp_exposure_range = 1.0f / 20;
    float exposure_compensation = 0.0f;
    Vector4 lum_size;
    float meter_calibration_constant = 12.5f;
    float speed_to_light = 3.0f;
    float speed_to_dark = 1.0f;
    uint32_t pixel_count;
};

class Exposure {
public:
    void Setup(Renderer* renderer);

    const std::shared_ptr<Buffer>& GetExposureBuffer() const { return exposure_buf_; }

    void OnResize(uint32_t width, uint32_t height);

    void GenerateLuminance(CommandBuffer* cmd_buffer);
    void UpdateExposure(CommandBuffer* cmd_buffer);

    void DrawOptionWindow();

protected:
    //linear hdr render target
    std::shared_ptr<RenderTarget> hdr_render_target_;

    std::shared_ptr<Material> lumin_mat_;
    std::shared_ptr<Texture> lumin_texture_;
    std::shared_ptr<Buffer> exposure_buf_;

    std::shared_ptr<Material> histogram_mat_;
    std::shared_ptr<Buffer> histogram_buf_;

    std::shared_ptr<Material> exposure_mat_;
    ConstantParameter<ExposureAdaptParam> exposure_params_;
};

}
}
