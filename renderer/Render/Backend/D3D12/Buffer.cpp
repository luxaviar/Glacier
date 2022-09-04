#include "Buffer.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

void* D3D12Buffer::GetNativeResource() const {
    return location_.GetResource().Get();
}

void D3D12Buffer::Upload(CommandBuffer* cmd_buffer, const void* data, size_t size) {
    if (size == (size_t)-1) {
        size = size_;
    }

    assert(size <= size_);
    assert(type_ != BufferType::kConstantBuffer);

    UploadResource(cmd_buffer, data, size);
}

void D3D12Buffer::Update(const void* data, size_t size) {
    memcpy(location_.GetMappedAddress(), data, size);
}

void D3D12Buffer::UploadResource(CommandBuffer* cmd_buffer, const void* data, size_t size) {
    if (!data) return;

    auto gfx = D3D12GfxDriver::Instance();
    auto upload_allocator = gfx->GetUploadBufferAllocator();
    auto upload_location = upload_allocator->CreateResource(size, UPLOAD_RESOURCE_ALIGNMENT);
    auto command_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    auto mapped_address = upload_location.GetMappedAddress();
    memcpy(mapped_address, data, size);

    command_list->TransitionBarrier(this, ResourceAccessBit::kCopyDest);

    command_list->CopyBufferRegion(location_.GetResource().Get(), 0,
        upload_location.GetResource().Get(), 0, size);

    command_list->AddInflightResource(std::move(upload_location));
}

D3D12VertexBuffer::D3D12VertexBuffer(size_t size, size_t stride) :
    D3D12Buffer(BufferType::kVertexBuffer, size, stride)
{
    auto default_allocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateVertexOrIndexBuffer(size, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());
}

void D3D12VertexBuffer::Bind(CommandBuffer* cmd_buffer) {
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    constexpr auto target_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;

    D3D12_VERTEX_BUFFER_VIEW VBV;
    VBV.BufferLocation = gpu_address_;
    VBV.StrideInBytes = stride_;
    VBV.SizeInBytes = size_;

    cmd_list->TransitionBarrier(this, (ResourceAccessBit)target_state);
    cmd_list->IASetVertexBuffers(0, 1, &VBV);
}

D3D12IndexBuffer::D3D12IndexBuffer(size_t size, IndexFormat format) :
    D3D12Buffer(BufferType::kIndexBuffer, size),
    format_(format)
{
    count_ = size_ / (format_ == IndexFormat::kUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));

    auto default_allocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateVertexOrIndexBuffer(size, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());
}

void D3D12IndexBuffer::Bind(CommandBuffer* cmd_buffer) {
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    constexpr auto target_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;

    D3D12_INDEX_BUFFER_VIEW IBV;
    IBV.BufferLocation = gpu_address_;// location_.GetGpuAddress();
    IBV.Format = format_ == IndexFormat::kUInt32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    IBV.SizeInBytes = size_;

    cmd_list->TransitionBarrier(this, (ResourceAccessBit)target_state);
    cmd_list->IASetIndexBuffer(&IBV);
}

