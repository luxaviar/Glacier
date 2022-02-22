#include "Buffer.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D12Buffer::D3D12Buffer(size_t size, bool create_default, bool is_vbo) : vbo_(is_vbo) {
    if (!create_default) {
        return;
    }

    auto default_allocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    if (vbo_) {
        location_ = default_allocator->AllocVertexOrIndexBuffer(size, DEFAULT_RESOURCE_ALIGNMENT);
    }
    else {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
        location_ = default_allocator->AllocResource(desc, DEFAULT_RESOURCE_ALIGNMENT);
    }

    resource_ = location_.resource;
    state_ = location_.source_resource->GetState();
}

void D3D12Buffer::UpdateResource(const void* data, size_t size) {
    if (!data) return;

    auto gfx = D3D12GfxDriver::Instance();
    auto upload_allocator = gfx->GetUploadBufferAllocator();
    auto upload_location = upload_allocator->AllocResource(size, UPLOAD_RESOURCE_ALIGNMENT);

    auto mapped_address = upload_location.GetMappedAddress();
    memcpy(mapped_address, data, size);

    auto& default_buffer = location_.GetLocationResource<D3D12DefaultBufferAllocator>();
    auto& upload_buffer = upload_location.GetLocationResource<D3D12UploadBufferAllocator>();

    auto command_list = gfx->GetCommandList();
    auto state = state_;
    default_buffer.TransitionBarrier(command_list, D3D12_RESOURCE_STATE_COPY_DEST);

    command_list->CopyBufferRegion(default_buffer, location_.block.align_offset,
        upload_buffer, upload_location.block.align_offset, size);

    default_buffer.TransitionBarrier(command_list, state);

    gfx->AddInflightResource(std::move(upload_location));
}

