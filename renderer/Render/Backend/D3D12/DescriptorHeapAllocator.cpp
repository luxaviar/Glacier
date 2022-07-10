#include "DescriptorHeapAllocator.h"
#include <cassert>

namespace glacier {
namespace render {

D3D12DescriptorRange::D3D12DescriptorRange(D3D12DescriptorHeapAllocator* allocator, D3D12DescriptorHeapEntry* heap,
    D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t num) noexcept :
    allocator(allocator),
    heap(heap),
    handle(handle),
    num(num)
{

}

D3D12DescriptorRange::D3D12DescriptorRange(D3D12DescriptorRange&& other) noexcept : D3D12DescriptorRange() {
    Swap(other);
}

D3D12DescriptorRange::~D3D12DescriptorRange() {
    if (allocator) {
        allocator->Deallocate(*this);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorRange::GetDescriptorHandle(uint32_t offset) const {
    assert(offset < num);
    return { handle.ptr + allocator->IncrementSize() * offset };
}

D3D12DescriptorHeapAllocator::D3D12DescriptorHeapAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t NumDescriptorsPerHeap) :
    device_(device),
    desc_{},
    increment_size_(device->GetDescriptorHandleIncrementSize(type))
{
    desc_.Type = type;
    desc_.NumDescriptors = NumDescriptorsPerHeap;
    desc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // This heap will not be bound to the shader
    desc_.NodeMask = 0;
}

D3D12DescriptorHeapAllocator::~D3D12DescriptorHeapAllocator()
{

}

D3D12DescriptorHeapEntry* D3D12DescriptorHeapAllocator::AllocateHeap()
{
    // Create a new descriptorHeap
    ComPtr<ID3D12DescriptorHeap> heap;
    GfxThrowIfFailed(device_->CreateDescriptorHeap(&desc_, IID_PPV_ARGS(&heap)));
    heap.Get()->SetName(TEXT("D3D12DescriptorHeapEntry Descriptor Heap"));

    // Add an entry covering the entire heap.
    D3D12DescriptorHeapEntry::DescriptorHandle HeapBase = heap->GetCPUDescriptorHandleForHeapStart();
    assert(HeapBase.ptr != 0);

    auto& entry = heaps_.emplace_back();
    entry.heap = heap;
    entry.free_list.push_back({ HeapBase.ptr, HeapBase.ptr + (SIZE_T)desc_.NumDescriptors * increment_size_, desc_.NumDescriptors });

    return &entry;
}

D3D12DescriptorRange D3D12DescriptorHeapAllocator::Allocate(uint32_t num) {
    assert(desc_.NumDescriptors >= num);

    D3D12DescriptorHeapEntry* entry = nullptr;
    D3D12DescriptorHeapEntry::FreeRange* range = nullptr;
    
    for (auto& heap : heaps_) {
        for (auto& free_entry : heap.free_list) {
            if (free_entry.num >= num) {
                entry = &heap;
                range = &free_entry;
            }
        }
    }

    // If all entries are full, create a new one
    if (!entry) {
        entry = AllocateHeap();
    }

    // Allocate a slot
    range = &entry->free_list.back();
    D3D12DescriptorRange slot{ this, entry, { range->begin }, num };

    // Remove this range if all slot has been allocated.
    range->begin += increment_size_ * num;
    range->num -= num;

    if (range->begin == range->end) {
        for (auto it = entry->free_list.begin(); it != entry->free_list.end(); ++it) {
            if (&(*it) == range) {
                entry->free_list.erase(it);
                break;
            }
        }
    }

    return slot;
}

void D3D12DescriptorHeapAllocator::Deallocate(const D3D12DescriptorRange& slot)
{
    //assert(slot.heap_index < heaps_.size());
    if (!slot.heap) return;

    D3D12DescriptorHeapEntry* entry = slot.heap;
    D3D12DescriptorHeapEntry::FreeRange return_range{ slot.handle.ptr, slot.handle.ptr + increment_size_ * slot.num };

    bool found = false;
    for (auto it = entry->free_list.begin(); it != entry->free_list.end() && !found; it++) {
        D3D12DescriptorHeapEntry::FreeRange& range = *it;
        assert(range.begin < range.end);

        if (range.begin == return_range.end) //Merge NewRange and Range
        {
            range.begin = return_range.begin;
            found = true;
        }
        else if (range.end == return_range.begin) // Merge Range and NewRange
        {
            range.end = return_range.end;
            found = true;
        }
        else
        {
            //assert(Range.End < NewRange.Start || Range.Start > NewRange.Start);
            if (range.begin > return_range.begin) // Insert NewRange before Range
            {
                it = entry->free_list.insert(it, return_range);
                found = true;
            }
        }
    }

    if (!found)
    {
        entry->free_list.push_back(return_range);
    }
}

}
}