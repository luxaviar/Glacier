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
#include "Common/Log.h"

namespace glacier {
namespace render {

DeferredRenderer::DeferredRenderer(GfxDriver* gfx, AntiAliasingType aa) :
    Renderer(gfx, aa),
    option_aa_(aa)
{
    assert(aa != AntiAliasingType::kMSAA);
    fxaa_param_ = gfx_->CreateConstantParameter<FXAAParam>(UsageType::kDefault);
    fxaa_param_ = { 0.0833f, 0.166f, 0.75f, 0.0f };

    taa_param_ = gfx_->CreateConstantParameter<TAAParam>(UsageType::kDefault);
    taa_param_ = { 0.95f, 0.85f, 6000.0f, 0.0f };
}

void DeferredRenderer::Setup() {
    if (init_) return;

    Renderer::Setup();

    InitRenderGraph(gfx_);

    auto gpass_program = gfx_->CreateProgram("GPass", TEXT("GBufferPass"), TEXT("GBufferPass"));
    gpass_template_ = std::make_shared<MaterialTemplate>("GPass", gpass_program);
    gpass_template_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    gpass_template_->SetProperty("linear_sampler", ss);
    gpass_template_->SetProperty("_PerFrameData", per_frame_param_);
    gpass_template_->AddPass("GPass");

    lighting_mat_ = std::make_shared<PostProcessMaterial>("DeferredLighting", TEXT("DeferredLighting"));

    ss.warpU = ss.warpV = WarpMode::kClamp;
    lighting_mat_->SetProperty("linear_sampler", ss);
    lighting_mat_->SetProperty("_PerFrameData", per_frame_param_);

    lighting_mat_->AddPass("DeferredLighting");

    LightManager::Instance()->SetupMaterial(lighting_mat_.get());
    csm_manager_->SetupMaterial(lighting_mat_.get());

    lighting_mat_->SetProperty("albedo_tex", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    lighting_mat_->SetProperty("normal_tex", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor1));
    lighting_mat_->SetProperty("ao_metalroughness_tex", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor2));
    lighting_mat_->SetProperty("emissive_tex", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor3));
    lighting_mat_->SetProperty("_DepthBuffer", hdr_render_target_->GetDepthStencil());

    fxaa_mat_ = std::make_shared<PostProcessMaterial>("FXAA", TEXT("FXAA"));
    fxaa_mat_->SetProperty("fxaa_param", fxaa_param_);
    fxaa_mat_->SetProperty("_PostSourceTexture", ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    fxaa_mat_->SetProperty("_PerFrameData", per_frame_param_);

    taa_mat_ = std::make_shared<PostProcessMaterial>("TAA", TEXT("TAA"));
    taa_mat_->SetProperty("taa_param", taa_param_);
    taa_mat_->SetProperty("_PostSourceTexture", temp_hdr_texture_);
    taa_mat_->SetProperty("_PerFrameData", per_frame_param_);
    taa_mat_->SetProperty("_DepthBuffer", hdr_render_target_->GetDepthStencil());
    taa_mat_->SetProperty("PrevColorTexture", prev_hdr_texture_);
    taa_mat_->SetProperty("VelocityTexture", gbuffer_render_target_->GetColorAttachment(AttachmentPoint::kColor4));

    InitHelmetPbr(gfx_);
    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

void DeferredRenderer::DoTAA() {
    if (anti_aliasing_ != AntiAliasingType::kTAA) return;

    if (frame_count_ > 0) {
        gfx_->CopyResource(hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0), temp_hdr_texture_);
        PostProcess(hdr_render_target_, taa_mat_.get());
    }

    gfx_->CopyResource(hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0), prev_hdr_texture_);
}

void DeferredRenderer::DoFXAA() {
    if (anti_aliasing_ != AntiAliasingType::kFXAA) {
        Renderer::DoFXAA();
        return;
    }

    PostProcess(present_render_target_, fxaa_mat_.get());
}

void DeferredRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();

    temp_hdr_texture_ = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT);
    prev_hdr_texture_ = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT);

    auto albedo_texture = RenderTexturePool::Get(width, height);
    albedo_texture->SetName(TEXT("GBuffer Albedo"));

    gbuffer_render_target_ = gfx_->CreateRenderTarget(width, height);
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor0, albedo_texture);

    auto normal_texture = RenderTexturePool::Get(width, height, TextureFormat::kR16G16_FLOAT);
    normal_texture->SetName(TEXT("GBuffer Normal"));
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor1, normal_texture);

    auto ao_metal_roughness_texture = RenderTexturePool::Get(width, height);
    ao_metal_roughness_texture->SetName(TEXT("GBuffer ao_metal_roughness"));
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor2, ao_metal_roughness_texture);

    auto emissive_texture = RenderTexturePool::Get(width, height);
    emissive_texture->SetName(TEXT("GBuffer Emissive"));
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor3, emissive_texture);

    auto velocity_texture = RenderTexturePool::Get(width, height, TextureFormat::kR16G16_FLOAT);
    velocity_texture->SetName(TEXT("GBuffer Velocity"));
    gbuffer_render_target_->AttachColor(AttachmentPoint::kColor4, velocity_texture);

    gbuffer_render_target_->AttachDepthStencil(hdr_render_target_->GetDepthStencil());
}

bool DeferredRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    gbuffer_render_target_->Resize(width, height);

    temp_hdr_texture_->Resize(width, height);
    prev_hdr_texture_->Resize(width, height);

    return true;
}

