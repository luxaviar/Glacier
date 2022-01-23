#include "DescriptorTableHeap.h"
#include "Exception/Exception.h"
#include <cassert>

namespace glacier {
namespace render {

D3D12DescriptorTableHeap::D3D12DescriptorTableHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size) :
    type_(type),
    size_(size),
    device_(device)
{
    // Create the descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
    SrvHeapDesc.NumDescriptors = size;
    SrvHeapDesc.Type = type;
    SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    GfxThrowIfFailed(device->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&heap_)));
    heap_->SetName(TEXT("D3D12DescriptorTable"));

    increment_size_ = device->GetDescriptorHandleIncrementSize(type);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorTableHeap::AppendDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE* descriptors, uint32_t count)
{
    assert(count + offset_ < size_);

    auto CpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(heap_->GetCPUDescriptorHandleForHeapStart(), offset_, increment_size_);
    device_->CopyDescriptors(1, &CpuDescriptorHandle, &count, count, descriptors, nullptr, type_);

    // Get GpuDescriptorHandle
    auto GpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(heap_->GetGPUDescriptorHandleForHeapStart(), offset_, increment_size_);

    // Increase descriptor offset
    offset_ += count;

    return GpuDescriptorHandle;
}

void D3D12DescriptorTableHeap::Reset() {
    offset_ = 0;
}

}
}