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
    using Buffer::Buffer;

    void* GetNativeResource() const override;
    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    //for vertex & index buffer
    void Bind(CommandBuffer* cmd_buffer, size_t offset) override {}
    void Upload(CommandBuffer* cmd_buffer, const void* data, size_t size = (size_t)-1) override;

    //for constant/structure buffer
    void Update(const void* data, size_t size) override;
    void Update(size_t offset, const void* data, size_t size) override;

    //for structure buffer
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return {}; }
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUavHandle(uint32_t offset = 0) const { return {}; }

protected:
    void UploadResource(CommandBuffer* cmd_buffer, const void* data, size_t size);

    ResourceLocation location_;
    void* mapped_address_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
};

class D3D12IndexBuffer : public D3D12Buffer {
public:
    D3D12IndexBuffer(size_t size, IndexFormat format);
    void Bind(CommandBuffer* cmd_buffer, size_t offset) override;

protected:
    IndexFormat format_ = IndexFormat::kUInt16; //for index buffer
};

class D3D12VertexBuffer : public D3D12Buffer {
public:
    //flags say which shader views the buffer needs on top of being drawn from:
    //kShaderResource to be read by a compute pass (the bind pose of a skinned
    //mesh) and kUav to be written by one (the skinned vertices)
    D3D12VertexBuffer(size_t size, size_t stride, CreateFlags flags = CreateFlags::kNone);
    void Bind(CommandBuffer* cmd_buffer, size_t offset) override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const override { return srv_slot_.GetDescriptorHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetUavHandle(uint32_t offset = 0) const override { return uav_slot_.GetDescriptorHandle(offset); }

protected:
    D3D12DescriptorRange srv_slot_ = {};
    D3D12DescriptorRange uav_slot_ = {};
};

class D3D12ConstantBuffer : public D3D12Buffer {
public:
    D3D12ConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic);

    void Update(const void* data, size_t size) override;
    //a dynamic constant buffer hands out an address per update, so a range of
    //it cannot be written afterwards
    void Update(size_t offset, const void* data, size_t size) override;

protected:
    void UpdateDynamic(const void* data, size_t size);

    LinearAllocBlock linear_block_; // for transient constant buffer
};

class D3D12ByteAddressBuffer : public D3D12Buffer {
public:
    D3D12ByteAddressBuffer(size_t element_count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const override { return srv_slot_.GetDescriptorHandle(); }

protected:
    D3D12DescriptorRange srv_slot_ = {};
};

class D3D12RWByteAddressBuffer : public D3D12ByteAddressBuffer {
public:
    D3D12RWByteAddressBuffer(size_t element_count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetUavHandle(uint32_t offset = 0) const override { return uav_slot_.GetDescriptorHandle(offset); }

protected:
    D3D12DescriptorRange uav_slot_ = {};
};

class D3D12StructuredBuffer : public D3D12Buffer {
public:
    D3D12StructuredBuffer(size_t element_size, size_t element_count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const override { return srv_slot_.GetDescriptorHandle(); }

protected:
    D3D12DescriptorRange srv_slot_ = {};
};

//a structured buffer that lives in an upload heap, so the CPU can keep it up to
//date every frame; the per frame pools (bone matrices, instances) are built on
//it instead of the shared transient buffers, which only carry one object
class D3D12DynamicStructuredBuffer : public D3D12Buffer {
public:
    D3D12DynamicStructuredBuffer(size_t element_size, size_t element_count);

    void Bind(CommandBuffer* cmd_buffer, size_t offset) override {}
    void Upload(CommandBuffer* cmd_buffer, const void* data, size_t size = (size_t)-1) override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const override { return srv_slot_.GetDescriptorHandle(); }

protected:
    D3D12DescriptorRange srv_slot_ = {};
};

class D3D12RWStructuredBuffer : public D3D12StructuredBuffer {
public:
    D3D12RWStructuredBuffer(size_t element_size, size_t element_count);
    D3D12_CPU_DESCRIPTOR_HANDLE GetUavHandle(uint32_t offset = 0) const override { return uav_slot_.GetDescriptorHandle(offset); }

protected:
    D3D12DescriptorRange uav_slot_ = {};
    std::shared_ptr<D3D12RWByteAddressBuffer> counter_buffer_;
};

}
}
