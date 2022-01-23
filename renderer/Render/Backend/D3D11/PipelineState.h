#pragma once

#include <d3d11_1.h>
#include <map>
#include "render/base/pipelinestate.h"

namespace glacier {
namespace render {

class D3D11RasterState : private Uncopyable {
public:
    static const std::shared_ptr<D3D11RasterState>& Create(const RasterStateDesc& rs);
    static void Clear();

    D3D11RasterState(const RasterStateDesc& rs);

    void Bind(GfxDriver* gfx);

private:
    RasterStateDesc state_;
    ComPtr<ID3D11RasterizerState> rasterizer_state_;
    ComPtr<ID3D11BlendState> blender_state_;
    ComPtr<ID3D11DepthStencilState> stencil_state_;

private:
    static std::map<uint64_t, std::shared_ptr<D3D11RasterState>> cache_;
};

class D3D11PipelineState : public PipelineState {
public:
    D3D11PipelineState(const RasterStateDesc& rs, const InputLayoutDesc& layout);

    void SetInputLayout(const InputLayoutDesc& layout) override;
    void SetRasterState(const RasterStateDesc& rs) override;

    void Bind(GfxDriver* gfx);

private:
    void Create();

    bool dirty_ = false;
    InputLayoutDesc layout_desc_;
    std::shared_ptr<D3D11RasterState> rso_;
    std::shared_ptr<InputLayout> layout_;
};

}
}
