#pragma once

#include <d3d12.h>
#include "Render/Base/PipelineState.h"

namespace glacier {
namespace render {

class D3D12Program;
class D3D12RenderTarget;

class D3D12PipelineState : public PipelineState {
public:
    D3D12PipelineState(const RasterStateDesc& rs, const InputLayoutDesc& layout);

    void SetInputLayout(const InputLayoutDesc& layout) override;
    void SetRasterState(const RasterStateDesc& rs) override;

    void CreatePSO(D3D12Program* program);

    void Bind(GfxDriver* gfx) override;

    void Check(GfxDriver* gfx, D3D12Program* program);

private:
    void Create();

    bool dirty_ = false;
    bool is_compute_ = false;

    D3D12_COMPUTE_PIPELINE_STATE_DESC compute_desc_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_;

    InputLayoutDesc layout_desc_;
    ComPtr<ID3D12PipelineState> pso_;
    D3D12Program* program_ = nullptr;
    std::shared_ptr<D3D12RenderTarget> render_target_;
};

}
}
