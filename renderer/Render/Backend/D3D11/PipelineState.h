#pragma once

#include <d3d11_1.h>
#include <map>
#include "render/base/pipelinestate.h"

namespace glacier {
namespace render {

class D3D11PipelineState : public PipelineState {
public:
    static const std::shared_ptr<D3D11PipelineState>& Create(const RasterState& rs);
    static void Clear();

    D3D11PipelineState(const RasterState& rs);

    void Bind() override;

private:
    ComPtr<ID3D11RasterizerState> rasterizer_state_;
    ComPtr<ID3D11BlendState> blender_state_;
    ComPtr<ID3D11DepthStencilState> stencil_state_;

private:
    static std::map<uint64_t, std::shared_ptr<D3D11PipelineState>> cache_;
};

}
}
