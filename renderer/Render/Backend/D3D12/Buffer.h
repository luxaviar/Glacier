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
#include "Render/Base/BufferLayout.h"
#include "Render/Base/VertexLayout.h"

namespace glacier {
namespace render {

class D3D12Buffer : public D3D12Resource {
public:
    D3D12Buffer(size_t size, bool create_default=true, bool is_vbo=false);

    const ResourceLocation& GetLocation() const { return location_; }
    const D3D12DescriptorRange& GetDescriptorSlot() const { return slot_; }

protected:
    void UpdateResource(const void* data, size_t size);

    bool vbo_;
    ResourceLocation location_;
    D3D12DescriptorRange slot_;
};

class D3D12VertexBuffer : public D3D12Buffer, public VertexBuffer {
public:
    D3D12VertexBuffer(size_t size_in_bytes, size_t stride_size);
    D3D12VertexBuffer(const void* data, size_t size_in_bytes, size_t stride_size);
    D3D12VertexBuffer(const VertexData& vdata);
    
    void Update(const void* data, size_t size) override {
        assert(size <= size_);
        UpdateResource(data, size);
    }

    void Bind(D3D12CommandList* command_list) const;

    void Bind() const override;
    void UnBind() const override {}

    void Bind(uint32_t slot, uint32_t offset) const override {}
    void UnBind(uint32_t slot, uint32_t offset) const override {}
};

class D3D12IndexBuffer : public D3D12Buffer, public IndexBuffer {
public:
    D3D12IndexBuffer(const void* data, size_t size, IndexFormat format);
    D3D12IndexBuffer(const std::vector<uint32_t>& indices);
    D3D12IndexBuffer(const std::vector<uint16_t>& indices);

    void Update(const void* data, size_t size) override;

    void Bind(D3D12CommandList* command_list) const;

    void Bind() const override;
    void UnBind() const override {}

};

class D3D12ConstantBuffer : public D3D12Buffer, public ConstantBuffer {
public:
    D3D12ConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic);
    D3D12ConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage = UsageType::kDynamic);

    void Update(const void* data, size_t size) override;

private:
    void UpdateDynamic(const void* data, size_t size);

    UsageType usage_;
    LinearAllocBlock linear_block_;
};

class D3D12StructuredBuffer : public D3D12Buffer, public StructuredBuffer {
public:
    D3D12StructuredBuffer(const void* data, size_t element_size, size_t element_count);

    void Update(const void* data, size_t size) override {
        assert(size <= size_);
        UpdateResource(data, size);
    }
};

class D3D12RWStructuredBuffer : public D3D12Buffer, public Buffer {
public:
    D3D12RWStructuredBuffer(uint32_t element_size, uint32_t element_count);
    const D3D12DescriptorRange& GetUavDescriptorSlot() const { return uav_slot_; }

    void Update(const void* data, size_t size) override {
        assert(size <= size_);
        UpdateResource(data, size);
    }

private:
    D3D12DescriptorRange uav_slot_;
};

class D3D12ReadbackBuffer : public D3D12Buffer, public Buffer {
public:
    D3D12ReadbackBuffer(uint32_t size);

    const void* Map() const;
    void Unmap() const;

    template<typename T>
    T const* Map() const { return reinterpret_cast<const T*>(Map()); };

    void Update(const void* data, size_t size) override {
        //assert(size <= size_);
        //UpdateResource(data, size);
    }
};

}
}
