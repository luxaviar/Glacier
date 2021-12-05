#pragma once

#include "light.h"
#include "render/base/gfxdriver.h"

namespace glacier {
namespace render {

class Texture;
class RenderTarget;
class Material;
class Renderable;

class CascadedShadowManager : public Uncopyable {
public:
    constexpr static uint32_t kMaxCascadedLevel = 8;
    constexpr static float kCascadedPartionMax = 100.0f;

    CascadedShadowManager(GfxDriver* gfx, uint32_t size, std::vector<float> cascade_partions);

    void Render(const Camera* camera, const std::vector<Renderable*> visibles,
        DirectionalLight* light);

    void Bind();
    void UnBind();

    uint32_t pcf_size() const { return pcf_size_; }
    void pcf_size(uint32_t v);

    float bias() const { return shadow_data_.bias; }
    void bias(float v) { shadow_data_.bias = v; }

    float blend_area() const { return shadow_data_.blend_band; }
    void blend_area(float v) { shadow_data_.blend_band = v; }

    const std::shared_ptr<Texture>& shadow_map() const { return shadow_map_; }

    void GrabShadowMap();

private:
    void UpdateShadowInfo(const Camera* camera, DirectionalLight* light, const AABB& shadow_bounds);

    struct CascadeShadowData {
        uint32_t cascade_levels;
        float partion_size;
        Vec2f texel_size;
        int32_t pcf_start;
        int32_t pcf_end;
        float bias = 0.001f;
        float blend_band = 0.0005f;
        float cascade_interval[kMaxCascadedLevel];
        Matrix4x4 shadow_coord_trans[kMaxCascadedLevel];
    };

    GfxDriver* gfx_;
    uint32_t map_size_;
    uint32_t pcf_size_ = 3; //start: pcf_size_ / -2, end: pcf_size_ / 2 + 1
    std::vector<float> cascade_partions_;
    
    std::shared_ptr<Texture> shadow_map_;
    std::vector<std::shared_ptr<RenderTarget>> render_targets_;
    std::unique_ptr<Material> material_;

    Matrix4x4 shadow_view_;
    Matrix4x4 shadow_proj_[kMaxCascadedLevel];

    CascadeShadowData shadow_data_;
    std::shared_ptr<ConstantBuffer> shadow_cbuffer_;
    std::shared_ptr<Sampler> shadow_sampler_;

    std::vector<Renderable*> casters_;
    std::vector<Frustum> frustums_;
};

}
}
