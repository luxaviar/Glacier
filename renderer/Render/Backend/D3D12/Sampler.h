#pragma once

#include <map>
#include "Render/Base/SamplerState.h"
#include "Resource.h"

namespace glacier {
namespace render {

class GfxDriver;

class D3D12Sampler : public Sampler, public D3D12Resource {
public:
    static std::shared_ptr<D3D12Sampler> Create(GfxDriver* gfx, const SamplerState& ss);
    static void Clear();

    D3D12Sampler(GfxDriver* gfx, const SamplerState& ss);

    const D3D12DescriptorRange& GetDescriptorSlot() const { return sampler_slot_; }

private:
    static std::map<uint32_t, std::shared_ptr<D3D12Sampler>> cache_;
    D3D12DescriptorRange sampler_slot_;

};

}
}
