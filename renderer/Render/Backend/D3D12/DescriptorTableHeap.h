#pragma once

#include <d3d12.h>
#include <vector>
#include "d3dx12.h"
#include "Algorithm/BuddyAllocator.h"
#include "Common/Uncopyable.h"
#include "Common/BitUtil.h"
#include "Common/Util.h"
#include "Exception/Exception.h"
#include "Resource.h"

namespace glacier {
namespace render {

class D3D12DescriptorTableHeap : private Uncopyable {
public:
    D3D12DescriptorTableHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size = 2048);

    ID3D12DescriptorHeap* GetUnderlyingHeap() const { return heap_.Get(); }

    CD3DX12_GPU_DESCRIPTOR_HANDLE AppendDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE* descriptors, uint32_t count);
    void Reset();

    uint32_t IncrementSize() const { return increment_size_; }

private:
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    uint32_t increment_size_;
    uint32_t offset_ = 0;
    uint32_t size_;

    ID3D12Device* device_ = nullptr;
    ComPtr<ID3D12DescriptorHeap> heap_;
};

}
}