#include "ForwardRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include <imgui_impl_win32.h>
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

    //the occlusion texture the GTAO pass declares is imported by
    //InitRenderGraph, so it has to exist first
    gtao_.Setup(this, prepass_render_target_->GetDepthStencil(), depthnormal_);

    InitRenderGraph(gfx_);

    prepass_mat_ = std::make_shared<Material>("PrePass", TEXT("PrePass"), nullptr);
    prepass_mat_->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    depthnormal_mat_ = std::make_shared<Material>("DepthNormalCS", TEXT("DepthNormalCS"));
    depthnormal_mat_->SetProperty("DepthBuffer", prepass_render_target_->GetDepthStencil());
    depthnormal_mat_->SetProperty("NormalTexture", depthnormal_);

    ss.warpU = ss.warpV = WarpMode::kClamp;
    depthnormal_mat_->SetProperty("linear_sampler", ss);

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

void ForwardRenderer::AddPreDepthPass(GraphResourceHandle prepass_depth) {
    render_graph_.AddPass("prepass",
        [=](PassBuilder& b) {
            b.Write(prepass_depth, ResourceAccessBit::kDepthWrite);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
            PerfSample("predepth pass");

            prepass_render_target_->Bind(cmd_buffer);
            pass.Render(cmd_buffer, visibles_, prepass_mat_.get());

            BindLightingTarget(cmd_buffer);
        });
}

void ForwardRenderer::AddDepthNormalPass(GraphResourceHandle prepass_depth, GraphResourceHandle depth_normal) {
    render_graph_.AddPass("depth normal",
        [=](PassBuilder& b) {
            b.Read(prepass_depth, ResourceAccessBit::kNonPixelShaderRead);
            b.Write(depth_normal, ResourceAccessBit::kShaderWrite);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
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
        [this](PassBuilder& b) {
            b.Write(hdr_color_handle(), ResourceAccessBit::kRenderTarget);
            b.Write(hdr_depth_handle(), ResourceAccessBit::kDepthWrite);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
            PerfSample("Lighting Pass");

            LightManager::Instance()->Update(cmd_buffer);
            csm_manager_->Update();

            pass.Render(cmd_buffer, visibles_);
        });
}

void ForwardRenderer::InitRenderGraph(GfxDriver* gfx) {
    //what the passes of the forward stack declare they touch (§3.3). The
    //lighting target of this renderer is the MSAA target, which the graph does
    //not manage; the passes of the shared tail (skybox, outline) declare the
    //hdr pair, which is where the resolve puts the frame (see §8, T1.7).
    auto prepass_depth = render_graph_.Import("prepass depth",
        prepass_render_target_->GetDepthStencil().get(),
        ResourceAccessBit::kDepthWrite, ResourceAccessBit::kDepthWrite);
    auto depth_normal = render_graph_.Import("depth normal", depthnormal_.get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto ao_texture = render_graph_.Import("ao texture", gtao_.occlusion_texture().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);
    auto csm_shadow = render_graph_.Import("csm shadow map", csm_manager_->shadow_map().get(),
        ResourceAccessBit::kPixelShaderRead, ResourceAccessBit::kPixelShaderRead);

    AddPreDepthPass(prepass_depth);
    AddDepthNormalPass(prepass_depth, depth_normal);
    AddGtaoPass(prepass_depth, depth_normal, ao_texture);
    AddShadowPass(csm_shadow);
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
