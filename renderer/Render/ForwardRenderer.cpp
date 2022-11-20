#include "ForwardRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "fmt/format.h"
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
#include "Render/Base/Program.h"
#include "Render/Base/CommandBuffer.h"
#include "PostProcess/GTAO.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(ForwardRenderer, ForwardRenderer)
LUX_CTOR(ForwardRenderer, GfxDriver*, MSAAType)
LUX_IMPL_END

ForwardRenderer::ForwardRenderer(GfxDriver* gfx, MSAAType msaa) :
    Renderer(gfx, msaa == MSAAType::kNone ? AntiAliasingType::kNone :AntiAliasingType::kMSAA),
    msaa_(msaa)
{

}

void ForwardRenderer::Setup() {
    pbr_program_ = gfx_->CreateProgram("PBR", TEXT("ForwardLighting"), TEXT("ForwardLighting"));
    pbr_program_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    pbr_program_->SetProperty("linear_sampler", ss);
    pbr_program_->AddPass("ForwardLighting");

    Renderer::Setup();
    msaa_.Setup(this);

    InitRenderGraph(gfx_);

    prepass_mat_ = std::make_shared<Material>("PrePass", TEXT("PrePass"), nullptr);
    prepass_mat_->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    depthnormal_mat_ = std::make_shared<Material>("DepthNormalCS", TEXT("DepthNormalCS"));
    depthnormal_mat_->SetProperty("DepthBuffer", prepass_render_target_->GetDepthStencil());
    depthnormal_mat_->SetProperty("NormalTexture", depthnormal_);

    ss.warpU = ss.warpV = WarpMode::kClamp;
    depthnormal_mat_->SetProperty("linear_sampler", ss);

    gtao_.Setup(this, prepass_render_target_->GetDepthStencil(), depthnormal_);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    solid_mat->AddPass("solid");
}

std::shared_ptr<Material> ForwardRenderer::CreateLightingMaterial(const char* name) {
    return std::make_shared<Material>(name, pbr_program_);
}

bool ForwardRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    depthnormal_->Resize(width, height);
    prepass_render_target_->Resize(width, height);
    msaa_.OnResize(width, height);

    return true;
}

void ForwardRenderer::PreRender(CommandBuffer* cmd_buffer) {
    msaa_.PreRender(this, cmd_buffer);

    prepass_render_target_->ClearDepthStencil(cmd_buffer);

    Renderer::PreRender(cmd_buffer);
}

void ForwardRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();

    auto predepth_texture = RenderTexturePool::Get(width, height, TextureFormat::kR24G8_TYPELESS,
        (CreateFlags)((uint32_t)CreateFlags::kDepthStencil | (uint32_t)CreateFlags::kShaderResource));
    predepth_texture->SetName("prepass depth texture");

    prepass_render_target_ = gfx_->CreateRenderTarget(width, height);
    prepass_render_target_->AttachDepthStencil(predepth_texture);

    depthnormal_ = RenderTexturePool::Get(width, height, TextureFormat::kR16G16_FLOAT,
        (CreateFlags)((uint32_t)CreateFlags::kUav | (uint32_t)CreateFlags::kShaderResource));
    depthnormal_->SetName("depth normal texture");
}

void ForwardRenderer::ResolveMSAA(CommandBuffer* cmd_buffer) {
    PerfSample("Resolve MSAA");
    msaa_.Execute(this, cmd_buffer);
}

void ForwardRenderer::AddPreDepthPass() {
    render_graph_.AddPass("prepass",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("predepth pass");

            prepass_render_target_->Bind(cmd_buffer);
            pass.Render(cmd_buffer, visibles_, prepass_mat_.get());

            BindLightingTarget(cmd_buffer);
        });
}

void ForwardRenderer::AddDepthNormalPass() {
    render_graph_.AddPass("depth normal",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("depth normal pass");

            auto dst_width = depthnormal_->width();
            auto dst_height = depthnormal_->height();

            cmd_buffer->BindMaterial(depthnormal_mat_.get());
            cmd_buffer->Dispatch(math::DivideByMultiple(dst_width, 8), math::DivideByMultiple(dst_height, 8), 1);

            cmd_buffer->UavResource(depthnormal_.get());

            //prepass_render_target_->Bind(cmd_buffer);
            //pass.Render(cmd_buffer, visibles_, prepass_mat_.get());

            //BindLightingTarget(cmd_buffer);
        });
}

void ForwardRenderer::AddLightingPass() {
    render_graph_.AddPass("ForwardLighting",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("Lighting Pass");

            LightManager::Instance()->Update(cmd_buffer);
            csm_manager_->Update();

            pass.Render(cmd_buffer, visibles_);
        });
}

void ForwardRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddPreDepthPass();
    AddDepthNormalPass();
    AddGtaoPass();
    AddShadowPass();
    AddLightingPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

void ForwardRenderer::DrawOptionWindow() {
    msaa_.DrawOptionWindow();
}


}
}
