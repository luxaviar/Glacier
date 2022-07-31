#include <algorithm>
#include "CascadedShadowManager.h"
#include "Camera.h"
#include "Math/Util.h"
#include "Common/Color.h"
#include "Render/Material.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Program.h"
#include "Core/Objectmanager.h"
#include "Input/Input.h"
#include "Editor/Gizmos.h"
#include "Render/Image.h"
#include "Inspect/Profiler.h"
#include "imgui/imgui.h"
#include "Render/Base/CommandBuffer.h"

namespace glacier {
namespace render {

CascadedShadowManager::CascadedShadowManager(GfxDriver* gfx, uint32_t size, 
    std::vector<float> cascade_partions) :
    gfx_(gfx),
    map_size_(size)
{
    shadow_param_ = gfx->CreateConstantParameter<CascadeShadowParam, UsageType::kDefault>();
    auto& param = shadow_param_.param();

    uint32_t cascade_levels = (uint32_t)cascade_partions.size();
    param.cascade_levels = cascade_levels;

    assert(cascade_levels > 0 && cascade_levels <= kMaxCascadedLevel);
    auto sum = 0.0f;
    for (uint32_t i = 0; i < cascade_levels; ++i) {
        auto p = cascade_partions[i];
        assert(p >= 0.1f);
        sum += p;
        cascade_data_[i].partion = p;
    }
    assert(sum == 100.0f);

    TextureDescription tex_desc = Texture::Description()
        .SetFormat(TextureFormat::kR32_TYPELESS)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource)
        .SetDimension(size * cascade_levels, size);
    
    shadow_map_ = gfx->CreateTexture(tex_desc);
    shadow_map_->SetName("shadow map");

    OnCascadeChange();

    param.texel_size.x = 1.0f / size /cascade_levels;
    param.texel_size.y = 1.0f / size;
    param.partion_size = 1.0f / cascade_levels;

    material_ = std::make_unique<Material>("shadow", TEXT("Shadow"), nullptr);
    material_->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    shadow_sampler_.warpU = shadow_sampler_.warpV = WarpMode::kBorder;
    shadow_sampler_.filter = FilterMode::kCmpBilinear;
    shadow_sampler_.comp = RasterStateDesc::kDefaultDepthFuncWithEqual;

