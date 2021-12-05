#include "sampler.h"
#include "Math/Util.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

std::map<uint32_t, std::shared_ptr<D3D11Sampler>> D3D11Sampler::cache_;

std::shared_ptr<D3D11Sampler> D3D11Sampler::Create(const SamplerState& ss) {
    auto sig = ss.u;
    auto it = cache_.find(sig);
    if (it != cache_.end()) {
        return it->second;
    }

    auto ret = std::make_shared<D3D11Sampler>(ss);
    cache_.emplace_hint(it, sig, ret);
    return ret;
}

void D3D11Sampler::Clear() {
    cache_.clear();
}

D3D11Sampler::D3D11Sampler(const SamplerState& ss) {
    D3D11_SAMPLER_DESC sampler_desc = CD3D11_SAMPLER_DESC{ CD3D11_DEFAULT{} };
    sampler_desc.Filter = GetUnderlyingFilter(ss.filter);
    sampler_desc.AddressU = GetUnderlyingWrap(ss.warpU);
    sampler_desc.AddressV = GetUnderlyingWrap(ss.warpV);
    sampler_desc.AddressW = GetUnderlyingWrap(ss.warpW);
    sampler_desc.ComparisonFunc = GetUnderlyingCompareFunc(ss.comp);

    if (ss.filter == FilterMode::kAnisotropic || ss.filter == FilterMode::kCmpAnisotropic) {
        sampler_desc.MaxAnisotropy = ss.max_anisotropy;
    }

    GfxThrowIfFailed(GfxDriverD3D11::Instance()->GetDevice()->CreateSamplerState(&sampler_desc, &sampler_state_));
}

D3D11Sampler::D3D11Sampler(const SamplerBuilder& builder) : Sampler() {
    D3D11_SAMPLER_DESC sampler_desc = CD3D11_SAMPLER_DESC{ CD3D11_DEFAULT{} };
    sampler_desc.Filter = GetUnderlyingFilter(builder.filter);
    sampler_desc.AddressU = GetUnderlyingWrap(builder.warp);
    sampler_desc.AddressV = GetUnderlyingWrap(builder.warp);
    sampler_desc.ComparisonFunc = GetUnderlyingCompareFunc(builder.cmp);
    sampler_desc.MipLODBias = builder.lod_bias;
    sampler_desc.MinLOD = builder.min_lod;
    sampler_desc.MaxLOD = builder.max_lod;

    if (builder.filter == FilterMode::kAnisotropic || builder.filter == FilterMode::kCmpAnisotropic) {
        sampler_desc.MaxAnisotropy = builder.max_anisotropy; //D3D11_REQ_MAXANISOTROPY;
    }

    if (builder.warp == WarpMode::kBorder) {
        sampler_desc.BorderColor[0] = builder.border_color[0];
        sampler_desc.BorderColor[1] = builder.border_color[1];
        sampler_desc.BorderColor[2] = builder.border_color[2];
        sampler_desc.BorderColor[3] = builder.border_color[3];
    }

    GfxThrowIfFailed(GfxDriverD3D11::Instance()->GetDevice()->CreateSamplerState(&sampler_desc, &sampler_state_));
}

void D3D11Sampler::Bind(ShaderType shader_type, uint16_t slot) {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    switch (shader_type)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(context->VSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(context->HSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(context->DSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(context->GSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(context->PSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(context->CSSetSamplers(slot, 1, sampler_state_.GetAddressOf()));
        break;
    default:
        throw std::exception{ "Invalid shader type for Sampler::Bind." };
        break;
    }
}

void D3D11Sampler::UnBind(ShaderType shader_type, uint16_t slot) {
    ID3D11SamplerState* null_sampler = nullptr;
    auto context = GfxDriverD3D11::Instance()->GetContext();
    switch (shader_type)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(context->VSSetSamplers(slot, 1, &null_sampler));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(context->HSSetSamplers(slot, 1, &null_sampler));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(context->DSSetSamplers(slot, 1, &null_sampler));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(context->GSSetSamplers(slot, 1, &null_sampler));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(context->PSSetSamplers(slot, 1, &null_sampler));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(context->CSSetSamplers(slot, 1, &null_sampler));
        break;
    default:
        throw std::exception{ "Invalid shader type for Sampler::UnBind." };
        break;
    }
}

}
}
