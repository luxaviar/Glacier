#include "ForwardRenderer.h"
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
#include "Render/Base/RenderTarget.h"
#include "Common/BitUtil.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/RenderTexturePool.h"

namespace glacier {
namespace render {

ForwardRenderer::ForwardRenderer(GfxDriver* gfx, MSAAType msaa) :
    Renderer(gfx),
    msaa_(msaa)
{
    if (msaa_ != MSAAType::kNone) {
        gfx_->CheckMSAA(msaa_, sample_count_, quality_level_);
        assert(sample_count_ <= 8);

        msaa_ = (MSAAType)sample_count_;
    }
}

void ForwardRenderer::Setup() {
    if (init_) return;

    Renderer::Setup();

    InitMSAA();
    InitRenderGraph(gfx_);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    solid_mat->AddPass("solid");

    auto pbr_program = gfx_->CreateProgram("PBR", TEXT("ForwardLighting"), TEXT("ForwardLighting"));
    pbr_template_ = std::make_shared<MaterialTemplate>("PBR", pbr_program);
    pbr_template_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    pbr_template_->SetProperty("linear_sampler", ss);
    pbr_template_->AddPass("ForwardLighting");

    LightManager::Instance()->SetupMaterial(pbr_template_.get());
    csm_manager_->SetupMaterial(pbr_template_.get());

    InitHelmetPbr(gfx_);
    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

bool ForwardRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    msaa_render_target_->Resize(width, height);

    return true;
}

void ForwardRenderer::PreRender() {
    if (msaa_ != MSAAType::kNone) {
        msaa_render_target_->Clear();
    }

    Renderer::PreRender();
}

void ForwardRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    msaa_render_target_ = gfx_->CreateRenderTarget(width, height);

    if (msaa_ == MSAAType::kNone) {
        msaa_render_target_ = hdr_render_target_;
        return;
    }

    auto msaa_color = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
        CreateFlags::kRenderTarget, sample_count_, quality_level_);
    msaa_color->SetName(TEXT("msaa color buffer"));

    msaa_render_target_->AttachColor(AttachmentPoint::kColor0, msaa_color);

    auto depth_tex_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR24G8_TYPELESS)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource)
        .SetSampleDesc(sample_count_, quality_level_);
    auto depthstencil_texture = gfx_->CreateTexture(depth_tex_desc);
    depthstencil_texture->SetName(TEXT("msaa depth texture"));

    msaa_render_target_->AttachDepthStencil(depthstencil_texture);
}

void ForwardRenderer::InitMSAA() {
    RasterStateDesc rs;
    rs.depthFunc = CompareFunc::kAlways;

    auto vert_shader = gfx_->CreateShader(ShaderType::kVertex, TEXT("MSAAResolve"));

    std::array<std::pair<MSAAType, std::string>, 3> msaa_types = { { {MSAAType::k2x, "2"}, {MSAAType::k4x, "4"}, {MSAAType::k8x, "8"} } };
    for (auto& v : msaa_types) {
        auto pixel_shader = gfx_->CreateShader(ShaderType::kPixel, TEXT("MSAAResolve"), "main_ps", { {"MSAASamples_", v.second.c_str()}, {nullptr, nullptr} });
        auto program = gfx_->CreateProgram("MSAA");
        program->SetShader(vert_shader);
        program->SetShader(pixel_shader);

        auto mat = std::make_shared<Material>("msaa resolve", program);
        mat->GetTemplate()->SetRasterState(rs);

        mat->SetProperty("depth_buffer", msaa_render_target_->GetDepthStencil());
        mat->SetProperty("color_buffer", msaa_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_resolve_mat_[glacier::log2((uint32_t)v.first)] = mat;
    }
}

void ForwardRenderer::ResolveMSAA() {
    PerfSample("Resolve MSAA");
    if (msaa_ == MSAAType::kNone) return;

    auto& mat = msaa_resolve_mat_[log2((uint32_t)msaa_)];
    PostProcess(hdr_render_target_, mat.get());
}

void ForwardRenderer::InitHelmetPbr(GfxDriver* gfx) {
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
    pbr_mat->SetProperty("emissive_tex", emissive_tex);
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

void ForwardRenderer::InitDefaultPbr(GfxDriver* gfx) {
    auto pbr_mat = std::make_unique<Material>("pbr_default", pbr_template_);

    pbr_mat->SetProperty("albedo_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emissive_tex", nullptr, Color::kBlack);
    pbr_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 0;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    auto red_pbr = std::make_unique<Material>(*pbr_mat);
    red_pbr->name("red_pbr_default");
    red_pbr->SetProperty("albedo_tex", nullptr, Color::kRed);

    auto green_pbr = std::make_unique<Material>(*pbr_mat);
    green_pbr->name("green_pbr_default");
    green_pbr->SetProperty("albedo_tex", nullptr, Color::kGreen);

    auto orange_pbr = std::make_unique<Material>(*pbr_mat);
    orange_pbr->name("orange_pbr_default");
    orange_pbr->SetProperty("albedo_tex", nullptr, Color::kOrange);

    auto indigo_pbr = std::make_unique<Material>(*pbr_mat);
    indigo_pbr->name("indigo_pbr_default");
    indigo_pbr->SetProperty("albedo_tex", nullptr, Color::kIndigo);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
    MaterialManager::Instance()->Add(std::move(red_pbr));
    MaterialManager::Instance()->Add(std::move(green_pbr));
    MaterialManager::Instance()->Add(std::move(orange_pbr));
    MaterialManager::Instance()->Add(std::move(indigo_pbr));
}

void ForwardRenderer::InitFloorPbr(GfxDriver* gfx) {
    auto albedo_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_desc);

    auto normal_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_desc);

    auto pbr_mat = std::make_unique<Material>("pbr_floor", pbr_template_);
    pbr_mat->SetTexTilingOffset({5.0f, 5.0f, 0.0f, 0.0f});

    pbr_mat->SetProperty("albedo_tex", albedo_tex);
    pbr_mat->SetProperty("normal_tex", normal_tex);
    pbr_mat->SetProperty("metalroughness_tex", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("ao_tex", nullptr, Color::kWhite);
    pbr_mat->SetProperty("emissive_tex", nullptr, Color::kBlack);
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

void ForwardRenderer::AddLightingPass() {
    render_graph_.AddPass("ForwardLighting",
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

void ForwardRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddLightingPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

}
}
