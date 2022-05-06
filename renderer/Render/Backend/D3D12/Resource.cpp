#include "Resource.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D12_RESOURCE_STATES ResourceState::GetState(uint32_t subresource) {
    if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || subresource_states.empty())
    {
        return state;
    }
    else
    {
        if (subresource_states.empty()) {
            subresource_states.resize(num_subresources, state);
        }
        
        return subresource_states[subresource];
    }
}

void ResourceState::SetState(D3D12_RESOURCE_STATES new_state, uint32_t subresource) {
    if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        state = new_state;
        subresource_states.clear();
    }
    else
    {
        if (subresource_states.empty()) {
            subresource_states.resize(num_subresources, state);
        }

        subresource_states[subresource] = new_state;
    }
}

D3D12Resource::D3D12Resource()
{

}

D3D12Resource::D3D12Resource(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state) :
    resource_(resource),
    desc_(resource->GetDesc()),
    plant_count_(CalculatePlantCount()),
    current_state_(CalculateNumSubresources(), state)
{
    if (desc_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        gpu_address_ = resource_->GetGPUVirtualAddress();
    }
}

D3D12Resource::D3D12Resource(const ResourceLocation& resource_location) : 
    resource_(resource_location.GetResource()),
    desc_(resource_location.GetDesc()),
    plant_count_(CalculatePlantCount()),
    current_state_(CalculateNumSubresources(), resource_location.GetState()),
    mapped_address_(resource_location.GetMappedAddress()),
    gpu_address_(resource_location.GetGpuAddress())
{

}

void D3D12Resource::Initialize(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state) {
    resource_ = resource;
    desc_ = resource->GetDesc();
    plant_count_ = CalculatePlantCount();
    current_state_ = { CalculateNumSubresources(), state };

    if (desc_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        gpu_address_ = resource_->GetGPUVirtualAddress();
    }
}

void D3D12Resource::Initialize(const ResourceLocation& resource_location) {
    resource_ = resource_location.GetResource();
    desc_ = resource_location.GetDesc();
    plant_count_ = CalculatePlantCount();
    current_state_ = { CalculateNumSubresources(), resource_location.GetState() };

    mapped_address_ = resource_location.GetMappedAddress();
    gpu_address_ = resource_location.GetGpuAddress();
}

UINT D3D12Resource::CalculateNumSubresources() const {
    if (desc_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
        return desc_.MipLevels * desc_.DepthOrArraySize * plant_count_;
    }
    // Buffer only contains 1 subresource
    return 1;
}

uint8_t D3D12Resource::CalculatePlantCount() const {
    auto device = D3D12GfxDriver::Instance()->GetDevice();
    return D3D12GetFormatPlaneCount(device, desc_.Format);
}

D3D12_RESOURCE_DESC D3D12Resource::GetDesc() {
    return desc_;
}

bool D3D12Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const
{
    return (format_support_.Support1 & formatSupport) != 0;
}

bool D3D12Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const
{
    return (format_support_.Support2 & formatSupport) != 0;
}

bool D3D12Resource::CheckUAVSupport() const
{
    return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
}

void D3D12Resource::SetDebugName(const TCHAR* name) {
    resource_->SetName(name);
    name_ = name;
}

const EngineString& D3D12Resource::GetDebugName() const {
    return name_;
}

D3D12_RESOURCE_STATES D3D12Resource::GetResourceState(uint32_t subresource) {
    return current_state_.GetState(subresource);
}

void D3D12Resource::SetResourceState(D3D12_RESOURCE_STATES state, uint32_t subresource) {
    current_state_.SetState(state, subresource);
}

bool D3D12Resource::IsUniformState() {
    return current_state_.subresource_states.empty();
}

uint32_t D3D12Resource::GetSubresourceNum() {
    return current_state_.num_subresources;
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

    format_support_.Format = desc_.Format;
    GfxThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, 
        &format_support_, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

ResourceLocation::ResourceLocation(const BuddyAllocBlock& block, ID3D12Heap* heap,
    const ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state, bool mapped) noexcept :
    block_(block),
    heap_(heap),
    resource_(res),
    desc_(res->GetDesc()),
    state_(state)
{
    if (mapped) {
        resource_->Map(0, nullptr, &mapped_address_);
    }

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

std::shared_ptr<D3D12Resource> ResourceLocation::CreateAliasResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state) {
    uint64_t offset = block_.align_offset;
    auto device = D3D12GfxDriver::Instance()->GetDevice();

    ComPtr<ID3D12Resource> res;
    GfxThrowIfFailed(device->CreatePlacedResource(heap_, offset, &desc, state, nullptr, IID_PPV_ARGS(&res)));

    return std::make_shared<D3D12Resource>(res, state);
}

}
}
