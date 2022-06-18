#include "Sampler.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"
#include "Util.h"

namespace glacier {
namespace render {

std::map<uint32_t, std::shared_ptr<D3D12Sampler>> D3D12Sampler::cache_;

std::shared_ptr<D3D12Sampler> D3D12Sampler::Create(GfxDriver* gfx, const SamplerState& ss) {
    auto sig = ss.u;
    auto it = cache_.find(sig);
    if (it != cache_.end()) {
        return it->second;
    }

    auto ret = std::make_shared<D3D12Sampler>(gfx, ss);
    cache_.emplace_hint(it, sig, ret);
    return ret;
}

void D3D12Sampler::Clear() {
    cache_.clear();
}

D3D12Sampler::D3D12Sampler(GfxDriver* gfx, const SamplerState& ss) :
    Sampler(ss)
{
    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto device = driver->GetDevice();

    D3D12_SAMPLER_DESC sampler_desc;
    sampler_desc.Filter = GetUnderlyingFilter(ss.filter);
    sampler_desc.AddressU = GetUnderlyingWrap(ss.warpU);
    sampler_desc.AddressV = GetUnderlyingWrap(ss.warpV);
    sampler_desc.AddressW = GetUnderlyingWrap(ss.warpW);
    sampler_desc.ComparisonFunc = GetUnderlyingCompareFunc(ss.comp);
    sampler_desc.MaxAnisotropy = 1;
    sampler_desc.MinLOD = -D3D12_FLOAT32_MAX;
    sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
    sampler_desc.MipLODBias = 0;
    sampler_desc.BorderColor[0] = 1.0f;
    sampler_desc.BorderColor[1] = 1.0f;
    sampler_desc.BorderColor[2] = 1.0f;
    sampler_desc.BorderColor[3] = 1.0f;
    
    if (ss.filter == FilterMode::kAnisotropic || ss.filter == FilterMode::kCmpAnisotropic) {
        sampler_desc.MaxAnisotropy = ss.max_anisotropy;
    }

    auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    descriptor_slot_ = descriptor_allocator->Allocate();
    device->CreateSampler(&sampler_desc, descriptor_slot_.GetDescriptorHandle());
}

}
}
