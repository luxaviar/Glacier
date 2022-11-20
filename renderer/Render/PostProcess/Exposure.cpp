#include "Exposure.h"
#include <assert.h>
#include <algorithm>
#include "Render/Renderer.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Base/RenderTarget.h"
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

void Exposure::Setup(Renderer* renderer)
{
    hdr_render_target_ = renderer->GetHdrRenderTarget();
    auto width = hdr_render_target_->width();
    auto height = hdr_render_target_->height();

    lumin_texture_ = RenderTexturePool::Get(width / 2, height / 2, TextureFormat::kR8_UINT,
        (CreateFlags)((uint32_t)CreateFlags::kUav | (uint32_t)CreateFlags::kShaderResource));
    lumin_texture_->SetName("luminance texture");

    auto gfx_ = renderer->driver();
    exposure_buf_ = gfx_->CreateStructuredBuffer(4, 4, true);
    histogram_buf_ = gfx_->CreateByteAddressBuffer(256, true);
    exposure_params_ = gfx_->CreateConstantParameter<ExposureAdaptParam, UsageType::kDefault>();
    
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kCopy);
    alignas(16) float initExposure[] = { 1.0, 1.0 };
    exposure_buf_->Upload(cmd_buffer, initExposure);

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kCopy);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
    
    lumin_mat_ = std::make_shared<Material>("ExtractLumaCS", TEXT("ExtractLumaCS"));
    lumin_mat_->SetProperty("SourceTexture", hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    lumin_mat_->SetProperty("LumaResult", lumin_texture_);
    lumin_mat_->SetProperty("Exposure", exposure_buf_);
    lumin_mat_->SetProperty("exposure_params", exposure_params_);

    histogram_mat_ = std::make_shared<Material>("GenerateHistogramCS", TEXT("GenerateHistogramCS"));
    histogram_mat_->SetProperty("LumaBuf", lumin_texture_);
    histogram_mat_->SetProperty("Histogram", histogram_buf_);
    histogram_mat_->SetProperty("exposure_params", exposure_params_);

    exposure_mat_ = std::make_shared<Material>("AdaptExposureCS", TEXT("AdaptExposureCS"));
    exposure_mat_->SetProperty("Histogram", histogram_buf_);
    exposure_mat_->SetProperty("Exposure", exposure_buf_);
    exposure_mat_->SetProperty("exposure_params", exposure_params_);

    auto lum_wdith = lumin_texture_->width();
    auto lum_height = lumin_texture_->height();
    exposure_params_.param().pixel_count = lum_wdith * lum_height;
    exposure_params_.param().lum_size = { (float)lum_wdith, (float)lum_height, 1.0f / lum_wdith, 1.0f / lum_height };
    exposure_params_.Update();
}

void Exposure::OnResize(uint32_t width, uint32_t height) {
    lumin_texture_->Resize(width / 2, height / 2);

    auto lum_wdith = lumin_texture_->width();
    auto lum_height = lumin_texture_->height();
    exposure_params_.param().pixel_count = lum_wdith * lum_height;
    exposure_params_.param().lum_size = { (float)lum_wdith, (float)lum_height, 1.0f / lum_wdith, 1.0f / lum_height};
    exposure_params_.Update();
}

void Exposure::GenerateLuminance(CommandBuffer* cmd_buffer) {
    //calc luminance
    auto dst_width = lumin_texture_->width();
    auto dst_height = lumin_texture_->height();

    cmd_buffer->BindMaterial(lumin_mat_.get());
    cmd_buffer->Dispatch(math::DivideByMultiple(dst_width, 8), math::DivideByMultiple(dst_height, 8), 1);
    cmd_buffer->UavResource(lumin_texture_.get());
}

void Exposure::UpdateExposure(CommandBuffer* cmd_buffer) {
    auto dst_width = lumin_texture_->width();
    auto dst_height = lumin_texture_->height();

    //calculate histogram
    cmd_buffer->UavResource(histogram_buf_.get());
    cmd_buffer->ClearUAV(histogram_buf_.get());
    cmd_buffer->BindMaterial(histogram_mat_.get());
    cmd_buffer->Dispatch(math::DivideByMultiple(dst_width, 16), 1, 1);

    //calculate adapt exposure
    cmd_buffer->BindMaterial(exposure_mat_.get());
    cmd_buffer->Dispatch(1, 1, 1);
}

void Exposure::DrawOptionWindow() {
    if (ImGui::CollapsingHeader("Auto Exposure", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool updated = false;
        auto& param = exposure_params_.param();
        ImGui::Text("Exposure Compensation");
        ImGui::SameLine(160);
        if (ImGui::DragFloat("##Exposure Compensation", &param.exposure_compensation, 0.25f, -15, 15)) {
            updated = true;
        }

        ImGui::Text("Exposure Range");
        ImGui::SameLine(160);
        if (ImGui::DragFloatRange2("##Exposure Range", &param.min_exposure,  &param.max_exposure,
                0.25f, -5, 15))
        {
            param.rcp_exposure_range = 1.0f / std::max((param.max_exposure - param.min_exposure), 0.00001f);
            updated = true;
        }

        ImGui::Text("Speed Dark To Light");
        ImGui::SameLine(160);
        if (ImGui::DragFloat("##Exposure SpeedToLight", &param.speed_to_light, 0.05f, 0.001, 100)) {
            updated = true;
        }

        ImGui::Text("Speed Light To Dark");
        ImGui::SameLine(160);
        if (ImGui::DragFloat("##Exposure SpeedToDark", &param.speed_to_dark, 0.05f, 0.001, 100)) {
            updated = true;
        }

        if (updated) {
            exposure_params_.Update();
        }
    }
}

}
}
