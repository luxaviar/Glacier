#include "pipelinestate.h"
#include <stdexcept>
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

std::map<uint64_t, std::shared_ptr<D3D11PipelineState>> D3D11PipelineState::cache_;

std::shared_ptr<D3D11PipelineState> D3D11PipelineState::Create(const RasterState& rs) {
    auto sig = rs.u;
    auto it = cache_.find(sig);
    if (it != cache_.end()) {
        return it->second;
    }

    auto ret = std::make_shared<D3D11PipelineState>(rs);
    cache_.emplace_hint(it, sig, ret);
    return ret;
}

void D3D11PipelineState::Clear() {
    cache_.clear();
}

D3D11PipelineState::D3D11PipelineState(const RasterState& rs) : PipelineState(rs) {
    //rasterizer
    D3D11_RASTERIZER_DESC rasterDesc = CD3D11_RASTERIZER_DESC( CD3D11_DEFAULT{} );
    rasterDesc.CullMode =  GetUnderlyingCullMode(rs.culling);
    rasterDesc.ScissorEnable = rs.scissor;

    auto device = GfxDriverD3D11::Instance()->GetDevice();

    GfxThrowIfFailed(device->CreateRasterizerState(&rasterDesc, &rasterizer_state_));

    //stencil & depth
    D3D11_DEPTH_STENCIL_DESC dsDesc = CD3D11_DEPTH_STENCIL_DESC{ CD3D11_DEFAULT{} };
    if (rs.HasStencilDepth()) {
        if (!rs.depthWrite) {
            dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        }

        if (rs.depthFunc == CompareFunc::kAlways) {
            dsDesc.DepthEnable = false;
        }

        dsDesc.DepthFunc = GetUnderlyingCompareFunc(rs.depthFunc);

        if (rs.stencilEnable) {
            dsDesc.StencilEnable = TRUE;
            //dsDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
            //dsDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
            dsDesc.FrontFace.StencilFunc = GetUnderlyingCompareFunc(rs.stencilFunc);
            dsDesc.FrontFace.StencilFailOp = GetUnderlyingStencilOP(rs.stencilFailOp);
            dsDesc.FrontFace.StencilDepthFailOp = GetUnderlyingStencilOP(rs.depthFailOp);
            dsDesc.FrontFace.StencilPassOp = GetUnderlyingStencilOP(rs.depthStencilPassOp);
        }
    }

    device->CreateDepthStencilState(&dsDesc, &stencil_state_);

    //blend
    D3D11_BLEND_DESC blendDesc = CD3D11_BLEND_DESC{ CD3D11_DEFAULT{} };
    blendDesc.AlphaToCoverageEnable = rs.alphaToCoverage;
    auto& brt = blendDesc.RenderTarget[0];

    if (rs.HasBlending())
    {
        brt.BlendEnable = TRUE;
        brt.SrcBlend = GetUnderlyingBlend(rs.blendFunctionSrcRGB);
        brt.DestBlend = GetUnderlyingBlend(rs.blendFunctionDstRGB);
        brt.BlendOp = GetUnderlyingBlendOP(rs.blendEquationRGB);

        brt.SrcBlendAlpha = GetUnderlyingBlend(rs.blendFunctionSrcAlpha);
        brt.DestBlendAlpha = GetUnderlyingBlend(rs.blendFunctionDstAlpha);
        brt.BlendOpAlpha = GetUnderlyingBlendOP(rs.blendEquationAlpha);
    }
    GfxThrowIfFailed(device->CreateBlendState(&blendDesc, &blender_state_));
}

void D3D11PipelineState::Bind() {
    auto driver = GfxDriverD3D11::Instance();
    const RasterState& last_rs = driver->raster_state();
    if (last_rs == state_) return;

    auto context = driver->GetContext();
    if (state_.culling != last_rs.culling || state_.scissor != last_rs.scissor) {
        GfxThrowIfAny(context->RSSetState(rasterizer_state_.Get()));
    }

    if (!state_.SameBlending(last_rs)) {
        GfxThrowIfAny(context->OMSetBlendState(blender_state_.Get(), nullptr, 0xFFFFFFFFu));
    }

    if (state_.depthFunc != last_rs.depthFunc || state_.depthWrite != last_rs.depthWrite ||
        state_.stencilEnable != last_rs.stencilEnable || state_.stencilFunc != last_rs.stencilFunc ||
        state_.stencilFailOp != last_rs.stencilFailOp || state_.depthFailOp != last_rs.depthFailOp ||
        state_.depthStencilPassOp != last_rs.depthStencilPassOp || state_.alphaToCoverage != last_rs.alphaToCoverage) {

        GfxThrowIfAny(context->OMSetDepthStencilState(stencil_state_.Get(), 0xFF));
    }

    if (state_.topology != last_rs.topology) {
        GfxThrowIfAny(context->IASetPrimitiveTopology(GetTopology(state_.topology)));
    }

    //driver->raster_state(state_);
}

}
}
