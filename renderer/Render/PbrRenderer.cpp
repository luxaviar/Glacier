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
#include "Render/Base/Sampler.h"

namespace glacier {
namespace render {

PbrRenderer::PbrRenderer(GfxDriver* gfx) : Renderer(gfx) {

}

void PbrRenderer::Setup() {
    Renderer::Setup();

    InitRenderGraph(gfx_);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    solid_mat->AddPass("solid");

    pbr_program_ = gfx_->CreateProgram("Pbr", TEXT("Pbr"), TEXT("Pbr"));

    InitHelmetPbr(gfx_);
    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

void PbrRenderer::InitHelmetPbr(GfxDriver* gfx) {
    auto albedo_builder = Texture::Builder()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_builder);

    auto normal_builder = Texture::Builder()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_builder);

    auto metal_builder = Texture::Builder()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_metalroughness.png"))
        .EnableMips();
    auto metal_roughness_tex = gfx->CreateTexture(metal_builder);

    auto ao_builder = Texture::Builder()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_occlusion.png"))
        .EnableMips()
        .EnableSRGB();
    auto ao_tex = gfx->CreateTexture(ao_builder);

    auto em_builder = Texture::Builder()
        .SetFile(TEXT("assets\\model\\helmet\\helmet_emission.png"))
        .EnableMips()
        .EnableSRGB();
    auto emissive_tex = gfx->CreateTexture(em_builder);

    auto pbr_mat = MaterialManager::Instance()->Create("pbr_helmet");
    pbr_mat->SetProgram(pbr_program_);
    pbr_mat->AddPass("pbr");

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;
    auto linear_sampler = gfx->CreateSampler(ss);

    pbr_mat->SetProperty("linear_sampler", linear_sampler);
    pbr_mat->SetProperty("albedo_tex", albedo_tex);
    pbr_mat->SetProperty("normal_tex", normal_tex);
    pbr_mat->SetProperty("metalroughness_tex", metal_roughness_tex);
    pbr_mat->SetProperty("ao_tex", ao_tex);
    pbr_mat->SetProperty("emission_tex", emissive_tex);

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param);
    pbr_mat->SetProperty("object_material", mat_cbuf);
}

void PbrRenderer::InitDefaultPbr(GfxDriver* gfx) {
    auto pbr_mat = MaterialManager::Instance()->Create("pbr_default");
    pbr_mat->SetProgram(pbr_program_);
    pbr_mat->AddPass("pbr");

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;
    auto linear_sampler = gfx->CreateSampler(ss);
    
    pbr_mat->SetProperty("linear_sampler", linear_sampler);

    pbr_mat->SetProperty("albedo_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emission_tex", nullptr, Color::kBlack);

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 0;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param);
    pbr_mat->SetProperty("object_material", mat_cbuf);
}

void PbrRenderer::InitFloorPbr(GfxDriver* gfx) {
    auto albedo_builder = Texture::Builder()
        .SetFile(TEXT("assets\\textures\\floor_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_builder);

    auto normal_builder = Texture::Builder()
        .SetFile(TEXT("assets\\textures\\floor_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_builder);

    auto metal_builder = Texture::Builder()
        .SetFile(TEXT("assets\\textures\\floor_metallic.png"))
        .EnableMips();
    auto metal_roughness_tex = gfx->CreateTexture(metal_builder);

    auto ao_builder = Texture::Builder()
        .SetFile(TEXT("assets\\textures\\floor_roughness.png"))
        .EnableMips()
        .EnableSRGB();
    auto ao_tex = gfx->CreateTexture(ao_builder);

    auto pbr_mat = MaterialManager::Instance()->Create("pbr_floor");
    pbr_mat->SetProgram(pbr_program_);
    pbr_mat->AddPass("pbr");
    pbr_mat->SetTexTilingOffset({5.0f, 5.0f, 0.0f, 0.0f});
    
    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    auto linear_sampler = gfx->CreateSampler(ss);

    pbr_mat->SetProperty("albedo_tex", albedo_tex);
    pbr_mat->SetProperty("normal_tex", normal_tex);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emission_tex", nullptr, Color::kBlack);
    pbr_mat->SetProperty("linear_sampler", linear_sampler);

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param);
    pbr_mat->SetProperty("object_material", mat_cbuf);
}

void PbrRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddSolidPass();
    AddPhongPass();
    AddPbrPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

}
}
