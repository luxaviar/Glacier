#include "MemoryHeapAllocator.h"
#include <memory>

namespace glacier {
namespace render {

D3D12UploadBufferAllocator::D3D12UploadBufferAllocator(ID3D12Device* device, D3D12_RESOURCE_FLAGS flags) :
    D3D12CommittedHeapAllocator(device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, flags)
{

}

D3D12DefaultBufferAllocator::D3D12DefaultBufferAllocator(ID3D12Device* device) :
    default_allocator_(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON),
    uav_allocator_(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
    vbo_allocator_(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER)
{

}

ResourceLocation D3D12DefaultBufferAllocator::AllocResource(const D3D12_RESOURCE_DESC& desc, uint32_t alignment) {
    if (desc.Flags == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        return uav_allocator_.AllocResource((uint32_t)desc.Width, alignment);
    } else {
        return default_allocator_.AllocResource((uint32_t)desc.Width, alignment);
    }
}

ResourceLocation D3D12DefaultBufferAllocator::AllocVertexOrIndexBuffer(size_t size, size_t alignment) {
    return vbo_allocator_.AllocResource(size, alignment);
}

D3D12TextureResourceAllocator::D3D12TextureResourceAllocator(ID3D12Device* device, D3D12_HEAP_FLAGS flags) :
    D3D12PlacedHeapAllocator(device, D3D12_HEAP_TYPE_DEFAULT, flags),
    device_(device)
{

}

ResourceLocation D3D12TextureResourceAllocator::AllocResource(const D3D12_RESOURCE_DESC& desc,
    const D3D12_RESOURCE_STATES& state, const D3D12_CLEAR_VALUE* clear_value)
{
    const D3D12_RESOURCE_ALLOCATION_INFO info = device_->GetResourceAllocationInfo(0, 1, &desc);
    return CreateResource((size_t)info.SizeInBytes, DEFAULT_RESOURCE_ALIGNMENT, desc, state, clear_value);
}

}
}
