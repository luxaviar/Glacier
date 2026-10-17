#include "DeferredRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include <imgui.h>
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
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
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(DeferredRenderer, DeferredRenderer)
LUX_CTOR(DeferredRenderer, GfxDriver*, AntiAliasingType)
LUX_IMPL_END

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

    //the occlusion texture the lighting pass reads is imported by
    //InitRenderGraph, so it has to exist first
    gtao_.Setup(this, hdr_render_target_->GetDepthStencil(), gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor1));

    //the tail of the deferred frame has to exist before the graph is
    //built, since the passes of InitRenderGraph declare what it holds
    fxaa_.Setup(this);
    taa_.Setup(this);

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
    //the node runs only while the option is fxaa; the copy of every other
    //option is the node after this one
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

    //the TAA node runs only while the option is on; off, the frame is
    //the one the scene passes left, and the node is not in the frame at all
    if (taa_pass_ != nullptr) {
        taa_pass_->active(anti_aliasing_ == AntiAliasingType::kTAA);
    }

    gbuffer_render_target_->Clear(cmd_buffer);

    Renderer::PreRender(cmd_buffer);
}

void DeferredRenderer::AddGPass(GraphResourceHandle albedo, GraphResourceHandle normal,
    GraphResourceHandle ao_metal_roughness, GraphResourceHandle emissive, GraphResourceHandle velocity) {
    render_graph_.AddPass("GPass",
        [=](PassBuilder& b) {
            b.Write(albedo, ResourceAccessBit::kRenderTarget);
            b.Write(normal, ResourceAccessBit::kRenderTarget);
            b.Write(ao_metal_roughness, ResourceAccessBit::kRenderTarget);
            b.Write(emissive, ResourceAccessBit::kRenderTarget);
            b.Write(velocity, ResourceAccessBit::kRenderTarget);
            b.Write(hdr_depth_handle(), ResourceAccessBit::kDepthWrite);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
            PerfSample("GPass Pass");

            RenderTargetGuard rt_gurad(cmd_buffer, gbuffer_render_target_.get());
            pass.Render(cmd_buffer, visibles_);
        });
}

void DeferredRenderer::AddLightingPass(GraphResourceHandle albedo, GraphResourceHandle normal,
    GraphResourceHandle ao_metal_roughness, GraphResourceHandle emissive,
    GraphResourceHandle occlusion, GraphResourceHandle shadow_map) {
    render_graph_.AddPass("DeferredLighting",
        [=](PassBuilder& b) {
            b.Read(albedo, ResourceAccessBit::kPixelShaderRead);
            b.Read(normal, ResourceAccessBit::kPixelShaderRead);
            b.Read(ao_metal_roughness, ResourceAccessBit::kPixelShaderRead);
            b.Read(emissive, ResourceAccessBit::kPixelShaderRead);
            b.Read(hdr_depth_handle(), ResourceAccessBit::kPixelShaderRead);
            b.Read(shadow_map, ResourceAccessBit::kPixelShaderRead);
            b.Read(occlusion, ResourceAccessBit::kPixelShaderRead);
            b.Write(hdr_color_handle(), ResourceAccessBit::kRenderTarget);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
            PerfSample("DeferredLighting Pass");

            LightManager::Instance()->Update(cmd_buffer);
            csm_manager_->Update();

            PostProcess(cmd_buffer, hdr_render_target_, lighting_mat_.get(), true);
        });
}

void DeferredRenderer::InitRenderGraph(GfxDriver* gfx) {
    //what the passes of the deferred stack declare they touch
    auto gbuffer_albedo = render_graph_.Import("gbuffer albedo",
        gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor0).get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto gbuffer_normal = render_graph_.Import("gbuffer normal",
        gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor1).get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto gbuffer_ao_metal_roughness = render_graph_.Import("gbuffer ao metal roughness",
        gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor2).get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto gbuffer_emissive = render_graph_.Import("gbuffer emissive",
        gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor3).get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto gbuffer_velocity = render_graph_.Import("gbuffer velocity",
        gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor4).get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    gbuffer_velocity_ = gbuffer_velocity;
    auto csm_shadow = render_graph_.Import("csm shadow map", csm_manager_->shadow_map().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto ao_texture = render_graph_.Import("ao texture", gtao_.occlusion_texture().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);

    AddShadowPass(csm_shadow);
    AddGPass(gbuffer_albedo, gbuffer_normal, gbuffer_ao_metal_roughness, gbuffer_emissive, gbuffer_velocity);
    AddGtaoPass(hdr_depth_handle(), gbuffer_normal, ao_texture);
    AddLightingPass(gbuffer_albedo, gbuffer_normal, gbuffer_ao_metal_roughness, gbuffer_emissive, ao_texture, csm_shadow);

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    AddTaaPass();
    AddPostProcessPasses();

    render_graph_.Compile();
}

void DeferredRenderer::AddTaaPass() {
    // The two halves of the TAA history are resident (imported), and so is the
    // velocity the pass reprojects with; the node ping-pongs the frame through
    // them: the frame is copied into the scratch texture, resolved against the
    // history, and handed on as the history of the next frame. Both
    // copies and the draw run in one node, which is why the frame and the
    // pair are declared in both of the states the body uses them in.
    auto temp = render_graph_.Import("taa temp texture", taa_.temp_texture().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto prev = render_graph_.Import("taa prev texture", taa_.prev_texture().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);

    auto& pass = render_graph_.AddPass("TAA",
        [=](PassBuilder& b) {
            b.Read(hdr_color_handle(), ResourceAccessBit::kCopySource);
            b.Write(hdr_color_handle(), ResourceAccessBit::kRenderTarget);
            b.Read(hdr_depth_handle(), ResourceAccessBit::kPixelShaderRead);
            b.Write(hdr_depth_handle(), ResourceAccessBit::kDepthWrite);
            b.Read(gbuffer_velocity_, ResourceAccessBit::kPixelShaderRead);
            b.Read(temp, ResourceAccessBit::kPixelShaderRead);
            b.Write(temp, ResourceAccessBit::kCopyDest);
            b.Read(prev, ResourceAccessBit::kPixelShaderRead);
            b.Write(prev, ResourceAccessBit::kCopyDest);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            DoTAA(ctx.cmd_buffer());
        });

    taa_pass_ = &pass;
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
