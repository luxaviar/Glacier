#include "Resource.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D12Resource::D3D12Resource()
{

}

D3D12Resource::D3D12Resource(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state) :
    resource_(resource),
    state_(state)
{
    if (resource_->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        gpu_address_ = resource_->GetGPUVirtualAddress();
    }

    //CheckFeatureSupport();
}

//D3D12Resource::D3D12Resource(D3D12Resource&& other) : D3D12Resource() {
//    resource_.Swap(other.resource_);
//    std::swap(state_, other.state_);
//    std::swap(mapped_address_, other.mapped_address_);
//    std::swap(gpu_address_, other.gpu_address_);
//    std::swap(format_support_, other.format_support_);
//    std::swap(name_, other.name_);
//}

void D3D12Resource::TransitionBarrier(D3D12CommandList* cmd_list, D3D12_RESOURCE_STATES new_state, bool flush) {
    if (state_ == new_state) return;

    // Queue barrier
    cmd_list->TransitionBarrier(resource_.Get(), state_, new_state, flush);

    // Store stage
    state_ = new_state;
}

D3D12_RESOURCE_DESC D3D12Resource::GetDesc() {
    D3D12_RESOURCE_DESC resDesc = {};
    if (resource_)
    {
        resDesc = resource_->GetDesc();
    }

    return resDesc;
}

void D3D12Resource::SetDebugName(const TCHAR* name) {
    resource_->SetName(name);
    name_ = name;
}

const EngineString& D3D12Resource::GetDebugName() const {
    return name_;
}

void D3D12Resource::Map(uint32_t subresource, const D3D12_RANGE* range) {
    if (!mapped_address_) {
        resource_->Map(subresource, range, &mapped_address_);
    }
}

void D3D12Resource::Unmap() {
    if (mapped_address_ != nullptr) {
        resource_->Unmap(0, nullptr);
        mapped_address_ = nullptr;
    }
}

void D3D12Resource::CheckFeatureSupport() {
    auto device = D3D12GfxDriver::Instance()->GetDevice();

    auto desc = resource_->GetDesc();
    format_support_.Format = desc.Format;
    GfxThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, 
        &format_support_, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

ResourceLocation::ResourceLocation(AllocationType type, const BuddyAllocBlock& block, 
    ID3D12Heap* heap, D3D12Resource* resource) noexcept :
    type(type),
    block(block)
{
    if (type == AllocationType::kPlaced) {
        source_heap = heap;
    }
    else {
        source_resource = resource;
    }
}

ResourceLocation::ResourceLocation(ResourceLocation&& other) noexcept :
    ResourceLocation()
{
    Swap(other);
}

ResourceLocation::~ResourceLocation() {
    if (block.allocator && !is_alias) {
        block.allocator->Deallocate(block);
    }
}

void* ResourceLocation::GetMappedAddress() const {
    assert(type == AllocationType::kCommitted);
    assert(source_resource->GetMappedAddress());

    return (uint8_t*)source_resource->GetMappedAddress() + block.align_offset;
}

D3D12_GPU_VIRTUAL_ADDRESS ResourceLocation::GetGpuAddress() const {
    assert(type == AllocationType::kCommitted);
    assert(source_resource->GetGpuAddress() != 0);

    return source_resource->GetGpuAddress() + block.align_offset;
}

}
}