    pcf_size(3);
}

void CascadedShadowManager::OnCascadeChange() {
    auto cascade_levels = shadow_param_.param().cascade_levels;
    auto size = map_size_;

    for (uint32_t i = 0; i < cascade_levels; ++i) {
        auto shadow_rt = gfx_->CreateRenderTarget(size, size);
        shadow_rt->AttachDepthStencil(shadow_map_);
        ViewPort vp;
        vp.top_left_x = (float)size * i;
        vp.top_left_y = 0.0f;
        vp.width = (float)size;
        vp.height = (float)size;
        vp.min_depth = 0.0f;
        vp.max_depth = 1.0f;
        shadow_rt->viewport(vp);

        cascade_data_[i].render_target = std::move(shadow_rt);
    }
}

void CascadedShadowManager::pcf_size(uint32_t v) {
    assert(v >= 3);
    pcf_size_ = v;

    auto& param = shadow_param_.param();
    param.pcf_start = v / -2;
    param.pcf_end = v / 2 + 1;
}

void CascadedShadowManager::Render(CommandBuffer* cmd_buffer, const Camera* camera,
    const std::vector<Renderable*> visibles, DirectionalLight* light) 
{
    cascade_data_[0].render_target->ClearDepthStencil(cmd_buffer);

    int receiver_num= 0;
    AABB receiver_bounds;
    {
        PerfSample("calc receiver bound");
        for (auto o : visibles) {
            if (o->IsReciveShadow()) {
                receiver_bounds = AABB::Union(receiver_bounds, o->world_bounds());
                ++receiver_num;
            }
        }
    }

    if (receiver_num == 0) return;

    //caster culling
    {
        PerfSample("update shadow info");
        auto scene_bounds = RenderableManager::Instance()->SceneBounds();
        UpdateShadowData(camera, light, receiver_bounds, scene_bounds);
    }

    PerfSample("Render shadow");
    auto& param = shadow_param_.param();
    for (size_t i = 0; i < param.cascade_levels; ++i) {
        auto& data = cascade_data_[i];
        auto& casters = data.casters;
        if (casters.empty()) continue;

        cmd_buffer->BindCamera(shadow_position_, shadow_view_, data.projection);
        RenderTargetGuard guard(cmd_buffer, data.render_target.get());

        for (auto o : casters) {
            o->Render(cmd_buffer, material_.get());
        }
    }
}

void CascadedShadowManager::UpdateShadowData(const Camera* camera, DirectionalLight* light,
    const AABB& receiver_bounds, const AABB& scene_bounds)
{
    for (auto& data : cascade_data_) {
        data.casters.clear();
    }

    auto& light_transform = light->transform();
    shadow_position_ = light_transform.position();
    auto light_forward = light_transform.forward();
    if (light_forward.AlmostEquals(Vec3f::up)) {
        shadow_view_ = Matrix4x4::LookToLH(shadow_position_, light_forward, Vector3::back);
    }
    else {
        shadow_view_ = Matrix4x4::LookToLH(shadow_position_, light_forward, Vector3::up);
    }
    
    AABB shadow_scene_bounds = AABB::Transform(scene_bounds, shadow_view_);
    float nearz = shadow_scene_bounds.min.z;
    //float farz = shadow_bounds.max.z;

    //Fix self shadow artifacts when there is just one caster(due to float-point error)
    nearz -= 40.0f;

    AABB shadow_receiver_bounds = AABB::Transform(receiver_bounds, shadow_view_);
    float receive_farz = shadow_receiver_bounds.max.z;
    assert(receive_farz > nearz);

    Matrix4x4 coord_offset = Matrix4x4::Translate(Vec3f(0.5f, 0.5f, 0));
    Matrix4x4 coord_scale = Matrix4x4::Scale(Vec3f(0.5f, -0.5f, 1.0f));

    float camera_near_far_range = camera->farz() - camera->nearz();
    auto& param = shadow_param_.param();
    float begin_interval_percent = 0;
    for (uint32_t i = 0; i < param.cascade_levels; ++i) {
        auto& cascade_data = cascade_data_[i];
        float end_interval_percent = begin_interval_percent + cascade_data.partion * 0.01f;

        float near_interval_dist = begin_interval_percent * camera_near_far_range;
        float far_interval_dist = end_interval_percent * camera_near_far_range;
        param.cascade_interval[i] = far_interval_dist;

        Vec3f corners[(int)FrustumCorner::kCount];
        camera->FetchFrustumCorners(corners,
            near_interval_dist,
            far_interval_dist);

        begin_interval_percent = end_interval_percent;

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

        float farz = math::Min(receive_farz, ortho_max.z);

        cascade_data.projection = Matrix4x4::OrthoOffCenterLH(
            ortho_min.x, ortho_max.x,
            ortho_min.y, ortho_max.y,
#ifdef GLACIER_REVERSE_Z
            farz, nearz
#else
            nearz, farz
#endif
        );

        param.shadow_coord_trans[i] = coord_offset * coord_scale * cascade_data.projection * shadow_view_;

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

        cascade_data.frustum = ortho_corners;

        {
            PerfSample("shadow caster culling");
            RenderableManager::Instance()->Cull(cascade_data.frustum, cascade_data.casters,
                [](const Renderable* o) {
                    return o->IsCastShadow();
                });
        }
    }
}

void CascadedShadowManager::SetupMaterial(Material* mat) {
    if (mat->HasParameter("_CascadeShadowData")) {
        mat->SetProperty("_CascadeShadowData", shadow_param_);
    }
    if (mat->HasParameter("_ShadowTexture")) {
        mat->SetProperty("_ShadowTexture", shadow_map_);
    }
    if (mat->HasParameter("shadow_cmp_sampler")) {
        mat->SetProperty("shadow_cmp_sampler", shadow_sampler_);
    }
}

void CascadedShadowManager::Update() {
    shadow_param_.Update();
}

void CascadedShadowManager::DrawOptionWindow() {
    if (!ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto& param = shadow_param_.param();
    ImGui::Text("PCF Size");
    ImGui::SameLine(80);
    if (ImGui::SliderInt("##Shadow PCF Size", (int*)&pcf_size_, 3, 5)) {
        pcf_size(pcf_size_);
    }

    ImGui::Text("Bias");
    ImGui::SameLine(80);
    ImGui::SliderFloat("##Shadow Bias", &param.bias, 0.0005f, 0.01f);

    ImGui::Text("Blend Band");
    ImGui::SameLine(80);
    ImGui::SliderFloat("##Blend Band", &param.blend_band, 0.0001f, 0.001f, "%.4f");

    constexpr std::array<const char*, 3> cascades_desc = {"None", "Two Cascades","Four Cascades"};
    const char* combo_lable = cascades_desc[0];
    if (param.cascade_levels == 2) {
        combo_lable = cascades_desc[1];
    }
    else if (param.cascade_levels == 4) {
        combo_lable = cascades_desc[2];
    }

    ImGui::Text("Cascades");
    ImGui::SameLine(80);
    if (ImGui::BeginCombo("##Shadow Cascades", combo_lable, ImGuiComboFlags_PopupAlignLeft)) {
        for (int n = 0; n < cascades_desc.size(); n++) {
            const bool is_selected = ((int)param.cascade_levels == 1 << n);

            if (ImGui::Selectable(cascades_desc[n], is_selected)) {
                param.cascade_levels = 1 << n;
                OnCascadeChange();
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();

        if (param.cascade_levels == 1) {
            cascade_data_[0].partion = 100.0f;
        } else if (param.cascade_levels == 2) {
            cascade_data_[0].partion = 33.3f;
            cascade_data_[1].partion = 66.7f;
        } else if (param.cascade_levels == 4) {
            cascade_data_[0].partion = 10.6f;
            cascade_data_[1].partion = 18.6f;
            cascade_data_[2].partion = 19.2f;
            cascade_data_[3].partion = 51.6f;
        }
    }

    if (param.cascade_levels == 2) {
        std::array<float, 2> value = { cascade_data_[0].partion, cascade_data_[1].partion };
        if (ImGui::SliderFloat2("##Cascade Split2", value.data(), 0.1f, 99.9f, "%.1f")) {
            if (cascade_data_[1].partion != value[1]) {
                cascade_data_[1].partion = value[1];
                cascade_data_[0].partion = 100.0f - value[1];
            }
            else {
                cascade_data_[0].partion = value[0];
                cascade_data_[1].partion = 100.0f - value[0];
            }
        }
    } else if (param.cascade_levels == 4) {
        std::array<float, 4> value = { cascade_data_[0].partion, cascade_data_[1].partion, cascade_data_[2].partion, cascade_data_[3].partion };
        if (ImGui::SliderFloat4("##Cascade Split4", value.data(), 0.1f, 99.9f)) {
            if (cascade_data_[0].partion != value[0]) {
                auto range = cascade_data_[0].partion + cascade_data_[1].partion;
                cascade_data_[0].partion = math::Clamp(value[0], 0.1f, range - 0.1f);
                cascade_data_[1].partion = range - cascade_data_[0].partion;
            }
            else if (cascade_data_[1].partion != value[1]) {
                auto range = cascade_data_[1].partion + cascade_data_[2].partion;
                cascade_data_[1].partion = math::Clamp(value[1], 0.1f, range - 0.1f);
                cascade_data_[2].partion = range - cascade_data_[1].partion;
            }
            else if (cascade_data_[2].partion != value[2]) {
                auto range = cascade_data_[2].partion + cascade_data_[3].partion;
                cascade_data_[2].partion = math::Clamp(value[2], 0.1f, range - 0.1f);
                cascade_data_[3].partion = range - cascade_data_[2].partion;
            } else if (cascade_data_[3].partion != value[3]) {
                auto range = cascade_data_[2].partion + cascade_data_[3].partion;
                cascade_data_[3].partion = math::Clamp(value[3], 0.1f, range - 0.1f);
                cascade_data_[2].partion = range - cascade_data_[3].partion;
            }
        }
    }

    ImGui::Checkbox("Debug Cascades", (bool*)&param.debug);
}

void CascadedShadowManager::CaptureShadowMap() {
    auto& shadow_tex = shadow_map_;
    uint32_t width = shadow_map_->width();
    uint32_t height = shadow_map_->height();
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kDirect);

    shadow_map_->ReadBackImage(cmd_buffer, 0, 0, shadow_map_->width(), shadow_map_->height(), 0, 0,
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

            image.Save(TEXT("ShadowMapCaptured.png"), false);
        });
}

}
}
