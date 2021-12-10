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

PostProcess::PostProcess(const PostProcessBuilder& builder, Renderer* renderer) :
    screen_tex_(builder.src_tex),
    depth_tex_(builder.depth_tex),
    material_(builder.material)
{
    assert(screen_tex_);
    assert(material_);

    if (!builder.dst_rt) {
        auto& tex = builder.dst_tex;
        assert(tex);
        //TODO: reuse temp rendertarget
        render_target_ = renderer->driver()->CreateRenderTarget(tex->width(), tex->height());
        render_target_->AttachColor(AttachmentPoint::kColor0, tex);
    } else {
        render_target_ = builder.dst_rt;
    }
}

void PostProcess::Render(Renderer* renderer, Renderable* quad) {
    auto gfx = renderer->driver();

    RenderTargetGuard rt_guard(render_target_.get());
    MaterialGuard mat_guard(gfx, material_.get());
    TextureGurad tex_guard(screen_tex_.get(), ShaderType::kPixel, 0);

    quad->Render(gfx);
}

PostProcessManager::PostProcessManager(Renderer* renderer) :
    renderer_(renderer),
    material_("_post_common", TEXT("PostProcessCommon"))
{
    VertexCollection vertices;
    IndexCollection indices;
    geometry::CreateQuad(vertices, indices);
    auto fullscreen_quad_mesh = std::make_shared<Mesh>(vertices, indices);

    auto& quad_go = GameObject::Create("full screen quad");
    quad_go.DontDestroyOnLoad(true);
    quad_go.Hide();

    quad_ = quad_go.AddComponent<MeshRenderer>(fullscreen_quad_mesh);
    quad_->SetPickable(false);
    quad_->SetCastShadow(false);
    quad_->SetReciveShadow(false);

    auto gfx = renderer->driver();
    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;

    linear_sampler_ = gfx->CreateSampler(ss);
}

void PostProcessManager::Push(const PostProcessBuilder& builder) {
    jobs_.emplace_back(builder, renderer_);
}

void PostProcessManager::Render() {
    MaterialGuard guard(renderer_->driver(), &material_);
    for (auto job : jobs_) {
        job.Render(renderer_, quad_);
    }
}

void PostProcessManager::Process(Texture* src, RenderTarget* dst, PostProcessMaterial* mat)
{
    auto gfx = renderer_->driver();
    MaterialGuard mat_guard(gfx, mat);
    RenderTargetGuard rt_guard(dst);
    TextureGurad tex_guard(src, ShaderType::kPixel, 0);

    quad_->Render(gfx);
}

}
}
