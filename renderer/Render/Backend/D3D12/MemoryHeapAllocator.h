#pragma once

#include <d3d12.h>
#include "Algorithm/BuddyAllocator.h"
#include "Common/Uncopyable.h"
#include "Common/BitUtil.h"
#include "Common/Util.h"
#include "Exception/Exception.h"
#include "d3dx12.h"
#include "Resource.h"

namespace glacier {
namespace render {

#define DEFAULT_UPLOAD_HEAP_SIZE _256MB
#define DEFAULT_DEFAULT_HEAP_SIZE _256MB
#define DEFAULT_READBACK_HEAP_SIZE _128MB

#define UPLOAD_RESOURCE_ALIGNMENT 65536
#define DEFAULT_RESOURCE_ALIGNMENT 65536

template<uint32_t MaxOrder, uint32_t MinOrder = 4>
class D3D12PlacedHeapAllocator : private Uncopyable {
public:
    using AllocatorPool = BuddyAllocatorPool<ComPtr<ID3D12Heap>, MaxOrder, MinOrder>;

    D3D12PlacedHeapAllocator(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) :
        device_(device),
        desc_{},
        pool_(std::bind(&D3D12PlacedHeapAllocator::InitializeHeap, this))
    {
        CD3DX12_HEAP_PROPERTIES props(type);
        desc_.Properties = props;
        desc_.SizeInBytes = heap_size();
        desc_.Alignment = 0; //An alias for D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT: 64KB.
        desc_.Flags = flags;
    }
    virtual ~D3D12PlacedHeapAllocator() = default;

    ComPtr<ID3D12Heap> InitializeHeap() {
        ComPtr<ID3D12Heap> heap;
        GfxThrowIfFailed(device_->CreateHeap(&desc_, IID_PPV_ARGS(&heap)));
        return heap;
    }

    ResourceLocation AllocResource(size_t size, size_t alignment, const D3D12_RESOURCE_DESC& desc,
        D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear_value=nullptr)
    {
        auto block = pool_.Allocate(size, alignment);
        auto allocator = static_cast<AllocatorPool::AllocatorType*>(block.allocator);
        auto heap = allocator->GetUnderlyingResource().Get();

        ComPtr<ID3D12Resource> resource;
        uint64_t offset = block.align_offset;
        GfxThrowIfFailed(device_->CreatePlacedResource(heap, offset, &desc, state, clear_value, IID_PPV_ARGS(&resource)));

        return { block, heap, resource, state };// desc_.Properties.Type != D3D12_HEAP_TYPE_DEFAULT
    };

    constexpr uint32_t heap_size() const {  return AllocatorPool::kHeapSize; }

protected:
    ID3D12Device* device_;
    D3D12_HEAP_DESC desc_;

    AllocatorPool pool_;
};

class D3D12UploadBufferAllocator :
    public D3D12PlacedHeapAllocator<next_log_of2(DEFAULT_UPLOAD_HEAP_SIZE), next_log_of2(UPLOAD_RESOURCE_ALIGNMENT)>
{
public:
    D3D12UploadBufferAllocator(ID3D12Device* device, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ResourceLocation CreateResource(size_t size, size_t alignment,
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ);
};

class D3D12DefaultBufferAllocator {
public:
    using D3D12DefaultHeapAllocator = D3D12PlacedHeapAllocator<next_log_of2(DEFAULT_UPLOAD_HEAP_SIZE), next_log_of2(UPLOAD_RESOURCE_ALIGNMENT)>;
    using AllocatorPool = D3D12DefaultHeapAllocator::AllocatorPool;

    D3D12DefaultBufferAllocator(ID3D12Device* device);

    ResourceLocation CreateResource(const D3D12_RESOURCE_DESC& desc, uint32_t alignment,
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);

    ResourceLocation CreateVertexOrIndexBuffer(size_t size, size_t alignment,
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST);

private:
    D3D12DefaultHeapAllocator default_allocator_;
    D3D12DefaultHeapAllocator uav_allocator_;
    D3D12DefaultHeapAllocator vbo_allocator_;
};

class D3D12TextureResourceAllocator :
    public D3D12PlacedHeapAllocator<next_log_of2(DEFAULT_DEFAULT_HEAP_SIZE), next_log_of2(DEFAULT_RESOURCE_ALIGNMENT)>
{
public:
    D3D12TextureResourceAllocator(ID3D12Device* device, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);

    ResourceLocation CreateResource(const D3D12_RESOURCE_DESC& desc,
        D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear_value = nullptr);

private:
    ID3D12Device* device_;
};

class D3D12ReadbackBufferAllocator :
    public D3D12PlacedHeapAllocator<next_log_of2(DEFAULT_READBACK_HEAP_SIZE), next_log_of2(DEFAULT_RESOURCE_ALIGNMENT)>
{
public:
    D3D12ReadbackBufferAllocator(ID3D12Device* device, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ResourceLocation CreateResource(size_t size, size_t alignment = DEFAULT_RESOURCE_ALIGNMENT,
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST);
};

}
}