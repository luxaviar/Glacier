#include "postprocess.h"
#include "render/renderer.h"
#include "Render/Mesh/Geometry.h"
#include "Render/Mesh/MeshRenderer.h"
#include "render/base/texture.h"
#include "render/base/rendertarget.h"
#include "render/material.h"
#include "render/base/sampler.h"

namespace glacier {
namespace render {

PostProcess::PostProcess(const PostProcessBuilder& builder, GfxDriver* gfx) :
    screen_tex_(builder.src_tex),
    depth_tex_(builder.depth_tex),
    material_(builder.material)
{
    assert(screen_tex_);
    assert(material_);

    material_->SetProperty("tex_sam", builder.sampler);

    if (!builder.dst_rt) {
        auto& tex = builder.dst_tex;
        assert(tex);
        //TODO: reuse temp rendertarget
        render_target_ = gfx->CreateRenderTarget(tex->width(), tex->height());
        render_target_->AttachColor(AttachmentPoint::kColor0, tex);
    } else {
        render_target_ = builder.dst_rt;
    }
}

void PostProcess::Render(GfxDriver* gfx) {
    RenderTargetGuard rt_guard(render_target_.get());
    MaterialGuard mat_guard(gfx, material_.get());
    TextureGurad tex_guard(screen_tex_.get(), ShaderType::kPixel, 0);

    gfx->Draw(3, 0);
}

PostProcessManager::PostProcessManager(GfxDriver* gfx) :
    gfx_(gfx)
{
    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;

    linear_sampler_ = gfx->CreateSampler(ss);
}

void PostProcessManager::Push(PostProcessBuilder& builder) {
    builder.sampler = linear_sampler_;

    jobs_.emplace_back(builder, gfx_);
}

void PostProcessManager::Render() {
    for (auto job : jobs_) {
        job.Render(gfx_);
    }
}

void PostProcessManager::Process(Texture* src, RenderTarget* dst, PostProcessMaterial* mat)
{
    MaterialGuard mat_guard(gfx_, mat);
    RenderTargetGuard rt_guard(dst);
    TextureGurad tex_guard(src, ShaderType::kPixel, 0);

    gfx_->Draw(3, 0);
}

}
}
