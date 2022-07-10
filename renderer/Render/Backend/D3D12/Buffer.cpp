#include "Buffer.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

D3D12Buffer::D3D12Buffer(size_t size, size_t stride) :
    Buffer(BufferType::kVertexBuffer, size, stride)
{
    auto default_allocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateVertexOrIndexBuffer(size, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());
}

D3D12Buffer::D3D12Buffer(size_t size, IndexFormat format) :
    Buffer(BufferType::kIndexBuffer, size),
    format_(format)
{

    count_ = size_ / (format_ == IndexFormat::kUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));

    auto default_allocator = D3D12GfxDriver::Instance()->GetDefaultBufferAllocator();
    location_ = default_allocator->CreateVertexOrIndexBuffer(size, DEFAULT_RESOURCE_ALIGNMENT);
    gpu_address_ = location_.GetGpuAddress();

    SetResourceState((ResourceAccessBit)location_.GetState());
}

D3D12Buffer::D3D12Buffer(const void* data, size_t size, UsageType usage) :
    Buffer(BufferType::kConstantBuffer, size, 0, usage)
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

void* D3D12Buffer::GetNativeResource() const {
    return location_.GetResource().Get();
}

void D3D12Buffer::Upload(CommandBuffer* cmd_buffer, const void* data, size_t size) {
    assert(size <= size_);
    assert(type_ == BufferType::kVertexBuffer || type_ == BufferType::kIndexBuffer);

    UploadResource(cmd_buffer, data, size);
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

void D3D12Buffer::UpdateDynamic(const void* data, size_t size) {
    if (!data) return;

    auto gfx = D3D12GfxDriver::Instance();
    auto linear_allocator = gfx->GetLinearAllocator();
    linear_block_ = linear_allocator->Allocate(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    auto mapped_address = linear_block_.GetMappedAddress();
    memcpy(mapped_address, data, size);

    gpu_address_ = linear_block_.GetGpuAddress();
}

void D3D12Buffer::Bind(CommandBuffer* cmd_buffer) {
    assert(type_ == BufferType::kIndexBuffer || type_ == BufferType::kVertexBuffer);

    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    constexpr auto target_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (type_ == BufferType::kVertexBuffer) {
        D3D12_VERTEX_BUFFER_VIEW VBV;
        VBV.BufferLocation = gpu_address_;
        VBV.StrideInBytes = stride_;
        VBV.SizeInBytes = size_;

        cmd_list->TransitionBarrier(this, (ResourceAccessBit)target_state);
        cmd_list->IASetVertexBuffers(0, 1, &VBV);
    }
    else {
        D3D12_INDEX_BUFFER_VIEW IBV;
        IBV.BufferLocation = gpu_address_;// location_.GetGpuAddress();
        IBV.Format = format_ == IndexFormat::kUInt32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        IBV.SizeInBytes = size_;

        cmd_list->TransitionBarrier(this, (ResourceAccessBit)target_state);
        cmd_list->IASetIndexBuffer(&IBV);
    }
}

void D3D12Buffer::Update(const void* data, size_t size) {
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

}
}