D3D12ConstantBuffer::D3D12ConstantBuffer(const void* data, size_t size, UsageType usage) :
    D3D12Buffer(BufferType::kConstantBuffer, size, 0, usage)
{
    if (usage == UsageType::kDynamic) {
        UpdateDynamic(data, size);
    }
    else {
        auto gfx = D3D12GfxDriver::Instance();
        auto upload_allocator = gfx->GetUploadBufferAllocator();
        location_ = upload_allocator->CreateResource(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        SetResourceState((ResourceAccessBit)location_.GetState());

        gpu_address_ = location_.GetGpuAddress();

        if (data) {
            memcpy(location_.GetMappedAddress(), data, size);
        }
    }
}

void D3D12ConstantBuffer::Update(const void* data, size_t size) {
    assert(size <= size_);
    assert(type_ == BufferType::kConstantBuffer);

    if (!data) return;

    if (usage_ == UsageType::kDynamic) {
        UpdateDynamic(data, size);
    }
    else {
        memcpy(location_.GetMappedAddress(), data, size);
    }
}

void D3D12ConstantBuffer::UpdateDynamic(const void* data, size_t size) {
    if (!data) return;

    auto gfx = D3D12GfxDriver::Instance();
    auto linear_allocator = gfx->GetLinearAllocator();
    linear_block_ = linear_allocator->Allocate(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    auto mapped_address = linear_block_.GetMappedAddress();
    memcpy(mapped_address, data, size);

    gpu_address_ = linear_block_.GetGpuAddress();
}

D3D12ByteAddressBuffer::D3D12ByteAddressBuffer(size_t element_count) :
    D3D12Buffer(BufferType::kByteAdressBuffer, element_count * 4, 4)
{
    count_ = element_count;

    D3D12_RESOURCE_DESC desc = {};
    desc.Alignment = 0;
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Height = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Width = (UINT64)size_;

    auto gfx = D3D12GfxDriver::Instance();
    auto default_allocator = gfx->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateResource(desc, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());

    auto device = gfx->GetDevice();
    auto descriptor_allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srv_slot_ = descriptor_allocator->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srv_desc.Buffer.NumElements = size_ / 4;
    //srv_desc.Buffer.FirstElement = location_.GetOffset() / 4;

    device->CreateShaderResourceView(location_.GetResource().Get(), &srv_desc, srv_slot_.GetDescriptorHandle());
}

D3D12RWByteAddressBuffer::D3D12RWByteAddressBuffer(size_t element_count) :
    D3D12ByteAddressBuffer(element_count)
{
    type_ = BufferType::kRWByteAdressBuffer;
    auto gfx = D3D12GfxDriver::Instance();
    auto device = gfx->GetDevice();

    auto descriptor_allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uav_slot_ = descriptor_allocator->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    uav_desc.Buffer.NumElements = size_ / 4;
    //uav_desc.Buffer.FirstElement = location_.GetOffset() / 4;

    device->CreateUnorderedAccessView(location_.GetResource().Get(), nullptr, &uav_desc, uav_slot_.GetDescriptorHandle());
}

D3D12StructuredBuffer::D3D12StructuredBuffer(size_t element_size, size_t element_count) :
    D3D12Buffer(BufferType::kStructuredBuffer, element_count * element_size, element_size)
{
    count_ = element_count;

    D3D12_RESOURCE_DESC desc = {};
    desc.Alignment = 0;
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Height = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Width = (UINT64)size_;

    auto gfx = D3D12GfxDriver::Instance();
    auto default_allocator = gfx->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateResource(desc, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());

    auto device = gfx->GetDevice();
    auto descriptor_allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srv_slot_ = descriptor_allocator->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srv_desc.Buffer.StructureByteStride = element_size;
    srv_desc.Buffer.NumElements = element_count;
    //srv_desc.Buffer.FirstElement = 0;

    device->CreateShaderResourceView(location_.GetResource().Get(), &srv_desc, srv_slot_.GetDescriptorHandle());
}

D3D12RWStructuredBuffer::D3D12RWStructuredBuffer(size_t element_size, size_t element_count) :
    D3D12StructuredBuffer(element_size, element_count)
{
    type_ = BufferType::kRWStructuredBuffer;
    auto gfx = D3D12GfxDriver::Instance();
    auto device = gfx->GetDevice();
    auto descriptor_allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uav_slot_ = descriptor_allocator->Allocate();

    counter_buffer_ = std::make_shared<D3D12RWByteAddressBuffer>(4);
    auto counter_res = static_cast<ID3D12Resource*>(counter_buffer_->GetNativeResource());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uav_desc.Buffer.StructureByteStride = element_size;
    uav_desc.Buffer.NumElements = element_count;
    uav_desc.Buffer.CounterOffsetInBytes = 0;

    device->CreateUnorderedAccessView(location_.GetResource().Get(), counter_res, &uav_desc, uav_slot_.GetDescriptorHandle());
}

}
}
