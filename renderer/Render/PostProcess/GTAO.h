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
    void OnResize(uint32_t width, uint32_t height);

    // The chain runs as four nodes of the graph: the trace, the
    // upsample that puts a half resolution trace back on the full target, and
    // the two directions of the bilateral filter. The trace target of the half
    // resolution mode and the stage between the two directions are transients
    // the graph owns: the node that renders into one binds what the
    // frame resolved, the node that samples one points its material at it -
    // the texture of a transient belongs to the frame, not to the material.
    void BindHalfTarget(const std::shared_ptr<Texture>& texture);
    void BindSpatialTarget(const std::shared_ptr<Texture>& texture);

    void ExecuteTrace(Renderer* renderer, CommandBuffer* cmd_buffer);
    void ExecuteUpsample(CommandBuffer* cmd_buffer);
    void ExecuteFilterX(CommandBuffer* cmd_buffer);
    void ExecuteFilterY(CommandBuffer* cmd_buffer);

    //what the "Half Resolution" checkbox of the AO panel says; the upsample
    //node of the graph follows it
    bool half_resolution() const { return half_ao_res_; }

    void SetupBuiltinProperty(Material* mat);
    void DrawOptionWindow();

    //the occlusion the lighting pass reads; imported by the renderer's graph
    //registration
    std::shared_ptr<Texture>& occlusion_texture() {
        return ao_full_render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    }

protected:
    void UpdateParam(Renderer* renderer);
    void BindTarget(RenderTarget* target, const std::shared_ptr<Texture>& texture);

    // The trace is the heaviest thing the frame runs: eight angles of ten taps,
    // each tap reconstructing the view position of two pixels. It is also a
    // low frequency signal, which is why the upsample and the bilateral filter
    // are there at all - running the trace on the half resolution target and
    // putting it back at full resolution with them costs a quarter of it.
    // The checkbox of the AO panel still turns it off for a comparison.
    bool half_ao_res_ = true;

    ConstantParameter<GtaoParam> gtao_param_;
    std::shared_ptr<Material> gtao_mat_;
    std::shared_ptr<Material> gtao_upsampling_mat_;
    std::shared_ptr<Material> gtao_filter_x_mat_;
    std::shared_ptr<Material> gtao_filter_y_mat_;
    ConstantParameter<Vector4> gtao_filter_x_param_;
    ConstantParameter<Vector4> gtao_filter_y_param_;

    // the full target is the one the lighting pass reads and the renderer
    // imports; the half and the spatial target carry the transients of the
    // frame and are attached to them while the frame runs
    std::shared_ptr<RenderTarget> ao_half_render_target_;
    std::shared_ptr<RenderTarget> ao_full_render_target_;
    std::shared_ptr<RenderTarget> ao_spatial_render_target_;
};

}
}
