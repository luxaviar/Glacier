#include "DeferredRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui_impl_win32.h"
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

namespace glacier {
namespace render {

DeferredRenderer::DeferredRenderer(GfxDriver* gfx) :
    Renderer(gfx)
{
    frame_data_ = gfx->CreateConstantBuffer<FrameData>(UsageType::kDynamic);
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
    gpass_template_->AddPass("GPass");

    lighting_mat_ = std::make_shared<PostProcessMaterial>("DeferredLighting", TEXT("DeferredLighting"));

    ss.warpU = ss.warpV = WarpMode::kClamp;
    lighting_mat_->SetProperty("linear_sampler", ss);
    lighting_mat_->SetProperty("frame_data", frame_data_);
    //ss.filter = FilterMode::kPoint;
    //lighting_mat_->SetProperty("point_sampler", ss);

    lighting_mat_->AddPass("DeferredLighting");

    LightManager::Instance()->SetupMaterial(lighting_mat_.get());
    csm_manager_->SetupMaterial(lighting_mat_.get());

    lighting_mat_->SetProperty("albedo_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor0));
    lighting_mat_->SetProperty("normal_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor1));
    lighting_mat_->SetProperty("ao_metalroughness_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor2));
    lighting_mat_->SetProperty("emissive_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor3));
    lighting_mat_->SetProperty("position_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor4));
    lighting_mat_->SetProperty("view_position_tex", gbuffer_target_->GetColorAttachment(AttachmentPoint::kColor5));
    lighting_mat_->SetProperty("DepthBuffer_", render_target_->GetDepthStencil());

    InitHelmetPbr(gfx_);
    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

void DeferredRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();

    lighting_target_ = gfx_->CreateRenderTarget(width, height);
    lighting_target_->AttachColor(AttachmentPoint::kColor0, render_target_->GetColorAttachment(AttachmentPoint::kColor0));

    auto albedo_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
        .SetCreateFlag(CreateFlags::kRenderTarget);

    auto albedo_texture = gfx_->CreateTexture(albedo_desc);
    albedo_texture->SetName(TEXT("GBuffer Albedo"));

    gbuffer_target_ = gfx_->CreateRenderTarget(width, height);
    gbuffer_target_->AttachColor(AttachmentPoint::kColor0, albedo_texture);

    auto normal_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR16G16_FLOAT)
        .SetCreateFlag(CreateFlags::kRenderTarget);

    auto normal_texture = gfx_->CreateTexture(normal_desc);
    normal_texture->SetName(TEXT("GBuffer Normal"));

    gbuffer_target_->AttachColor(AttachmentPoint::kColor1, normal_texture);

    auto ao_metal_roughness_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
        .SetCreateFlag(CreateFlags::kRenderTarget);

    auto ao_metal_roughness_texture = gfx_->CreateTexture(ao_metal_roughness_desc);
    ao_metal_roughness_texture->SetName(TEXT("GBuffer ao_metal_roughness"));

    gbuffer_target_->AttachColor(AttachmentPoint::kColor2, ao_metal_roughness_texture);

    auto emissive_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
        .SetCreateFlag(CreateFlags::kRenderTarget);

    auto emissive_texture = gfx_->CreateTexture(emissive_desc);
    emissive_texture->SetName(TEXT("GBuffer Emissive"));
    gbuffer_target_->AttachColor(AttachmentPoint::kColor3, emissive_texture);
    gbuffer_target_->AttachDepthStencil(render_target_->GetDepthStencil());
}

bool DeferredRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    gbuffer_target_->Resize(width, height);
    lighting_target_->Resize(width, height);

    return true;
}

void DeferredRenderer::PreRender() {
    gbuffer_target_->Clear();

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
    helmet_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

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
    default_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

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

    // auto metal_desc = Texture::Description()
    //     .SetFile(TEXT("assets\\textures\\floor_metallic.png"))
    //     .EnableMips();
    // auto metal_roughness_tex = gfx->CreateTexture(metal_desc);

    // auto ao_desc = Texture::Description()
    //     .SetFile(TEXT("assets\\textures\\floor_roughness.png"))
    //     .EnableMips()
    //     .EnableSRGB();
    // auto ao_tex = gfx->CreateTexture(ao_desc);

    auto floor_mat = std::make_unique<Material>("pbr_floor", gpass_template_);
    floor_mat->SetTexTilingOffset({5.0f, 5.0f, 0.0f, 0.0f});

    floor_mat->SetProperty("albedo_tex", albedo_tex);
    floor_mat->SetProperty("normal_tex", normal_tex);
    floor_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    floor_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    floor_mat->SetProperty("emissive_tex", nullptr, Color::kBlack);
    floor_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

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

            RenderTargetBindingGuard rt_gurad(gfx, gbuffer_target_.get());

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

            {
                lighting_target_->Clear();
                RenderTargetBindingGuard gurad(gfx, lighting_target_.get());

                auto inverse_view = gfx->view().Inverted();
                //auto inverse_project = gfx->projection().Inverted();
                auto inverse_project = GetMainCamera()->projection(true).Inverted();

                assert(inverse_view);
                assert(inverse_project);

                FrameData frame_data = {
                    inverse_project.value(),
                    inverse_view.value()
                };

                frame_data_->Update(&frame_data);

                MaterialGuard mat_guard(gfx, lighting_mat_.get());

                gfx->Draw(3, 0);
            }

            RestoreCommonBindings();
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

}
}
