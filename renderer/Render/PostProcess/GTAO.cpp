#include "GTAO.h"
#include <assert.h>
#include <algorithm>
#include "Render/Renderer.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/Program.h"
#include "Render/Base/RenderTexturePool.h"
#include "Common/Log.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/CommandQueue.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

void GTAO::Setup(Renderer* renderer, const std::shared_ptr<Texture>& depth_buffer, const std::shared_ptr<Texture>& normal_buffer)
{
    auto& hdr_render_target = renderer->GetHdrRenderTarget();
    auto width = hdr_render_target->width();
    auto height = hdr_render_target->height();

    auto gfx_ = renderer->driver();
    gtao_param_ = gfx_->CreateConstantParameter<GtaoParam, UsageType::kDefault>();
    gtao_filter_x_param_ = gfx_->CreateConstantParameter<Vector4, UsageType::kDefault>(1.0f, 0.0f, 0.0f, 0.0f);
    gtao_filter_y_param_ = gfx_->CreateConstantParameter<Vector4, UsageType::kDefault>(0.0f, 1.0f, 0.0f, 0.0f);

    auto ao_texture = RenderTexturePool::Get(width, height, TextureFormat::kR11G11B10_FLOAT);
    ao_full_render_target_ = gfx_->CreateRenderTarget(width, height);
    ao_full_render_target_->AttachColor(AttachmentPoint::kColor0, ao_texture);
    ao_texture->SetName("ao texture");

    auto ao_half_texture = RenderTexturePool::Get(width / 2, height / 2, TextureFormat::kR11G11B10_FLOAT);
    ao_half_render_target_ = gfx_->CreateRenderTarget(width / 2, height / 2);
    ao_half_render_target_->AttachColor(AttachmentPoint::kColor0, ao_half_texture);
    ao_half_texture->SetName("ao half texture");

    auto ao_tmp_texture = RenderTexturePool::Get(width, height, TextureFormat::kR11G11B10_FLOAT);
    ao_spatial_render_target_ = gfx_->CreateRenderTarget(width, height);
    ao_spatial_render_target_->AttachColor(AttachmentPoint::kColor0, ao_tmp_texture);
    ao_tmp_texture->SetName("ao temp texture");

    gtao_upsampling_mat_ = std::make_shared<PostProcessMaterial>("GTAOUpsampling", TEXT("GTAOUpsampling"));
    gtao_upsampling_mat_->SetProperty("OcclusionTexture", ao_half_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    gtao_upsampling_mat_->SetProperty("_GtaoData", gtao_param_);

    gtao_filter_x_mat_ = std::make_shared<PostProcessMaterial>("GTAOFilter", TEXT("GTAOFilter"));
    gtao_filter_x_mat_->SetProperty("filter_param", gtao_filter_x_param_);
    gtao_filter_x_mat_->SetProperty("_GtaoData", gtao_param_);
    gtao_filter_x_mat_->SetProperty("OcclusionTexture", ao_full_render_target_->GetColorAttachment(AttachmentPoint::kColor0));

    gtao_filter_y_mat_ = std::make_shared<PostProcessMaterial>("GTAOFilter", TEXT("GTAOFilter"));
    gtao_filter_y_mat_->SetProperty("filter_param", gtao_filter_y_param_);
    gtao_filter_y_mat_->SetProperty("_GtaoData", gtao_param_);
    gtao_filter_y_mat_->SetProperty("OcclusionTexture", ao_spatial_render_target_->GetColorAttachment(AttachmentPoint::kColor0));

    gtao_mat_ = std::make_shared<PostProcessMaterial>("GTAO", TEXT("GTAO"));
    gtao_mat_->SetProperty("_GtaoData", gtao_param_);
    gtao_mat_->SetProperty("DepthBuffer", depth_buffer);
    gtao_mat_->SetProperty("NormalTexture", normal_buffer);

    gtao_upsampling_mat_->SetProperty("DepthBuffer", depth_buffer);
    gtao_filter_x_mat_->SetProperty("DepthBuffer", depth_buffer);
    gtao_filter_y_mat_->SetProperty("DepthBuffer", depth_buffer);
}

void GTAO::OnResize(uint32_t width, uint32_t height) {
    ao_full_render_target_->Resize(width, height);
    ao_half_render_target_->Resize(width / 2, height / 2);
    ao_spatial_render_target_->Resize(width, height);
}

void GTAO::SetupBuiltinProperty(Material* mat) {
    if (mat->HasParameter("_OcclusionTexture")) {
        mat->SetProperty("_OcclusionTexture", ao_full_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    }

    if (mat->HasParameter("_GtaoData")) {
        mat->SetProperty("_GtaoData", gtao_param_);
    }
}

void GTAO::Execute(Renderer* renderer, CommandBuffer* cmd_buffer) {
    //constexpr float Rots[6] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
    //constexpr float Offsets[4] = { 0.1f, 0.6f, 0.35f, 0.85f };
    //float TemporalAngle = Rots[frame_count_ % 6] * (math::k2PI / 360.0f);
    //float SinAngle, CosAngle;
    //math::FastSinCos(&SinAngle, &CosAngle, TemporalAngle);
    // GtaoParam Params = { CosAngle, SinAngle, Offsets[(frame_count_ / 6) % 4] * 0.25, Offsets[frame_count_ % 4] };

    auto camera = renderer->GetMainCamera();
    auto& param = gtao_param_.param();
    auto width = ao_full_render_target_->width();
    auto height = ao_full_render_target_->width();
    if (half_ao_res_) {
        width /= 2;
        height /= 2;
    }

    param.fov_scale = height / (math::Tan(camera->fov() * math::kDeg2Rad * 0.5f) * 2.0f) * 0.5f;
    param.render_param = Vec4f{ (float)width, (float)height, 1.0f / width, 1.0f / height };
    gtao_param_.Update();

    if (half_ao_res_) {
        Renderer::PostProcess(cmd_buffer, ao_half_render_target_, gtao_mat_.get());
        Renderer::PostProcess(cmd_buffer, ao_full_render_target_, gtao_upsampling_mat_.get());
    }
    else {
        Renderer::PostProcess(cmd_buffer, ao_full_render_target_, gtao_mat_.get());
    }

    Renderer::PostProcess(cmd_buffer, ao_spatial_render_target_, gtao_filter_x_mat_.get());
    Renderer::PostProcess(cmd_buffer, ao_full_render_target_, gtao_filter_y_mat_.get());
}

void GTAO::DrawOptionWindow() {
    if (ImGui::CollapsingHeader("AO", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& param = gtao_param_.param();

        ImGui::Text("Radius");
        ImGui::SameLine(80);
        ImGui::SliderFloat("##GTAO Radius", &param.radius, 1.0f, 10.0f);

        ImGui::Text("Fade Radius");
        ImGui::SameLine(80);
        ImGui::SliderFloat("##GTAO Fade Radius", &param.fade_to_radius, 1.0f, 2.0f);

        if (param.fade_to_radius >= param.radius) {
            param.fade_to_radius = param.radius - 0.01f;
        }

        ImGui::Text("Thickness");
        ImGui::SameLine(80);
        ImGui::SliderFloat("##GTAO Thickness", &param.thickness, 0.1f, 1.0f);

        ImGui::Text("Intensity");
        ImGui::SameLine(80);
        ImGui::SliderFloat("##GTAO Intensity", &param.intensity, 1.0f, 5.0f);

        ImGui::Text("Sharpness");
        ImGui::SameLine(80);
        ImGui::SliderFloat("##GTAO Sharpness", &param.sharpness, 0.0f, 2.0f);

        ImGui::Checkbox("Half Resolution", &half_ao_res_);

        if (ImGui::Checkbox("Debug AO", (bool*)&param.debug_ao)) {
            if (param.debug_ro && param.debug_ao) param.debug_ro = 0;
        }

        if (ImGui::Checkbox("Debug RO", (bool*)&param.debug_ro)) {
            if (param.debug_ro && param.debug_ao) param.debug_ao = 0;
        }
    }
}

}
}
