#pragma once

#include <d3d12.h>
#include <string>
#include "Common/Util.h"
#include "Common/Uncopyable.h"
#include "common/BuddyAllocator.h"
#include "DescriptorHeapAllocator.h"

namespace glacier {
namespace render {

class D3D12CommandList;

class D3D12Resource {
public:
    D3D12Resource();
    D3D12Resource(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    D3D12Resource(D3D12Resource&& other) = default;
    D3D12Resource& operator=(D3D12Resource&& other) = default;

    virtual ~D3D12Resource() = default;

    const ComPtr<ID3D12Resource>& GetUnderlyingResource() const { return resource_; }
    D3D12_RESOURCE_DESC GetDesc();

    void SetDebugName(const TCHAR* name);
    const EngineString& GetDebugName() const;

    D3D12_RESOURCE_STATES GetState() const { return state_; }
    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    void TransitionBarrier(D3D12CommandList* cmd_list, D3D12_RESOURCE_STATES new_state, bool flush=false);
    void Map(uint32_t subresource = 0, const D3D12_RANGE* range = nullptr);
    void Unmap();
    
protected:
    void CheckFeatureSupport();

    ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;

    void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support_;

    EngineString name_;
};

enum class AllocationType : uint8_t {
    kPlaced,
    kCommitted,
};

struct ResourceLocation : private Uncopyable {
    ResourceLocation() noexcept {}
    ResourceLocation(AllocationType type, const BuddyAllocBlock& block, ID3D12Heap* heap = nullptr, D3D12Resource* resource = nullptr) noexcept;
    ResourceLocation(ResourceLocation&& other) noexcept;
    ~ResourceLocation();

    ResourceLocation& operator=(ResourceLocation&& other) noexcept {
        if (this == &other) return *this;
        Swap(other);
        return *this;
    }

    bool IsEmpty() {
        return block.allocator == nullptr;
    }

    void Swap(ResourceLocation& other) noexcept {
        std::swap(is_alias, other.is_alias);
        std::swap(type, other.type);
        std::swap(block, other.block);
        if (type == AllocationType::kPlaced) {
            std::swap(source_heap, other.source_heap);
        }
        else {
            std::swap(source_resource, other.source_resource);
        }
        std::swap(resource, other.resource);
    }

    ResourceLocation GetAliasLocation() const {
        ResourceLocation location(type, block, source_heap, source_resource);
        location.is_alias = true;
        return location;
    }

    template<typename HeapAllocatorType>
    D3D12Resource& GetLocationResource() const {
        assert(block.allocator);
        auto allocator = static_cast<HeapAllocatorType::AllocatorPool::AllocatorType*>(block.allocator);
        return allocator->GetUnderlyingResource();
    }

    void* GetMappedAddress() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const;

    bool is_alias = false;
    AllocationType type;
    BuddyAllocBlock block;
    union {
        ID3D12Heap* source_heap = nullptr; //placed heap
        D3D12Resource* source_resource; //committed heap resource
    };
    
    //1. resource at placed heap, only for textures right now
    //2. read back buffer
    ComPtr<ID3D12Resource> resource;
#ifndef NDEBUG
    std::string name;
#endif
};

struct InflightResource {
    InflightResource() {}

    InflightResource(ResourceLocation&& res, uint64_t fence_value) :
        res(std::move(res)),
        fence_value(fence_value)
    {}

    InflightResource(D3D12DescriptorRange&& slot, uint64_t fence_value) :
        slot(std::move(slot)),
        fence_value(fence_value)
    {}

    ResourceLocation res;
    D3D12DescriptorRange slot;
    uint64_t fence_value = 0;
};

}
}
