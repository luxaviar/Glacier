#include "DeferredRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
#include "Render/Mesh/model.h"
#include "Camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "CascadedShadowManager.h"
#include "Core/GameObject.h"
#include "Render/LightManager.h"
#include "Render/Base/Texture.h"
#include "Render/Base/SamplerState.h"
#include "Inspect/Profiler.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/RenderTexturePool.h"
#include "Render/Base/Program.h"
#include "Common/Log.h"
#include "Render/Base/CommandBuffer.h"
#include "PostProcess/Gtao.h"

namespace glacier {
namespace render {

DeferredRenderer::DeferredRenderer(GfxDriver* gfx, AntiAliasingType aa) :
    Renderer(gfx, aa),
    option_aa_(aa)
{
    assert(aa != AntiAliasingType::kMSAA);
}

void DeferredRenderer::Setup() {
    gpass_program_ = gfx_->CreateProgram("GPass", TEXT("GBufferPass"), TEXT("GBufferPass"));
    gpass_program_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    gpass_program_->SetProperty("linear_sampler", ss);
    gpass_program_->AddPass("GPass");

    Renderer::Setup();

    InitRenderGraph(gfx_);

    lighting_mat_ = std::make_shared<PostProcessMaterial>("DeferredLighting", TEXT("DeferredLighting"));

    ss.warpU = ss.warpV = WarpMode::kClamp;
    lighting_mat_->SetProperty("linear_sampler", ss);
    lighting_mat_->AddPass("DeferredLighting");

    lighting_mat_->SetProperty("AlbedoTexture", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    lighting_mat_->SetProperty("NormalTexture", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor1));
    lighting_mat_->SetProperty("AoMetalroughnessTexture", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor2));
    lighting_mat_->SetProperty("EmissiveTexture", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor3));
    lighting_mat_->SetProperty("_DepthBuffer", hdr_render_target_->GetDepthStencil());

    fxaa_.Setup(this);
    taa_.Setup(this);
    gtao_.Setup(this, hdr_render_target_->GetDepthStencil(), gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor1));
}

void DeferredRenderer::JitterProjection(Matrix4x4& projection) {
    auto width = hdr_render_target_->width();
    auto height = hdr_render_target_->height();
    taa_.UpdatePerFrameData(frame_count_, projection, 1.0f / width, 1.0f / height);
}

std::shared_ptr<Material> DeferredRenderer::CreateLightingMaterial(const char* name) {
    return std::make_shared<Material>(name, gpass_program_);
}

void DeferredRenderer::DoTAA(CommandBuffer* cmd_buffer) {
    if (anti_aliasing_ != AntiAliasingType::kTAA) return;
    taa_.Execute(this, cmd_buffer);
}

void DeferredRenderer::DoFXAA(CommandBuffer* cmd_buffer) {
    if (anti_aliasing_ != AntiAliasingType::kFXAA) {
        Renderer::DoFXAA(cmd_buffer);
        return;
    }

    fxaa_.Execute(this, cmd_buffer);
}

void DeferredRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();

    auto albedo_texture = RenderTexturePool::Get(width, height);
    albedo_texture->SetName("GBuffer Albedo");

    gbuffer_render_target_ = gfx_->CreateRenderTarget(width, height);
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor0, albedo_texture);

    auto normal_texture = RenderTexturePool::Get(width, height, TextureFormat::kR16G16_FLOAT);
    normal_texture->SetName("GBuffer Normal");
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor1, normal_texture);

    auto ao_metal_roughness_texture = RenderTexturePool::Get(width, height);
    ao_metal_roughness_texture->SetName("GBuffer ao_metal_roughness");
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor2, ao_metal_roughness_texture);

    auto emissive_texture = RenderTexturePool::Get(width, height);
    emissive_texture->SetName("GBuffer Emissive");
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor3, emissive_texture);

    auto velocity_texture = RenderTexturePool::Get(width, height, TextureFormat::kR16G16_FLOAT);
    velocity_texture->SetName("GBuffer Velocity");
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor4, velocity_texture);

    gbuffer_render_target_->AttachDepthStencil(hdr_render_target_->GetDepthStencil());
}

bool DeferredRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    gbuffer_render_target_->Resize(width, height);
    taa_.OnResize(width, height);

    return true;
}

void DeferredRenderer::SetupBuiltinProperty(Material* mat) {
    Renderer::SetupBuiltinProperty(mat);
}

void DeferredRenderer::PreRender(CommandBuffer* cmd_buffer) {
    if (option_aa_ != anti_aliasing_) {
        anti_aliasing_ = option_aa_;
    }

    gbuffer_render_target_->Clear(cmd_buffer);

    Renderer::PreRender(cmd_buffer);
}

void DeferredRenderer::AddGPass() {
    render_graph_.AddPass("GPass",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("GPass Pass");

            RenderTargetGuard rt_gurad(cmd_buffer, gbuffer_render_target_.get());
            pass.Render(cmd_buffer, visibles_);
        });
}

void DeferredRenderer::AddLightingPass() {
    render_graph_.AddPass("DeferredLighting",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("DeferredLighting Pass");

            LightManager::Instance()->Update(cmd_buffer);
            csm_manager_->Update();

            PostProcess(cmd_buffer, hdr_render_target_, lighting_mat_.get(), true);
        });
}

void DeferredRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddGPass();
    AddGtaoPass();
    AddLightingPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

void DeferredRenderer::DrawOptionWindow() {
    if (ImGui::CollapsingHeader("Anti-Aliasing", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto aa_combo_label = kAADesc[(int)anti_aliasing_];

        ImGui::Text("Anti-Aliasing");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##Anti-Aliasing", aa_combo_label, ImGuiComboFlags_PopupAlignLeft)) {
            for (int n = 0; n < kAADesc.size(); n++) {
                if (n != (int)AntiAliasingType::kMSAA) {
                    const bool is_selected = ((int)option_aa_ == n);

                    if (ImGui::Selectable(kAADesc[n], is_selected))
                        option_aa_ = (AntiAliasingType)n;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (anti_aliasing_ == AntiAliasingType::kFXAA) {
            fxaa_.DrawOptionWindow();
        }

        if (anti_aliasing_ == AntiAliasingType::kTAA) {
            taa_.DrawOptionWindow();
        }
    }
}

}
}
