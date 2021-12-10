#pragma once

#include "render/base/texture.h"
#include "render/material.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace render {

class Renderer;

struct PostProcessBuilder {
    PostProcessBuilder() {
        rs.depthWrite = false;
        rs.depthFunc = CompareFunc::kAlways;
    }

    PostProcessBuilder& SetPipelineState(const RasterState& state) { rs = state; return *this; }

    PostProcessBuilder& SetSrc(const std::shared_ptr<Texture>& src) { src_tex = src; return *this; }
    PostProcessBuilder& SetDst(const std::shared_ptr<Texture>& dst) { dst_tex = dst; return *this; }
    PostProcessBuilder& SetDst(const std::shared_ptr<RenderTarget>& dst) { dst_rt = dst; return *this; }

    PostProcessBuilder& SetDepth(const std::shared_ptr<Texture>& depth) { depth_tex = depth; return *this; }
    PostProcessBuilder& SetMaterial(const std::shared_ptr<PostProcessMaterial>& mat) { material = mat; return *this; }

    RasterState rs;
    std::shared_ptr<Texture> src_tex;
    std::shared_ptr<Texture> dst_tex;
    std::shared_ptr<RenderTarget> dst_rt;
    std::shared_ptr<Texture> depth_tex;
    std::shared_ptr<PostProcessMaterial> material;
};

class PostProcess {
public:
    static PostProcessBuilder Builder() { return {}; }

    PostProcess(const PostProcessBuilder& builder, Renderer* renderer);

    void Render(Renderer* renderer, Renderable* quad);

private:
    //RasterState state_;
    std::shared_ptr<Texture> screen_tex_;
    std::shared_ptr<Texture> depth_tex_;

    std::shared_ptr<RenderTarget> render_target_;
    std::shared_ptr<Material> material_;
};

class PostProcessManager : private Uncopyable {
public:
    PostProcessManager(Renderer* renderer);// : renderer_(renderer) {}

    const std::shared_ptr<Sampler>& GetSampler() const { return linear_sampler_; }

    void Push(const PostProcessBuilder& builder);
    void Render();

    void Process(Texture* src, RenderTarget* dst, PostProcessMaterial* mat);

private:
    Renderer* renderer_;
    Renderable* quad_;
    Material material_;
    std::shared_ptr<Sampler> linear_sampler_;
    std::vector<PostProcess> jobs_;
};

}
}
