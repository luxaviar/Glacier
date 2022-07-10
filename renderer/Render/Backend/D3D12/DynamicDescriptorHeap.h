#pragma once

#include <wrl.h>
#include <cstdint>
#include <memory>
#include <queue>
#include "d3dx12.h"
#include "Common/Uncopyable.h"
#include "Program.h"

namespace glacier {
namespace render {

class Device;
class RootSignature;

class DynamicDescriptorHeap : private Uncopyable {
public:
    using DescriptorHeapPool = std::queue<ComPtr<ID3D12DescriptorHeap>>;

    DynamicDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptorsPerHeap = 1024 );

    void StageDescriptors( uint32_t root_index, uint32_t offset, uint32_t num_descriptors,
        const D3D12_CPU_DESCRIPTOR_HANDLE src_handle);

    void StageInlineCBV(uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address);
    void StageInlineSRV(uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address);
    void StageInlineUAV(uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address);

    void CommitStagedDescriptorsForDraw(D3D12CommandBuffer& commandList );
    void CommitStagedDescriptorsForDispatch(D3D12CommandBuffer& commandList );

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(D3D12CommandBuffer& comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor );

    void ParseRootSignature(D3D12Program* program);

    void Reset();

private:
    ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();

    uint32_t ComputeStaleDescriptorCount() const;

    void CommitDescriptorTables(D3D12CommandBuffer& D3D12CommandBuffer,
        std::function<void( ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE )> setFunc );
    void CommitInlineDescriptors(D3D12CommandBuffer& commandList, const D3D12_GPU_VIRTUAL_ADDRESS* bufferLocations, uint32_t& bit_mask,
        std::function<void( ID3D12GraphicsCommandList*, UINT, D3D12_GPU_VIRTUAL_ADDRESS )> setFunc );

    struct DescriptorTableCache {
        void Reset() {
            NumDescriptors = 0;
            BaseDescriptor = nullptr;
        }

        uint32_t NumDescriptors = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor = nullptr;
    };

    ID3D12Device* device_;
    D3D12_DESCRIPTOR_HEAP_TYPE heap_type_;
    uint32_t descriptor_heap_size_;
    uint32_t descriptor_increment_size_;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> program_handle_cache_;
    DescriptorTableCache program_table_cache_[D3D12Program::kMaxRootSlot];

    D3D12_GPU_VIRTUAL_ADDRESS inline_cbv_[D3D12Program::kMaxRootSlot];
    D3D12_GPU_VIRTUAL_ADDRESS inline_srv_[D3D12Program::kMaxRootSlot];
    D3D12_GPU_VIRTUAL_ADDRESS inline_uav_[D3D12Program::kMaxRootSlot];

    uint32_t program_table_bitmask_;

    uint32_t stale_table_bitmask_;
    uint32_t stale_cbv_bitmask_;
    uint32_t stale_srv_bitmask_;
    uint32_t stale_uav_bitmask_;

    DescriptorHeapPool descriptor_heap_pool_;
    DescriptorHeapPool available_descriptor_heaps_;

    ComPtr<ID3D12DescriptorHeap> cur_descriptor_heap_;
    CD3DX12_GPU_DESCRIPTOR_HANDLE cur_gpu_handle_;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cur_cpu_handle_;

    uint32_t num_free_handles_;
};

}
}