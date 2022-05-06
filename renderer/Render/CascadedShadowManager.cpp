#include "CascadedShadowManager.h"
#include "Camera.h"
#include "Math/Util.h"
#include "Common/Color.h"
#include "Render/Material.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SamplerState.h"
#include "Core/Objectmanager.h"
#include "Input/Input.h"
#include "Editor/Gizmos.h"
#include "Render/Image.h"
#include "Inspect/Profiler.h"

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

    TextureDescription tex_desc = Texture::Description()
        .SetFormat(TextureFormat::kR32_TYPELESS)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource)
        .SetDimension(size * cascade_levels, size);
    
    shadow_map_ = gfx->CreateTexture(tex_desc);
    shadow_map_->SetName(TEXT("shadow map"));

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
    material_->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));
    material_->GetTemplate()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    shadow_sampler_.warpU = shadow_sampler_.warpV = WarpMode::kBorder;
    shadow_sampler_.filter = FilterMode::kCmpBilinear;
    shadow_sampler_.comp = RasterStateDesc::kDefaultDepthFuncWithEqual;

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
    {
        PerfSample("calc reciver bound");
        for (auto o : visibles) {
            if (o->IsReciveShadow()) {
                reciver_bounds = AABB::Union(reciver_bounds, o->world_bounds());
            }
        }
    }

    AABB caster_bounds;
    casters_.clear();

    {
        PerfSample("calc caster bound");
        auto& renderables = RenderableManager::Instance()->GetList();
        for (auto& node : renderables) {
            auto& o = node.data;
            if (o->IsActive() && o->IsCastShadow()) {
                caster_bounds = AABB::Union(caster_bounds, o->world_bounds());
                casters_.push_back(o);
            }
        }
    }

    if (casters_.empty()) return;

    {
        PerfSample("update shadow info");
        UpdateShadowInfo(camera, light, AABB::Union(caster_bounds, reciver_bounds));
    }

    PerfSample("Render shadow");
    for (size_t i = 0; i < shadow_data_.cascade_levels; ++i) {
        gfx_->BindCamera(shadow_position_, shadow_view_, shadow_proj_[i]);
        RenderTargetBindingGuard guard(gfx_, render_targets_[i].get());

        auto& shadow_frustum = frustums_[i];
        for (auto o : casters_) {
            if (shadow_frustum.Intersect(o->world_bounds())) {
                o->Render(gfx_, material_.get());
            }
        }
    }
}

void CascadedShadowManager::UpdateShadowInfo(const Camera* camera, 
    DirectionalLight* light, const AABB& scene_bounds) 
{
    auto& light_transform = light->transform();
    shadow_position_ = light_transform.position();
    shadow_view_ = Matrix4x4::LookToLH(shadow_position_, light_transform.forward(), Vec3f::up);
    
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
            corners[i] = light_transform.InverseTransform(corners[i]); //world space to light camera space;
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
#ifdef GLACIER_REVERSE_Z
            farz, nearz
#else
            nearz, farz
#endif
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
            p = light_transform.ApplyTransform(p);// light camera space to world space;
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

void CascadedShadowManager::SetupMaterial(MaterialTemplate* mat) {
    mat->SetProperty("CascadeShadowData", shadow_cbuffer_);
    mat->SetProperty("ShadowTexture_", shadow_map_);
    mat->SetProperty("shadow_cmp_sampler", shadow_sampler_);
}

void CascadedShadowManager::SetupMaterial(Material* mat) {
    mat->SetProperty("CascadeShadowData", shadow_cbuffer_);
    mat->SetProperty("ShadowTexture_", shadow_map_);
    mat->SetProperty("shadow_cmp_sampler", shadow_sampler_);
}

void CascadedShadowManager::Update() {
    shadow_cbuffer_->Update(&shadow_data_);
}

void CascadedShadowManager::GrabShadowMap() {
    auto& shadow_tex = shadow_map_;
    uint32_t width = shadow_map_->width();
    uint32_t height = shadow_map_->height();

    shadow_map_->ReadBackImage(0, 0, shadow_map_->width(), shadow_map_->height(), 0, 0,
        [width, height, this](const uint8_t* data, size_t raw_pitch) {
            Image image(width, height, false);

            for (int y = 0; y < height; ++y) {
                const uint8_t* texel = data;
                auto pixel = image.GetRawPtr<ColorRGBA32>(0, y);
                for (int x = 0; x < width; ++x) {
                    auto v = *((const float*)texel);
                    uint8_t c = (uint8_t)((v / 1.0f) * 255);
                    *pixel = ColorRGBA32(c, c, c, 255);

                    texel += sizeof(uint32_t);
                    ++pixel;
                }
                data += raw_pitch;
            }

            image.Save(TEXT("shadowmap_grab.png"), false);
        });
}

}
}
