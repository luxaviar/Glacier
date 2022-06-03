#pragma once

#include "light.h"
#include "render/base/gfxdriver.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class Texture;
class RenderTarget;
class Material;
class Renderable;
class MaterialTemplate;

class CascadedShadowManager : public Uncopyable {
public:
    constexpr static uint32_t kMaxCascadedLevel = 8;
    constexpr static float kCascadedPartionMax = 100.0f;

    CascadedShadowManager(GfxDriver* gfx, uint32_t size, std::vector<float> cascade_partions);

    void Render(const Camera* camera, const std::vector<Renderable*> visibles,
        DirectionalLight* light);

    void SetupMaterial(MaterialTemplate* mat);
    void SetupMaterial(Material* mat);
    void Update();

    uint32_t pcf_size() const { return pcf_size_; }
    void pcf_size(uint32_t v);

    float bias() const { return shadow_param_.param().bias; }
    void bias(float v) { shadow_param_.param().bias = v; }

    float blend_area() const { return shadow_param_.param().blend_band; }
    void blend_area(float v) { shadow_param_.param().blend_band = v; }

    const std::shared_ptr<Texture>& shadow_map() const { return shadow_map_; }

    void CaptureShadowMap();
    void DrawOptionWindow();

private:
    void UpdateShadowData(const Camera* camera, DirectionalLight* light, const AABB& receiver_bounds, const AABB& scene_bounds);
    void OnCascadeChange();

    struct CascadeShadowParam {
        uint32_t cascade_levels;
        float partion_size;
        Vec2f texel_size;
        int32_t pcf_start;
        int32_t pcf_end;
        float bias = 0.001f;
        float blend_band = 0.0005f;
        float cascade_interval[kMaxCascadedLevel];
        Matrix4x4 shadow_coord_trans[kMaxCascadedLevel];
        int debug = 0;
        float padding[3];
    };

    struct CascadeData {
        std::shared_ptr<RenderTarget> render_target;
        std::vector<Renderable*> casters;
        Frustum frustum;
        Matrix4x4 projection;
        float partion;
    };

    GfxDriver* gfx_;
    uint32_t map_size_;
    uint32_t pcf_size_ = 3; //start: pcf_size_ / -2, end: pcf_size_ / 2 + 1
    
    std::shared_ptr<Texture> shadow_map_;
    std::unique_ptr<Material> material_;

    Vector3 shadow_position_;
    Matrix4x4 shadow_view_;

    ConstantParameter<CascadeShadowParam> shadow_param_;
    SamplerState shadow_sampler_;

    std::array<CascadeData, kMaxCascadedLevel> cascade_data_;
};

}
}
