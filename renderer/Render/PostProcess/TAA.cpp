#include "TAA.h"
#include <assert.h>
#include <algorithm>
#include "Render/DeferredRenderer.h"
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

void TAA::Setup(DeferredRenderer* renderer)
{
    auto gfx = renderer->driver();
    for (int i = 0; i < kTAASampleCount; ++i) {
        halton_sequence_[i] = { LowDiscrepancySequence::Halton(i + 1, 2), LowDiscrepancySequence::Halton(i + 1, 3) };
        halton_sequence_[i] = halton_sequence_[i] * 2.0f - 1.0f;
    }

    taa_param_ = gfx->CreateConstantParameter<TAAParam, UsageType::kDefault>(0.95f, 0.85f, 6000.0f, 0.0f);

    auto& hdr_render_target_ = renderer->GetHdrRenderTarget();
    auto width = hdr_render_target_->width();
    auto height = hdr_render_target_->height();

    temp_hdr_texture_ = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT);
    prev_hdr_texture_ = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT);

    taa_mat_ = std::make_shared<PostProcessMaterial>("TAA", TEXT("TAA"));
    taa_mat_->SetProperty("taa_param", taa_param_);
    taa_mat_->SetProperty("_PostSourceTexture", temp_hdr_texture_);
    taa_mat_->SetProperty("_DepthBuffer", hdr_render_target_->GetDepthStencil());
    taa_mat_->SetProperty("PrevColorTexture", prev_hdr_texture_);

    auto velocity_tex = renderer->GetGBufferRenderTarget()->GetColorAttachment(AttachmentPoint::kColor4);
    taa_mat_->SetProperty("VelocityTexture", velocity_tex);
}

void TAA::OnResize(uint32_t width, uint32_t height) {
    temp_hdr_texture_->Resize(width, height);
    prev_hdr_texture_->Resize(width, height);
}

void TAA::UpdatePerFrameData(uint64_t frame_count, Matrix4x4& projection, float rcp_width, float rcp_height) {
        uint64_t idx = frame_count % kTAASampleCount;
        double jitter_x = halton_sequence_[idx].x * (double)rcp_width;
        double jitter_y = halton_sequence_[idx].y * (double)rcp_height;
        projection[0][2] += jitter_x;
        projection[1][2] += jitter_y;
}

void TAA::Execute(Renderer* renderer, CommandBuffer* cmd_buffer) {

    auto frame_count = renderer->frame_count();
    auto& hdr_render_target = renderer->GetHdrRenderTarget();
    if (frame_count > 0) {
        cmd_buffer->CopyResource(hdr_render_target->GetColorAttachment(AttachmentPoint::kColor0).get(), temp_hdr_texture_.get());
        Renderer::PostProcess(cmd_buffer, hdr_render_target, taa_mat_.get());
    }

    cmd_buffer->CopyResource(hdr_render_target->GetColorAttachment(AttachmentPoint::kColor0).get(), prev_hdr_texture_.get());
}

void TAA::DrawOptionWindow() {
        auto& param = taa_param_.param();
    bool dirty = false;

    ImGui::Text("Static Blending");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Static Blending", &param.static_blending, 0.9f, 0.99f)) {
        dirty = true;
    }

    ImGui::Text("Dynamic Blending");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Dynamic Blending", &param.dynamic_blending, 0.8f, 0.9f)) {
        dirty = true;
    }

    ImGui::Text("Motion Amplify");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Motion Amplify", &param.motion_amplify, 4000.0f, 6000.0f)) {
        dirty = true;
    }

    if (dirty) {
        taa_param_.Update();
    }
}

}
}
