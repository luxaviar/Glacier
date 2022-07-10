#pragma once

#include <d3d12.h>
#include "Render/Base/PipelineState.h"

namespace glacier {
namespace render {

class D3D12Program;
class D3D12RenderTarget;

class D3D12PipelineState : public PipelineState {
public:
    D3D12PipelineState(Program* program, const RasterStateDesc& rs, const InputLayoutDesc& layout);

    void SetName(const TCHAR* name) override;

    void SetInputLayout(const InputLayoutDesc& layout) override;
    void SetRasterState(const RasterStateDesc& rs) override;

    void Create(CommandBuffer* cmd_buffer);

    void Bind(CommandBuffer* cmd_buffer) override;

private:
    void CreatePsoDesc(CommandBuffer* cmd_buffer);

    D3D12_COMPUTE_PIPELINE_STATE_DESC compute_desc_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_;

    InputLayoutDesc layout_desc_;
    ComPtr<ID3D12PipelineState> pso_;
    std::shared_ptr<D3D12RenderTarget> render_target_;
};

}
}
