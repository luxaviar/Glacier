#pragma once

#include "render/base/texture.h"
#include "render/material.h"
#include "Common/Uncopyable.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class GfxDriver;

struct PostProcessDescription {
    PostProcessDescription() {

    }

    PostProcessDescription& SetSrc(const std::shared_ptr<Texture>& src) { src_tex = src; return *this; }
    PostProcessDescription& SetDst(const std::shared_ptr<Texture>& dst) { dst_tex = dst; return *this; }
    PostProcessDescription& SetDst(const std::shared_ptr<RenderTarget>& dst) { dst_rt = dst; return *this; }

    PostProcessDescription& SetDepth(const std::shared_ptr<Texture>& depth) { depth_tex = depth; return *this; }
    PostProcessDescription& SetMaterial(const std::shared_ptr<PostProcessMaterial>& mat) { material = mat; return *this; }

    std::shared_ptr<Texture> src_tex;
    std::shared_ptr<Texture> dst_tex;
    std::shared_ptr<RenderTarget> dst_rt;
    std::shared_ptr<Texture> depth_tex;
    std::shared_ptr<PostProcessMaterial> material;

    SamplerState sampler;
};

class PostProcess {
public:
    static PostProcessDescription Description() { return {}; }

    PostProcess(const PostProcessDescription& desc, GfxDriver* gfx);

    void Render(GfxDriver* gfx);

private:
    //std::shared_ptr<Texture> screen_tex_;
    std::shared_ptr<Texture> depth_tex_;

    std::shared_ptr<RenderTarget> render_target_;
    std::shared_ptr<Material> material_;
};

class PostProcessManager : private Uncopyable {
public:
    PostProcessManager(GfxDriver* gfx);

    const SamplerState& GetSampler() const { return linear_sampler_; }

    void Push(PostProcessDescription& desc);
    void Render();

    void Process(RenderTarget* dst, PostProcessMaterial* mat);

private:
    GfxDriver* gfx_;
    SamplerState linear_sampler_;
    std::vector<PostProcess> jobs_;
};

}
}