D3D12ConstantBuffer::D3D12ConstantBuffer(const void* data, size_t size, UsageType usage) :
    D3D12Buffer(size, false),
    ConstantBuffer(size),
    usage_(usage)
{
    if (usage == UsageType::kDynamic) {
        UpdateDynamic(data, size);
        return;
    }

    auto gfx = D3D12GfxDriver::Instance();
    auto upload_allocator = gfx->GetUploadBufferAllocator();
    location_ = upload_allocator->AllocResource(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

#ifndef NDEBUG
    location_.name = "ConstantBuffer";
#endif

    if (data) {
        auto mapped_address = location_.GetMappedAddress();
        memcpy(mapped_address, data, size);
    }

    gpu_address_ = location_.GetGpuAddress();
    //location_.source_resource->SetDebugName(TEXT("constant buffer"));
    //resource_ = location_.resource;
}

D3D12ConstantBuffer::D3D12ConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage) :
    D3D12ConstantBuffer(data->data(), data->data_size(), usage)
{
    data_ = data;
    version_ = data_->version();
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

void D3D12ConstantBuffer::Update(const void* data, size_t size) {
    assert(size <= size_);
    if (!data) return;

    if (usage_ == UsageType::kDynamic) {
        UpdateDynamic(data, size);
    }
    else {
        memcpy(location_.GetMappedAddress(), data, size);
    }
}

D3D12VertexBuffer::D3D12VertexBuffer(size_t size, size_t stride) :
    D3D12Buffer(size, true, true),
    VertexBuffer(size, stride)
{
#ifndef NDEBUG
    location_.name = "VertexBuffer";
#endif

    location_.source_resource->SetDebugName(TEXT("Vertex Buffer"));
}

D3D12VertexBuffer::D3D12VertexBuffer(const void* data, size_t size, size_t stride) :
    D3D12Buffer(size, true, true),
    VertexBuffer(size, stride)
{
    UpdateResource(data, size);

#ifndef NDEBUG
    location_.name = "VertexBuffer";
#endif

    location_.source_resource->SetDebugName(TEXT("Vertex Buffer"));
}

D3D12VertexBuffer::D3D12VertexBuffer(const VertexData& vdata) :
    D3D12VertexBuffer(vdata.data(), vdata.data_size(), vdata.stride())
{

}

void D3D12VertexBuffer::Bind(D3D12CommandList* command_list) const {
    //location_.GetLocationResource<D3D12DefaultBufferAllocator>().TransitionBarrier(command_list,
    //    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER);

    D3D12_VERTEX_BUFFER_VIEW VBV;
    VBV.BufferLocation = location_.GetGpuAddress();
    VBV.StrideInBytes = stride_;
    VBV.SizeInBytes = size_;
    command_list->IASetVertexBuffers(0, 1, &VBV);
}

void D3D12VertexBuffer::Bind() const {
    auto cmd_list = D3D12GfxDriver::Instance()->GetCommandList();
    Bind(cmd_list);
}

D3D12IndexBuffer::D3D12IndexBuffer(const void* data, size_t size, IndexFormat format) :
    D3D12Buffer(size, true, true),
    IndexBuffer(size, format)
{
    UpdateResource(data, size);
#ifndef NDEBUG
    location_.name = "IndexBuffer";
#endif

    location_.source_resource->SetDebugName(TEXT("Index Buffer"));
}

D3D12IndexBuffer::D3D12IndexBuffer(const std::vector<uint32_t>& indices) :
    D3D12IndexBuffer(indices.data(), indices.size() * sizeof(uint32_t), IndexFormat::kUInt32)
{

}

D3D12IndexBuffer::D3D12IndexBuffer(const std::vector<uint16_t>& indices) :
    D3D12IndexBuffer(indices.data(), indices.size() * sizeof(uint16_t), IndexFormat::kUInt16)
{

}

void D3D12IndexBuffer::Update(const void* data, size_t size) {
    assert(size <= size_);
    UpdateResource(data, size);

    count_ = size / ((format_ == IndexFormat::kUInt16) ? sizeof(uint16_t) : sizeof(uint32_t));
}

void D3D12IndexBuffer::Bind(D3D12CommandList* command_list) const {
    //location_.GetLocationResource<D3D12DefaultBufferAllocator>().TransitionBarrier(command_list,
    //    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER);

    D3D12_INDEX_BUFFER_VIEW IBV;
    IBV.BufferLocation = location_.GetGpuAddress();
    IBV.Format = format_ == IndexFormat::kUInt32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    IBV.SizeInBytes = size_;
    command_list->IASetIndexBuffer(&IBV);
}

void D3D12IndexBuffer::Bind() const {
    auto cmd_list = D3D12GfxDriver::Instance()->GetCommandList();
    Bind(cmd_list);
}

D3D12StructuredBuffer::D3D12StructuredBuffer(const void* data, size_t element_size, size_t element_count) :
    D3D12Buffer(element_size * element_count),
    StructuredBuffer(element_size, element_count)
{
    auto upload_allocator = D3D12GfxDriver::Instance()->GetUploadBufferAllocator();
    location_ = upload_allocator->AllocResource(size_, element_size);

#ifndef NDEBUG
    location_.name = "StructuredBuffer";
#endif
    void* mapped_addres = location_.GetMappedAddress();
    memcpy(mapped_addres, data, size_);

    auto& res = location_.GetLocationResource<D3D12UploadBufferAllocator>();

    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    desc.Buffer.StructureByteStride = element_size;
    desc.Buffer.NumElements = element_count;
    desc.Buffer.FirstElement = location_.block.align_offset / element_size;

    auto descriptor_allocator = D3D12GfxDriver::Instance()->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    slot_ = descriptor_allocator->Allocate();

    D3D12GfxDriver::Instance()->GetDevice()->CreateShaderResourceView(res.GetUnderlyingResource().Get(), &desc, slot_.GetDescriptorHandle());

    resource_ = location_.resource;
}

D3D12RWStructuredBuffer::D3D12RWStructuredBuffer(uint32_t element_size, uint32_t element_count) :
    D3D12Buffer(element_size * element_count),
    Buffer(BufferType::kRWStructuredBuffer, element_size* element_count)
{
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size_, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto DefaultBufferAllocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    location_ = DefaultBufferAllocator->AllocResource(desc, element_size);
#ifndef NDEBUG
    location_.name = "RWStructuredBuffer";
#endif
    auto& resource = location_.GetLocationResource<D3D12DefaultBufferAllocator>();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srv_desc.Buffer.StructureByteStride = element_size;
    srv_desc.Buffer.NumElements = element_count;
    srv_desc.Buffer.FirstElement = location_.block.align_offset / element_size;

    auto descriptor_allocator = D3D12GfxDriver::Instance()->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    slot_ = descriptor_allocator->Allocate();

    D3D12GfxDriver::Instance()->GetDevice()->CreateShaderResourceView(resource.GetUnderlyingResource().Get(), &srv_desc, slot_.GetDescriptorHandle());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uav_desc.Buffer.StructureByteStride = element_size;
    uav_desc.Buffer.NumElements = element_count;
    uav_desc.Buffer.FirstElement = location_.block.align_offset / element_size;
    uav_desc.Buffer.CounterOffsetInBytes = 0;

    uav_slot_ = descriptor_allocator->Allocate();

    D3D12GfxDriver::Instance()->GetDevice()->CreateUnorderedAccessView(resource.GetUnderlyingResource().Get(), nullptr, &uav_desc, uav_slot_.GetDescriptorHandle());

    resource_ = location_.resource;
}

D3D12ReadbackBuffer::D3D12ReadbackBuffer(uint32_t size) :
    D3D12Buffer(size),
    Buffer(BufferType::kReadBackBuffer, size)
{
    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto res_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    GfxThrowIfFailed(D3D12GfxDriver::Instance()->GetDevice()->CreateCommittedResource(
        &heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &res_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource_)
    ));
}

const void* D3D12ReadbackBuffer::Map() const {
    assert(resource_);
    void* data = nullptr;
    resource_->Map(0, nullptr, &data);
    return data;
}

void D3D12ReadbackBuffer::Unmap() const {
    assert(resource_);
    resource_->Unmap(0, nullptr);
}

}
}