void DeferredRenderer::PreRender() {
    if (option_aa_ != anti_aliasing_) {
        anti_aliasing_ = option_aa_;
    }

    gbuffer_render_target_->Clear();

    Renderer::PreRender();
}

void DeferredRenderer::InitHelmetPbr(GfxDriver* gfx) {
    auto albedo_desc = Texture::Description()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_desc);

    auto normal_desc = Texture::Description()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_desc);

    auto metal_desc = Texture::Description()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_metalroughness.png"))
        .EnableMips();
    auto metal_roughness_tex = gfx->CreateTexture(metal_desc);

    auto ao_desc = Texture::Description()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_occlusion.png"))
        .EnableMips()
        .EnableSRGB();
    auto ao_tex = gfx->CreateTexture(ao_desc);

    auto em_desc = Texture::Description()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_emission.png"))
        .EnableMips()
        .EnableSRGB();
    auto emissive_tex = gfx->CreateTexture(em_desc);

    auto helmet_mat = std::make_unique<Material>("pbr_helmet", gpass_template_);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;

    helmet_mat->SetProperty("linear_sampler", ss);
    helmet_mat->SetProperty("albedo_tex", albedo_tex);
    helmet_mat->SetProperty("normal_tex", normal_tex);
    helmet_mat->SetProperty("metalroughness_tex", metal_roughness_tex);
    helmet_mat->SetProperty("ao_tex", ao_tex);
    helmet_mat->SetProperty("emissive_tex", emissive_tex);
    helmet_mat->SetProperty("_PerObjectData", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    helmet_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(helmet_mat));
}

void DeferredRenderer::InitDefaultPbr(GfxDriver* gfx) {
    auto default_mat = std::make_unique<Material>("pbr_default", gpass_template_);

    default_mat->SetProperty("albedo_tex", nullptr, Color::kWhite);
    default_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    default_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    default_mat->SetProperty("emissive_tex", nullptr, Color::kBlack);
    default_mat->SetProperty("_PerObjectData", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 0;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    default_mat->SetProperty("object_material", mat_cbuf);

    auto red_mat = std::make_unique<Material>(*default_mat);
    red_mat->name("red_pbr_default");
    red_mat->SetProperty("albedo_tex", nullptr, Color::kRed);

    auto green_mat = std::make_unique<Material>(*default_mat);
    green_mat->name("green_pbr_default");
    green_mat->SetProperty("albedo_tex", nullptr, Color::kGreen);

    auto orange_mat = std::make_unique<Material>(*default_mat);
    orange_mat->name("orange_pbr_default");
    orange_mat->SetProperty("albedo_tex", nullptr, Color::kOrange);

    auto indigo_mat = std::make_unique<Material>(*default_mat);
    indigo_mat->name("indigo_pbr_default");
    indigo_mat->SetProperty("albedo_tex", nullptr, Color::kIndigo);

    MaterialManager::Instance()->Add(std::move(default_mat));
    MaterialManager::Instance()->Add(std::move(red_mat));
    MaterialManager::Instance()->Add(std::move(green_mat));
    MaterialManager::Instance()->Add(std::move(orange_mat));
    MaterialManager::Instance()->Add(std::move(indigo_mat));
}

void DeferredRenderer::InitFloorPbr(GfxDriver* gfx) {
    auto albedo_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_desc);

    auto normal_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_desc);

    auto floor_mat = std::make_unique<Material>("pbr_floor", gpass_template_);
    floor_mat->SetTexTilingOffset({5.0f, 5.0f, 0.0f, 0.0f});

    floor_mat->SetProperty("albedo_tex", albedo_tex);
    floor_mat->SetProperty("normal_tex", normal_tex);
    floor_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    floor_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    floor_mat->SetProperty("emissive_tex", nullptr, Color::kBlack);
    floor_mat->SetProperty("_PerObjectData", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    floor_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(floor_mat));
}

void DeferredRenderer::AddGPass() {
    render_graph_.AddPass("GPass",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto gfx = renderer->driver();
            PerfSample("GPass Pass");

            RenderTargetBindingGuard rt_gurad(gfx, gbuffer_render_target_.get());

            pass.Render(renderer, visibles_);
        });
}

void DeferredRenderer::AddLightingPass() {
    render_graph_.AddPass("DeferredLighting",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto gfx = renderer->driver();
            PerfSample("DeferredLighting Pass");

            LightManager::Instance()->Update();
            csm_manager_->Update();

            PostProcess(hdr_render_target_, lighting_mat_.get());

            BindLightingTarget();
        });
}

void DeferredRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddGPass();
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
            auto& param = fxaa_param_.param();
            bool dirty = false;

            ImGui::Text("Contrast Threshold");
            ImGui::SameLine(140);
            if (ImGui::SliderFloat("##Contrast Threshold", &param.contrast_threshold, 0.0312f, 0.0833f)) {
                dirty = true;
            }

            ImGui::Text("Relative Threshold");
            ImGui::SameLine(140);
            if (ImGui::SliderFloat("##Relative Threshold", &param.relative_threshold, 0.063f, 0.333f)) {
                dirty = true;
            }

            ImGui::Text("Subpixel Blending");
            ImGui::SameLine(140);
            if (ImGui::SliderFloat("##Subpixel Blending", &param.subpixel_blending, 0.0f, 1.0f)) {
                dirty = true;
            }

            if (dirty) {
                fxaa_param_.Update();
            }
        }


        if (anti_aliasing_ == AntiAliasingType::kTAA) {
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

}
}
