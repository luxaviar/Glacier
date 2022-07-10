#include "Resource.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

ResourceLocation::ResourceLocation(const BuddyAllocBlock& block, ID3D12Heap* heap,
    const ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state) noexcept :
    block_(block),
    heap_(heap),
    resource_(res),
    desc_(res->GetDesc()),
    state_(state)
{
    if (desc_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        gpu_address_ = resource_->GetGPUVirtualAddress();
    }
}

ResourceLocation::ResourceLocation(ResourceLocation&& other) noexcept :
    ResourceLocation()
{
    Swap(other);
}

ResourceLocation::~ResourceLocation() {
    if (mapped_address_) {
        resource_->Unmap(0, nullptr);
    }

    if (block_.allocator) {
        block_.allocator->Deallocate(block_);
    }
}

ResourceLocation& ResourceLocation::operator=(ResourceLocation&& other) noexcept {
    if (this == &other) return *this;
    Swap(other);
    return *this;
}

void ResourceLocation::Swap(ResourceLocation& other) noexcept {
    std::swap(block_, other.block_);
    std::swap(heap_, other.heap_);
    std::swap(resource_, other.resource_);
    std::swap(desc_, other.desc_);
    std::swap(state_, other.state_);
    std::swap(gpu_address_, other.gpu_address_);
    std::swap(mapped_address_, other.mapped_address_);
}

void* ResourceLocation::GetMappedAddress(uint32_t subresource, const D3D12_RANGE* range) const {
    if (!mapped_address_) {
        resource_->Map(subresource, range, &mapped_address_);
    }
    return mapped_address_;
}

void ResourceLocation::Unmap() {
    if (mapped_address_) {
        resource_->Unmap(0, nullptr);
        mapped_address_ = nullptr;
    }
}

ComPtr<ID3D12Resource> ResourceLocation::CreateAliasResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state) {
    uint64_t offset = block_.align_offset;
    auto device = D3D12GfxDriver::Instance()->GetDevice();

    ComPtr<ID3D12Resource> res;
    GfxThrowIfFailed(device->CreatePlacedResource(heap_, offset, &desc, state, nullptr, IID_PPV_ARGS(&res)));
    return res;
}

}
}
