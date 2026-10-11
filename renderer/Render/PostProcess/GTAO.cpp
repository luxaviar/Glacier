#include "GTAO.h"
#include <assert.h>
#include <algorithm>
#include "Render/Renderer.h"
#include <imgui.h>
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

    // the half resolution trace target and the stage between the two filter
    // directions are transients the graph hands the chain every frame (T2.4),
    // so they are not attached here; BindHalfTarget / BindSpatialTarget attach
    // what the frame resolved before the node that uses it runs
    ao_half_render_target_ = gfx_->CreateRenderTarget(width / 2, height / 2);
    ao_spatial_render_target_ = gfx_->CreateRenderTarget(width, height);

    gtao_upsampling_mat_ = std::make_shared<PostProcessMaterial>("GTAOUpsampling", TEXT("GTAOUpsampling"));
    gtao_upsampling_mat_->SetProperty("_GtaoData", gtao_param_);

    gtao_filter_x_mat_ = std::make_shared<PostProcessMaterial>("GTAOFilter", TEXT("GTAOFilter"));
    gtao_filter_x_mat_->SetProperty("filter_param", gtao_filter_x_param_);
    gtao_filter_x_mat_->SetProperty("_GtaoData", gtao_param_);
    gtao_filter_x_mat_->SetProperty("OcclusionTexture", ao_full_render_target_->GetColorAttachment(AttachmentPoint::kColor0));

    // the source of the second direction is the transient of the frame, so the
    // node that binds it points the material at it (BindSpatialTarget); the
    // source of the first direction is the full target above and stays
    gtao_filter_y_mat_ = std::make_shared<PostProcessMaterial>("GTAOFilter", TEXT("GTAOFilter"));
    gtao_filter_y_mat_->SetProperty("filter_param", gtao_filter_y_param_);
    gtao_filter_y_mat_->SetProperty("_GtaoData", gtao_param_);

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

    // the half and the spatial target carry the transient textures of the
    // frame that just ran (T2.4); a pooled texture is not resized here - it is
    // detached, the target is sized, and the node that runs next attaches the
    // texture of its own frame
    ao_half_render_target_->DetachColor(AttachmentPoint::kColor0);
    ao_half_render_target_->Resize(width / 2, height / 2);

    ao_spatial_render_target_->DetachColor(AttachmentPoint::kColor0);
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

void GTAO::BindTarget(RenderTarget* target, const std::shared_ptr<Texture>& texture) {
    if (!texture) {
        return;
    }

    if (target->GetColorAttachment(AttachmentPoint::kColor0).get() == texture.get()) {
        return;
    }

    // the texture is the frame's and comes at the size the frame resolved; the
    // attachment steps aside first so that sizing the target does not reach
    // into the texture the graph owns
    target->DetachColor(AttachmentPoint::kColor0);
    target->Resize(texture->width(), texture->height());
    target->AttachColor(AttachmentPoint::kColor0, texture);
}

void GTAO::BindHalfTarget(const std::shared_ptr<Texture>& texture) {
    BindTarget(ao_half_render_target_.get(), texture);
    gtao_upsampling_mat_->SetProperty("OcclusionTexture", texture);
}

void GTAO::BindSpatialTarget(const std::shared_ptr<Texture>& texture) {
    BindTarget(ao_spatial_render_target_.get(), texture);
    gtao_filter_y_mat_->SetProperty("OcclusionTexture", texture);
}

void GTAO::UpdateParam(Renderer* renderer) {
    auto camera = renderer->GetMainCamera();
    auto& param = gtao_param_.param();
    auto width = ao_full_render_target_->width();
    // the shader turns the search direction of a pixel into uv with 1 / width
    // and 1 / height, and the upsample reads the resolution of the trace back
    // out of render_param.xy, so the height has to be the real one: a square
    // pair made the vertical march of the search short by the aspect ratio and
    // the upsample interpolate in y with the wrong fraction
    auto height = ao_full_render_target_->height();
    if (half_ao_res_) {
        width /= 2;
        height /= 2;
    }

    param.fov_scale = height / (math::Tan(camera->fov() * math::kDeg2Rad * 0.5f) * 2.0f) * 0.5f;
    param.render_param = Vec4f{ (float)width, (float)height, 1.0f / width, 1.0f / height };
    gtao_param_.Update();
}

void GTAO::ExecuteTrace(Renderer* renderer, CommandBuffer* cmd_buffer) {
    UpdateParam(renderer);

    // the trace renders into the half resolution target while it runs at half
    // resolution, and into the full target the lighting reads otherwise; the
    // four stages are timed apart by the graph, which brackets every node
    if (half_ao_res_) {
        Renderer::PostProcess(cmd_buffer, ao_half_render_target_, gtao_mat_.get());
    }
    else {
        Renderer::PostProcess(cmd_buffer, ao_full_render_target_, gtao_mat_.get());
    }
}

void GTAO::ExecuteUpsample(CommandBuffer* cmd_buffer) {
    Renderer::PostProcess(cmd_buffer, ao_full_render_target_, gtao_upsampling_mat_.get());
}

void GTAO::ExecuteFilterX(CommandBuffer* cmd_buffer) {
    Renderer::PostProcess(cmd_buffer, ao_spatial_render_target_, gtao_filter_x_mat_.get());
}

void GTAO::ExecuteFilterY(CommandBuffer* cmd_buffer) {
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
