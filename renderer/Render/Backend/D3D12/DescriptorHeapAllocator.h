#pragma once

#include <d3d12.h>
#include <vector>
#include <list>
//#include "Algorithm/BuddyAllocator.h"
#include "Common/Uncopyable.h"
#include "Common/BitUtil.h"
#include "Common/Util.h"
#include "Exception/Exception.h"
#include "d3dx12.h"

namespace glacier {
namespace render {

class D3D12DescriptorHeapAllocator;

struct D3D12DescriptorHeapEntry {
    typedef D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
    typedef decltype(DescriptorHandle::ptr) DescriptorPointerType;

    struct FreeRange
    {
        DescriptorPointerType begin;
        DescriptorPointerType end;
        uint32_t num;
    };

    ComPtr<ID3D12DescriptorHeap> heap = nullptr;
    std::vector<FreeRange> free_list;
};

struct D3D12DescriptorRange : private Uncopyable {
    D3D12DescriptorRange() noexcept {}
    D3D12DescriptorRange(D3D12DescriptorHeapAllocator* allocator, D3D12DescriptorHeapEntry* heap,
        D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t num) noexcept;
    D3D12DescriptorRange(D3D12DescriptorRange&& other) noexcept;
    ~D3D12DescriptorRange();

    D3D12DescriptorRange& operator=(D3D12DescriptorRange&& other) noexcept {
        if (this == &other) return *this;
        Swap(other);
        return *this;
    }

    void Swap(D3D12DescriptorRange& other) noexcept {
        std::swap(allocator, other.allocator);
        std::swap(heap, other.heap);
        std::swap(handle, other.handle);
        std::swap(num, other.num);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;

    D3D12DescriptorHeapAllocator* allocator = nullptr;
    D3D12DescriptorHeapEntry* heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    uint32_t num;
};

class D3D12DescriptorHeapAllocator
{
public:
    D3D12DescriptorHeapAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size);
    ~D3D12DescriptorHeapAllocator();

    D3D12DescriptorRange Allocate(uint32_t num = 1);
    void Deallocate(const D3D12DescriptorRange& Slot);

    uint32_t IncrementSize() const { return increment_size_; }

private:
    D3D12DescriptorHeapEntry* AllocateHeap();

    ID3D12Device* device_;
    D3D12_DESCRIPTOR_HEAP_DESC desc_;
    uint32_t increment_size_;

    std::list<D3D12DescriptorHeapEntry> heaps_;
};

}
}