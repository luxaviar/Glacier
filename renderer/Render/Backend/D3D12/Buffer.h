#pragma once

#include <d3d12.h>
#include <string>
#include "Common/Uncopyable.h"
#include "Common/Util.h"
#include "MemoryHeapAllocator.h"
#include "Resource.h"
#include "DescriptorHeapAllocator.h"
#include "LinearAllocator.h"
#include "Render/Base/Buffer.h"

namespace glacier {
namespace render {

class D3D12Buffer : public Buffer {
public:
    // vertex
    D3D12Buffer(size_t size, size_t stride);

    // index
    D3D12Buffer(size_t size, IndexFormat format);

    //constant buffer
    D3D12Buffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic);

    void Bind(CommandBuffer* cmd_buffer) override;

    void* GetNativeResource() const;

    void Update(const void* data, size_t size) override;
    void Upload(CommandBuffer* cmd_buffer, const void* data, size_t size) override;

    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    const D3D12DescriptorRange& GetDescriptorSlot() const { return descriptor_slot_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const { return descriptor_slot_.GetDescriptorHandle(); }

protected:
    void UpdateDynamic(const void* data, size_t size);
    void UploadResource(CommandBuffer* cmd_buffer, const void* data, size_t size);


    ResourceLocation location_;
    void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
    D3D12DescriptorRange descriptor_slot_ = {};

    IndexFormat format_ = IndexFormat::kUInt16;
    LinearAllocBlock linear_block_; // for transient constant buffer
    D3D12DescriptorRange slot_;
};

}
}
