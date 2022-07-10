#pragma once

#include <map>
#include "Render/Base/SamplerState.h"
#include "DescriptorHeapAllocator.h"

namespace glacier {
namespace render {

class GfxDriver;

class D3D12Sampler : public Sampler {
public:
    static std::shared_ptr<D3D12Sampler> Create(GfxDriver* gfx, const SamplerState& ss);
    static void Clear();

    D3D12Sampler(GfxDriver* gfx, const SamplerState& ss);

    const D3D12DescriptorRange& GetDescriptorSlot() const { return descriptor_slot_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const { return descriptor_slot_.GetDescriptorHandle(); }

private:
    static std::map<uint32_t, std::shared_ptr<D3D12Sampler>> cache_;
    D3D12DescriptorRange descriptor_slot_;
};

}
}
