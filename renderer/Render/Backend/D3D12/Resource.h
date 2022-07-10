#pragma once

#include <d3d12.h>
#include <string>
#include <cstdint>
#include "Algorithm/BuddyAllocator.h"
#include "Common/Util.h"
#include "Common/Uncopyable.h"
#include "DescriptorHeapAllocator.h"
#include "Util.h"
#include "Render/Base/Resource.h"

namespace glacier {
namespace render {

enum class AllocationType : uint8_t {
    kPlaced,
    kCommitted,
};

class ResourceLocation : private Uncopyable {
public:
    ResourceLocation() noexcept {}
    ResourceLocation(const BuddyAllocBlock& block, ID3D12Heap* heap, const ComPtr<ID3D12Resource>& res,
        D3D12_RESOURCE_STATES state) noexcept;

    ResourceLocation(ResourceLocation&& other) noexcept;
    ~ResourceLocation();

    ResourceLocation& operator=(ResourceLocation&& other) noexcept;
    void Swap(ResourceLocation& other) noexcept;

    ComPtr<ID3D12Resource> CreateAliasResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state);

    bool IsEmpty() const { return block_.allocator == nullptr; }

    void* GetMappedAddress(uint32_t subresource = 0, const D3D12_RANGE* range = nullptr) const;
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    const ComPtr<ID3D12Resource>& GetResource() const { return resource_; }
    D3D12_RESOURCE_STATES GetState() const { return state_; }
    const D3D12_RESOURCE_DESC& GetDesc() const { return desc_; }

    size_t GetOffset() const { return block_.align_offset; }
    size_t GetSize() const { return block_.size; }

    template<typename T>
    T* Map(uint32_t subresource = 0, const D3D12_RANGE* range = nullptr) {
        return static_cast<T*>(GetMappedAddress(subresource, range));
    }

    void Unmap();

private:
    BuddyAllocBlock block_;
    ID3D12Heap* heap_ = nullptr;
    ComPtr<ID3D12Resource> resource_; //placed resource
    D3D12_RESOURCE_DESC desc_;

    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    mutable void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
};

struct InflightResource {
    InflightResource() {}

    InflightResource(ResourceLocation&& location, uint64_t fence_value=0) :
        location(std::move(location)),
        fence_value(fence_value)
    {}

    InflightResource(D3D12DescriptorRange&& slot, uint64_t fence_value=0) :
        slot(std::move(slot)),
        fence_value(fence_value)
    {}

    InflightResource(std::shared_ptr<Resource>&& res, uint64_t fence_value=0) :
        res(std::move(res)),
        fence_value(fence_value)
    {}

    std::shared_ptr<Resource> res;
    ResourceLocation location;
    D3D12DescriptorRange slot;
    uint64_t fence_value = 0;
};

}
}
