#include "PbrRenderer.h"
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

namespace glacier {
namespace render {

PbrRenderer::PbrRenderer(GfxDriver* gfx, MSAAType msaa) :
    Renderer(gfx, msaa)
{
}

void PbrRenderer::Setup() {
    if (init_) return;

    Renderer::Setup();

    InitRenderGraph(gfx_);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    solid_mat->AddPass("solid");

    auto pbr_program = gfx_->CreateProgram("PBR", TEXT("Pbr"), TEXT("Pbr"));
    pbr_template_ = std::make_shared<MaterialTemplate>("PBR", pbr_program);
    pbr_template_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    pbr_template_->SetProperty("linear_sampler", ss);
    pbr_template_->AddPass("Lighting");

    LightManager::Instance()->SetupMaterial(pbr_template_.get());
    csm_manager_->SetupMaterial(pbr_template_.get());

    InitHelmetPbr(gfx_);
    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

void PbrRenderer::InitHelmetPbr(GfxDriver* gfx) {
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

    auto pbr_mat = std::make_unique<Material>("pbr_helmet", pbr_template_);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;

    pbr_mat->SetProperty("linear_sampler", ss);
    pbr_mat->SetProperty("albedo_tex", albedo_tex);
    pbr_mat->SetProperty("normal_tex", normal_tex);
    pbr_mat->SetProperty("metalroughness_tex", metal_roughness_tex);
    pbr_mat->SetProperty("ao_tex", ao_tex);
    pbr_mat->SetProperty("emission_tex", emissive_tex);
    pbr_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
}

void PbrRenderer::InitDefaultPbr(GfxDriver* gfx) {
    auto pbr_mat = std::make_unique<Material>("pbr_default", pbr_template_);

    pbr_mat->SetProperty("albedo_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emission_tex", nullptr, Color::kBlack);
    pbr_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 0;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
}

void PbrRenderer::InitFloorPbr(GfxDriver* gfx) {
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

    auto pbr_mat = std::make_unique<Material>("pbr_floor", pbr_template_);
    pbr_mat->SetTexTilingOffset({5.0f, 5.0f, 0.0f, 0.0f});

    pbr_mat->SetProperty("albedo_tex", albedo_tex);
    pbr_mat->SetProperty("normal_tex", normal_tex);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emission_tex", nullptr, Color::kBlack);
    pbr_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
}

void PbrRenderer::SetCommonBinding(MaterialTemplate* mat) {
    LightManager::Instance()->SetupMaterial(mat);
    csm_manager_->SetupMaterial(mat);
    mat->SetInputLayout(Mesh::kDefaultLayout);
}

void PbrRenderer::AddLightingPass() {
    render_graph_.AddPass("Lighting",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto gfx = renderer->driver();
            PerfSample("Lighting Pass");

            LightManager::Instance()->Update();
            csm_manager_->Update();

            pass.Render(renderer, visibles_);
        });
}

void PbrRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddSolidPass();
    AddLightingPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

}
}
