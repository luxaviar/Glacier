#include "CascadedShadowManager.h"
#include "Camera.h"
#include "Math/Util.h"
#include "Common/Color.h"
#include "Render/Material.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/ConstantBuffer.h"
#include "Render/Base/Sampler.h"
#include "Core/Objectmanager.h"
#include "Input/Input.h"
#include "Editor/Gizmos.h"
#include "Render/Image.h"

namespace glacier {
namespace render {

CascadedShadowManager::CascadedShadowManager(GfxDriver* gfx, uint32_t size, 
    std::vector<float> cascade_partions) :
    gfx_(gfx),
    map_size_(size),
    cascade_partions_(cascade_partions)
{
    shadow_cbuffer_ = gfx->CreateConstantBuffer<CascadeShadowData>();

    uint32_t cascade_levels = (uint32_t)cascade_partions.size();
    shadow_data_.cascade_levels = cascade_levels;

    assert(cascade_levels > 0 && cascade_levels <= kMaxCascadedLevel);
    for (uint32_t i = 1; i < cascade_levels; ++i) {
        assert(cascade_partions[i - 1] < cascade_partions[i]);
    }
    assert(cascade_partions.front() > 0.0f);
    assert(cascade_partions.back() == 100.0f);

    TextureBuilder tex_builder = Texture::Builder()
        .SetFormat(TextureFormat::kR32_TYPELESS)
        .SetCreateFlag(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE)
        .SetDimension(size * cascade_levels, size);
    
    shadow_map_ = gfx->CreateTexture(tex_builder);

    shadow_data_.texel_size.x = 1.0f / size /cascade_levels;
    shadow_data_.texel_size.y = 1.0f / size;
    shadow_data_.partion_size = 1.0f / cascade_levels;

    render_targets_.reserve(cascade_levels);
    for (uint32_t i = 0; i < cascade_levels; ++i) {
        auto shadow_rt = gfx->CreateRenderTarget(size, size);
        shadow_rt->AttachDepthStencil(shadow_map_);
        ViewPort vp;
        vp.top_left_x = (float)size * i;
        vp.top_left_y = 0.0f;
        vp.width = (float)size;
        vp.height = (float)size;
        vp.min_depth = 0.0f;
        vp.max_depth = 1.0f;
        shadow_rt->viewport(vp);

        render_targets_.emplace_back(std::move(shadow_rt));
    }

    material_ = std::make_unique<Material>("shadow", TEXT("Shadow"));

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kBorder;
    ss.filter = FilterMode::kCmpBilinear;
    ss.comp = CompareFunc::kLessEqual;

    shadow_sampler_ = gfx->CreateSampler(ss);

    pcf_size(3);

    frustums_.reserve(cascade_levels);
}

void CascadedShadowManager::pcf_size(uint32_t v) {
    assert(v >= 3);
    pcf_size_ = v;
    shadow_data_.pcf_start = v / -2;
    shadow_data_.pcf_end = v / 2 + 1;
}

void CascadedShadowManager::Render(const Camera* camera,
    const std::vector<Renderable*> visibles, DirectionalLight* light) 
{
    render_targets_[0]->ClearDepthStencil();

    //cull objects
    AABB reciver_bounds;
    for (auto o : visibles) {
        if (o->IsReciveShadow()) {
            reciver_bounds = AABB::Union(reciver_bounds, o->world_bounds());
        }
    }

    AABB caster_bounds;
    casters_.clear();

    auto& renderables = RenderableManager::Instance()->GetList();
    for (auto& node : renderables) {
        auto& o = node.data;
        if (o->IsActive() && o->IsCastShadow()) {
            caster_bounds = AABB::Union(caster_bounds, o->world_bounds());
            casters_.push_back(o);
        }
    }

    if (casters_.empty()) return;

    UpdateShadowInfo(camera, light, AABB::Union(caster_bounds, reciver_bounds));

    MaterialGuard guard(gfx_, material_.get());
    for (size_t i = 0; i < shadow_data_.cascade_levels; ++i) {
        gfx_->BindCamera(shadow_view_, shadow_proj_[i]);
        RenderTargetGuard guard(render_targets_[i].get());

        auto& shadow_frustum = frustums_[i];
        for (auto o : casters_) {
            if (shadow_frustum.Intersect(o->world_bounds())) {
                o->Render(gfx_);
            }
        }
    }
}

void CascadedShadowManager::UpdateShadowInfo(const Camera* camera, 
    DirectionalLight* light, const AABB& scene_bounds) 
{
    auto& light_camera = light->camera();

    shadow_view_ = light_camera.view();
    //auto light_space_tx = light_camera.transform().WorldToLocalMatrix();

    AABB shadow_bounds = AABB::Transform(scene_bounds, shadow_view_);
    float nearz = shadow_bounds.min.z;
    float farz = shadow_bounds.max.z;

    Matrix4x4 coord_offset = Matrix4x4::Translate(Vec3f(0.5f, 0.5f, 0));
    Matrix4x4 coord_scale = Matrix4x4::Scale(Vec3f(0.5f, -0.5f, 1.0f));

    frustums_.clear();

    bool draw = Input::IsJustKeyDown(Keyboard::F5);
    static std::vector<Vec3f> gizmos_corners;

    if (draw) {
        gizmos_corners.clear();
    }

    float camera_near_far_range = camera->farz() - camera->nearz();
    for (uint32_t i = 0; i < shadow_data_.cascade_levels; ++i) {
        float interval_begin = 0;
        if (i > 0) interval_begin = cascade_partions_[i - 1];
        float interval_end = cascade_partions_[i];

        interval_begin /= kCascadedPartionMax;
        interval_end /= kCascadedPartionMax;

        interval_begin *= camera_near_far_range;
        interval_end *= camera_near_far_range;
        shadow_data_.cascade_interval[i] = interval_end;

        Vec3f corners[(int)FrustumCorner::kCount];
        camera->FetchFrustumCorners(corners, interval_begin, interval_end);

        Vec3f ortho_min(FLT_MAX);
        Vec3f ortho_max(-FLT_MAX);

        for (int i = 0; i < (int)FrustumCorner::kCount; ++i) {
            corners[i] = light_camera.WorldToCamera(corners[i]);
            ortho_min = Vec3f::Min(ortho_min, corners[i]);
            ortho_max = Vec3f::Max(ortho_max, corners[i]);
        }

        // We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
        // sampling within the correct map.
        auto pcf_blur_amount = pcf_size_ * 2 + 1;
        auto pcf_border_scale = (float)pcf_blur_amount / (float)map_size_;
        auto border_offset = (ortho_max - ortho_min) * pcf_border_scale * 0.5f;
        ortho_min -= border_offset;
        ortho_max += border_offset;

        // The world units per texel are used to snap  the orthographic projection
        // to texel sized increments.  
        // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
        // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
        auto unit_per_texel = (ortho_max - ortho_min) / (float)map_size_;

        ortho_min /= unit_per_texel;
        ortho_min = Vec3f::Floor(ortho_min);
        ortho_min *= unit_per_texel;

        ortho_max /= unit_per_texel;
        ortho_max = Vec3f::Floor(ortho_max);
        ortho_max *= unit_per_texel;

        shadow_proj_[i] = Matrix4x4::OrthoOffCenterLH(
            ortho_min.x, ortho_max.x,
            ortho_min.y, ortho_max.y,
            nearz, farz
        );

        shadow_data_.shadow_coord_trans[i] = coord_offset * coord_scale * shadow_proj_[i] * shadow_view_;

        Vec3f ortho_corners[(int)FrustumCorner::kCount] = {
            {ortho_min.x, ortho_min.y, nearz},
            {ortho_min.x, ortho_max.y, nearz},
            {ortho_max.x, ortho_max.y, nearz},
            {ortho_max.x, ortho_min.y, nearz},
            {ortho_min.x, ortho_min.y, farz},
            {ortho_min.x, ortho_max.y, farz},
            {ortho_max.x, ortho_max.y, farz},
            {ortho_max.x, ortho_min.y, farz},
        };

        for (auto& p : ortho_corners) {
            p = light_camera.CameraToWorld(p);
        }

        if (draw) {
            for (auto p : ortho_corners) {
                gizmos_corners.push_back(p);
            }
        }

        frustums_.emplace_back(ortho_corners);
    }

    if (!gizmos_corners.empty()) {
        auto gizmos = Gizmos::Instance();
        gizmos->SetColor(Color::kRed);
        for (size_t i = 0; i < gizmos_corners.size(); i += 8) {
            gizmos->DrawFrustum(gizmos_corners.data() + i);
        }
    }
}

void CascadedShadowManager::Bind() {
    shadow_cbuffer_->Update(&shadow_data_);
    shadow_cbuffer_->Bind(ShaderType::kPixel, 0);

    shadow_map_->Bind(ShaderType::kPixel, 0);
    shadow_sampler_->Bind(ShaderType::kPixel, 0);
}

void CascadedShadowManager::UnBind() {
    shadow_cbuffer_->UnBind(ShaderType::kPixel, 0);
    shadow_map_->UnBind(ShaderType::kPixel, 0);
    shadow_sampler_->UnBind(ShaderType::kPixel, 0);
}

void CascadedShadowManager::GrabShadowMap() {
    auto& shadow_tex = shadow_map_;
    Image tmp_img(shadow_map_->width(), shadow_map_->height());
    shadow_map_->ReadBackImage(tmp_img, 0, 0, shadow_map_->width(), shadow_map_->height(), 0, 0,
        [](const uint8_t* texel, ColorRGBA32* pixel) {
            float v = *(const float*)texel;
            uint8_t c = (uint8_t)((v / 1.0f) * 255);
            *pixel = ColorRGBA32(c, c, c, 255);
            return sizeof(float);
        });
    tmp_img.Save(TEXT("shadowmap_grab.png"), false);
}

}
}
