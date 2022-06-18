#pragma once

#include <d3d12.h>
#include <string>
#include "Algorithm/BuddyAllocator.h"
#include "Common/Util.h"
#include "Common/Uncopyable.h"
#include "DescriptorHeapAllocator.h"
#include "Util.h"

namespace glacier {
namespace render {

class D3D12CommandList;
class ResourceLocation;

struct ResourceState {
    ResourceState(uint32_t num_subresources, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON) :
        state(state),
        num_subresources(num_subresources)
    {}

    D3D12_RESOURCE_STATES GetState(uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SetState(D3D12_RESOURCE_STATES new_state, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

    D3D12_RESOURCE_STATES state;
    uint32_t num_subresources;
    std::vector<D3D12_RESOURCE_STATES> subresource_states;
};

class D3D12Resource {
public:
    D3D12Resource();
    D3D12Resource(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    D3D12Resource(const ResourceLocation& resource_location);

    D3D12Resource(D3D12Resource&& other) = default;
    D3D12Resource& operator=(D3D12Resource&& other) = default;

    virtual ~D3D12Resource() = default;

    const ComPtr<ID3D12Resource>& GetUnderlyingResource() const { return resource_; }
    D3D12_RESOURCE_DESC GetDesc();

    void SetDebugName(const TCHAR* name);
    const EngineString& GetDebugName() const;

    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;
    bool CheckUAVSupport() const;

    const D3D12_RESOURCE_DESC& GetDesc() const { return desc_; }

    D3D12_RESOURCE_STATES GetResourceState(uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SetResourceState(D3D12_RESOURCE_STATES state, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

    uint32_t GetSubresourceNum();
    bool IsUniformState();

    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    const D3D12DescriptorRange& GetDescriptorSlot() const { return descriptor_slot_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const { return descriptor_slot_.GetDescriptorHandle(); }
    //virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUavDescriptorHandle(uint32_t miplevel = 0) const { return {}; }

    void Map(uint32_t subresource = 0, const D3D12_RANGE* range = nullptr);
    void Unmap();

    UINT CalculateNumSubresources() const;
protected:
    uint8_t CalculatePlantCount() const;

    void Initialize(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    void Initialize(const ResourceLocation& resource_location);

    void CheckFeatureSupport();

    ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_DESC desc_ = {};
    uint8_t plant_count_ = 0;

    ResourceState current_state_ = { 0, D3D12_RESOURCE_STATE_COMMON };

    void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
    D3D12DescriptorRange descriptor_slot_ = {};

    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support_;
    EngineString name_;
};

enum class AllocationType : uint8_t {
    kPlaced,
    kCommitted,
};

class ResourceLocation : private Uncopyable {
public:
    ResourceLocation() noexcept {}
    ResourceLocation(const BuddyAllocBlock& block, ID3D12Heap* heap, const ComPtr<ID3D12Resource>& res,
        D3D12_RESOURCE_STATES state, bool mapped=false) noexcept;

    ResourceLocation(ResourceLocation&& other) noexcept;
    ~ResourceLocation();

    ResourceLocation& operator=(ResourceLocation&& other) noexcept;
    void Swap(ResourceLocation& other) noexcept;

    std::shared_ptr<D3D12Resource> CreateAliasResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state);

    bool IsEmpty() const { return block_.allocator == nullptr; }

    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    const ComPtr<ID3D12Resource>& GetResource() const { return resource_; }
    D3D12_RESOURCE_STATES GetState() const { return state_; }
    const D3D12_RESOURCE_DESC& GetDesc() const { return desc_; }

    size_t GetOffset() const { return block_.align_offset; }
    size_t GetSize() const { return block_.size; }

private:
    BuddyAllocBlock block_;
    ID3D12Heap* heap_ = nullptr;
    ComPtr<ID3D12Resource> resource_; //placed resource
    D3D12_RESOURCE_DESC desc_;

    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
};

struct InflightResource {
    InflightResource() {}

    InflightResource(ResourceLocation&& location, uint64_t fence_value) :
        location(std::move(location)),
        fence_value(fence_value)
    {}

    InflightResource(D3D12DescriptorRange&& slot, uint64_t fence_value) :
        slot(std::move(slot)),
        fence_value(fence_value)
    {}

    InflightResource(std::shared_ptr<D3D12Resource>&& res, uint64_t fence_value) :
        res(std::move(res)),
        fence_value(fence_value)
    {}

    std::shared_ptr<D3D12Resource> res;
    ResourceLocation location;
    D3D12DescriptorRange slot;
    uint64_t fence_value = 0;
};

}
}
