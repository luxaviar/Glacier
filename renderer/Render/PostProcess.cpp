#include "postprocess.h"
#include "render/renderer.h"
#include "Render/Mesh/Geometry.h"
#include "Render/Mesh/MeshRenderer.h"
#include "render/base/texture.h"
#include "render/base/rendertarget.h"
#include "render/material.h"
#include "Render/Base/Buffer.h"

namespace glacier {
namespace render {

PostProcess::PostProcess(const PostProcessDescription& desc, GfxDriver* gfx) :
    //screen_tex_(builder.src_tex),
    depth_tex_(desc.depth_tex),
    material_(desc.material)
{
    assert(desc.src_tex);
    assert(material_);

    material_->SetProperty("tex_sam", desc.sampler);
    material_->SetProperty("tex", desc.src_tex);

    if (!desc.dst_rt) {
        auto& tex = desc.dst_tex;
        assert(tex);
        //TODO: reuse temp rendertarget
        render_target_ = gfx->CreateRenderTarget(tex->width(), tex->height());
        render_target_->AttachColor(AttachmentPoint::kColor0, tex);
    } else {
        render_target_ = desc.dst_rt;
    }
}

void PostProcess::Render(GfxDriver* gfx) {
    RenderTargetBindingGuard rt_guard(gfx, render_target_.get());
    MaterialGuard mat_guard(gfx, material_.get());
    //TextureBindingGurad tex_guard(screen_tex_.get(), ShaderType::kPixel, 0);

    gfx->Draw(3, 0);
}

PostProcessManager::PostProcessManager(GfxDriver* gfx) :
    gfx_(gfx)
{
    linear_sampler_.warpU = linear_sampler_.warpV = WarpMode::kClamp;
}

void PostProcessManager::Push(PostProcessDescription& desc) {
    desc.sampler = linear_sampler_;

    jobs_.emplace_back(desc, gfx_);
}

void PostProcessManager::Render() {
    for (auto job : jobs_) {
        job.Render(gfx_);
    }
}

void PostProcessManager::Process(RenderTarget* dst, PostProcessMaterial* mat)
{
    RenderTargetBindingGuard rt_guard(gfx_, dst);
    MaterialGuard mat_guard(gfx_, mat);
    //TextureBindingGurad tex_guard(src, ShaderType::kPixel, 0);

    gfx_->Draw(3, 0);
}

}
}
