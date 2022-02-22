#include "PipelineState.h"
#include "GfxDriver.h"
#include "Exception/Exception.h"
#include "Util.h"
#include "Program.h"
#include "RenderTarget.h"

namespace glacier {
namespace render {

D3D12PipelineState::D3D12PipelineState(const RasterStateDesc& rs, const InputLayoutDesc& layout) :
    PipelineState(rs, layout),
    layout_desc_(layout),
    desc_{}
{
    desc_.SampleMask = UINT_MAX;
    desc_.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc_.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc_.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
}

void D3D12PipelineState::Create() {
    dirty_ = false;

    desc_.PrimitiveTopologyType = GetUnderlyingTopologyType(state_.topology);
    auto& rasterDesc = desc_.RasterizerState;
    rasterDesc.CullMode = GetUnderlyingCullMode(state_.culling);
    //rasterDesc.ScissorEnable = rs.scissor;
    rasterDesc.FillMode = state_.fillMode == FillMode::kSolid ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    rasterDesc.MultisampleEnable = TRUE;

    //stencil & depth
    auto& dsDesc = desc_.DepthStencilState;
    if (state_.HasStencilDepth()) {
        if (!state_.depthWrite) {
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }
        else {
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        }

        dsDesc.DepthEnable = state_.depthEnable;
        dsDesc.DepthFunc = GetUnderlyingCompareFunc(state_.depthFunc);

        if (state_.stencilEnable) {
            dsDesc.StencilEnable = TRUE;
            //dsDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
            //dsDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
            dsDesc.FrontFace.StencilFunc = GetUnderlyingCompareFunc(state_.stencilFunc);
            dsDesc.FrontFace.StencilFailOp = GetUnderlyingStencilOP(state_.stencilFailOp);
            dsDesc.FrontFace.StencilDepthFailOp = GetUnderlyingStencilOP(state_.depthFailOp);
            dsDesc.FrontFace.StencilPassOp = GetUnderlyingStencilOP(state_.depthStencilPassOp);
        }
    }

    //blend
    auto& blendDesc = desc_.BlendState;
    blendDesc.AlphaToCoverageEnable = state_.alphaToCoverage;
    auto& brt = blendDesc.RenderTarget[0];

    if (state_.HasBlending()) {
        brt.BlendEnable = TRUE;
        brt.SrcBlend = GetUnderlyingBlend(state_.blendFunctionSrcRGB);
        brt.DestBlend = GetUnderlyingBlend(state_.blendFunctionDstRGB);
        brt.BlendOp = GetUnderlyingBlendOP(state_.blendEquationRGB);

        brt.SrcBlendAlpha = GetUnderlyingBlend(state_.blendFunctionSrcAlpha);
        brt.DestBlendAlpha = GetUnderlyingBlend(state_.blendFunctionDstAlpha);
        brt.BlendOpAlpha = GetUnderlyingBlendOP(state_.blendEquationAlpha);
        brt.LogicOp = D3D12_LOGIC_OP_CLEAR;
    }

    auto layout_desc = layout_desc_.layout_desc<D3D12_INPUT_ELEMENT_DESC>();
    desc_.InputLayout.pInputElementDescs = layout_desc.data();
    desc_.InputLayout.NumElements = (uint32_t)layout_desc.size();

    auto device = D3D12GfxDriver::Instance()->GetDevice();
    GfxThrowIfFailed(device->CreateGraphicsPipelineState(&desc_, IID_PPV_ARGS(&pso_)));
}

void D3D12PipelineState::SetInputLayout(const InputLayoutDesc& layout) {
    layout_desc_ = layout;
    dirty_ = true;
}

void D3D12PipelineState::SetRasterState(const RasterStateDesc& rs) {
    state_ = rs;
    dirty_ = true;
}

void D3D12PipelineState::SetProgram(D3D12Program* program) {
    auto vs = program->GetShader(ShaderType::kVertex);
    if (vs) {
        desc_.VS = { vs->GetBytecode()->GetBufferPointer(), vs->GetBytecodeSize() };
    }
    
    auto ps = program->GetShader(ShaderType::kPixel);
    if (ps) {
        desc_.PS = { ps->GetBytecode()->GetBufferPointer(), ps->GetBytecodeSize() };
    }

    auto ds = program->GetShader(ShaderType::kDomain);
    if (ds) {
        desc_.DS = { ds->GetBytecode()->GetBufferPointer(), ds->GetBytecodeSize() };
    }

    auto hs = program->GetShader(ShaderType::kHull);
    if (hs) {
        desc_.HS = { hs->GetBytecode()->GetBufferPointer(), hs->GetBytecodeSize() };
    }

    auto gs = program->GetShader(ShaderType::kGeometry);
    if (gs) {
        desc_.GS = { gs->GetBytecode()->GetBufferPointer(), gs->GetBytecodeSize() };
    }

    desc_.pRootSignature = program->GetRootSignature();

    dirty_ = true;
}

void D3D12PipelineState::SetRenderTarget(const D3D12RenderTarget* rt) {
    if (!rt) return;

    auto rt_fmts = rt->GetRenderTargetFormats();
    auto dsv_fmt = rt->GetDepthStencilFormat();

    for (int i = 0; i < 8; ++i) {
        desc_.RTVFormats[i] = rt_fmts.RTFormats[i];
    }

    desc_.DSVFormat = dsv_fmt;
    desc_.SampleDesc = rt->GetSampleDesc();
    desc_.NumRenderTargets = rt_fmts.NumRenderTargets;

    dirty_ = true;
}

void D3D12PipelineState::Check(GfxDriver* gfx, D3D12Program* program) {
    if (!pso_) {
        auto driver = static_cast<D3D12GfxDriver*>(gfx);
        SetRenderTarget(driver->GetCurrentRenderTarget());
        SetProgram(program);
    }
}

void D3D12PipelineState::Bind(GfxDriver* gfx) {
    if (dirty_ || !pso_) {
        Create();
    }

    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto cmd_list = driver->GetCommandList()->GetUnderlyingCommandList();

    cmd_list->SetPipelineState(pso_.Get());
    cmd_list->SetGraphicsRootSignature(desc_.pRootSignature);
    cmd_list->IASetPrimitiveTopology(GetUnderlyingTopology(state_.topology));

    if (state_.HasBlending()) {
        FLOAT BlendFactor[4] = {1.0f,1.0f, 1.0f, 1.0f};
        cmd_list->OMSetBlendFactor(BlendFactor);
    }

    if (state_.stencilEnable) {
        cmd_list->OMSetStencilRef(0xff);
    }
}

}
}