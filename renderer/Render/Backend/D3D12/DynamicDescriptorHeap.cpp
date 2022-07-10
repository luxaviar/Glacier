#include "DynamicDescriptorHeap.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

DynamicDescriptorHeap::DynamicDescriptorHeap(ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptorsPerHeap) :
    device_(device),
    heap_type_(heapType),
    descriptor_heap_size_(numDescriptorsPerHeap),
    program_handle_cache_(descriptor_heap_size_),
    program_table_bitmask_(0),
    stale_table_bitmask_(0),
    stale_cbv_bitmask_(0),
    stale_srv_bitmask_(0), 
    stale_uav_bitmask_(0),
    cur_cpu_handle_(D3D12_DEFAULT),
    cur_gpu_handle_(D3D12_DEFAULT),
    num_free_handles_(0)
{
    descriptor_increment_size_ = device_->GetDescriptorHandleIncrementSize(heapType);
}

void DynamicDescriptorHeap::ParseRootSignature(D3D12Program* program) {
    assert(program);
    stale_table_bitmask_ = 0;

    program_table_bitmask_ = program->GetDescriptorTableBitMask(heap_type_);
    uint32_t table_bitmask = program_table_bitmask_;

    uint32_t offset = 0;
    DWORD    root_index;
    while (_BitScanForward(&root_index, table_bitmask) && root_index < program->GetNumParameters()) {
        uint32_t num_descriptors = program->GetNumDescriptors(root_index);

        auto& table_cache = program_table_cache_[root_index];
        table_cache.NumDescriptors = num_descriptors;
        table_cache.BaseDescriptor = program_handle_cache_.data() + offset;

        offset += num_descriptors;

        table_bitmask ^= ( 1 << root_index );
    }

    // Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
    assert(
        offset <= descriptor_heap_size_ &&
        "The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap." );
}

void DynamicDescriptorHeap::StageDescriptors(uint32_t root_index, uint32_t offset, uint32_t num_descriptors,
    const D3D12_CPU_DESCRIPTOR_HANDLE src_handle )
{
    if (num_descriptors > descriptor_heap_size_ || root_index >= D3D12Program::kMaxRootSlot) {
        throw std::bad_alloc();
    }

    DescriptorTableCache& table_cache = program_table_cache_[root_index];

    if ((offset + num_descriptors) > table_cache.NumDescriptors) {
        throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* dst_handle = (table_cache.BaseDescriptor + offset);
    for (uint32_t i = 0; i < num_descriptors; ++i) {
        dst_handle[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(src_handle, i, descriptor_increment_size_);
    }

    stale_table_bitmask_ |= ( 1 << root_index );
}

void DynamicDescriptorHeap::StageInlineCBV(uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
    assert( root_index < D3D12Program::kMaxRootSlot);

    inline_cbv_[root_index] = address;
    stale_cbv_bitmask_ |= ( 1 << root_index );
}

void DynamicDescriptorHeap::StageInlineSRV(uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
    assert( root_index < D3D12Program::kMaxRootSlot);

    inline_srv_[root_index] = address;
    stale_srv_bitmask_ |= ( 1 << root_index );
}

void DynamicDescriptorHeap::StageInlineUAV( uint32_t root_index, D3D12_GPU_VIRTUAL_ADDRESS address)
{
    assert(root_index < D3D12Program::kMaxRootSlot);

    inline_uav_[root_index] = address;
    stale_uav_bitmask_ |= ( 1 << root_index);
}

uint32_t DynamicDescriptorHeap::ComputeStaleDescriptorCount() const {
    uint32_t count = 0;
    DWORD slot;
    DWORD table_bitmask = stale_table_bitmask_;

    while (_BitScanForward(&slot, table_bitmask)) {
        count += program_table_cache_[slot].NumDescriptors;
        table_bitmask ^= ( 1 << slot );
    }

    return count;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap() {
    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    if (!available_descriptor_heaps_.empty()) {
        descriptor_heap = available_descriptor_heaps_.front();
        available_descriptor_heaps_.pop();
    } else {
        descriptor_heap = CreateDescriptorHeap();
        descriptor_heap_pool_.push(descriptor_heap);
    }

    return descriptor_heap;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = heap_type_;
    desc.NumDescriptors = descriptor_heap_size_;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> heap;
    GfxThrowIfFailed(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

    return heap;
}

void DynamicDescriptorHeap::CommitDescriptorTables(D3D12CommandBuffer& commandList,
    std::function<void( ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE )> setFunc )
{
    uint32_t total_descriptor_num = ComputeStaleDescriptorCount();
    if (total_descriptor_num == 0) return;

    auto cmd_list = commandList.GetNativeCommandList();
    if (!cur_descriptor_heap_ || num_free_handles_ < total_descriptor_num) {
        cur_descriptor_heap_ = RequestDescriptorHeap();
        cur_cpu_handle_ = cur_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
        cur_gpu_handle_ = cur_descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
        num_free_handles_ = descriptor_heap_size_;

        commandList.SetDescriptorHeap(heap_type_, cur_descriptor_heap_.Get());

        // recopy all descriptor tables to the new descriptor heap
        stale_table_bitmask_ = program_table_bitmask_;
    }

    DWORD root_index;
    while (_BitScanForward(&root_index, stale_table_bitmask_)) {
        UINT src_num = program_table_cache_[root_index].NumDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* src_handles = program_table_cache_[root_index].BaseDescriptor;

        D3D12_CPU_DESCRIPTOR_HANDLE dst_range[] = { cur_cpu_handle_ };
        UINT dst_range_num[]  = { src_num };

        // Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
        device_->CopyDescriptors(1, dst_range, dst_range_num, src_num,
                                        src_handles, nullptr, heap_type_);

        // Set the descriptors on the command list using the passed-in setter function.
        setFunc(cmd_list, root_index, cur_gpu_handle_);

        // Offset current CPU and GPU descriptor handles.
        cur_cpu_handle_.Offset(src_num, descriptor_increment_size_);
        cur_gpu_handle_.Offset(src_num, descriptor_increment_size_);
        num_free_handles_ -= src_num;

        // Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new
        // descriptor.
        stale_table_bitmask_ ^= ( 1 << root_index );
    }
}

void DynamicDescriptorHeap::CommitInlineDescriptors(D3D12CommandBuffer& commandList,
    const D3D12_GPU_VIRTUAL_ADDRESS* bufferLocations, uint32_t& bit_mask,
    std::function<void( ID3D12GraphicsCommandList*, UINT, D3D12_GPU_VIRTUAL_ADDRESS )> setFunc )
{
    if (bit_mask != 0) {
        auto  cmd_list = commandList.GetNativeCommandList();
        DWORD root_index;
        while (_BitScanForward(&root_index, bit_mask))
        {
            setFunc(cmd_list, root_index, bufferLocations[root_index]);

            // Flip the stale bit so the descriptor is not recopied again unless it is updated with a new descriptor.
            bit_mask ^= ( 1 << root_index );
        }
    }
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(D3D12CommandBuffer& commandList )
{
    CommitDescriptorTables( commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable );
    CommitInlineDescriptors( commandList, inline_cbv_, stale_cbv_bitmask_,
                             &ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView );
    CommitInlineDescriptors( commandList, inline_srv_, stale_srv_bitmask_,
                             &ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView );
    CommitInlineDescriptors( commandList, inline_uav_, stale_uav_bitmask_,
                             &ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView );
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(D3D12CommandBuffer& commandList )
{
    CommitDescriptorTables( commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable );
    CommitInlineDescriptors( commandList, inline_cbv_, stale_cbv_bitmask_,
                             &ID3D12GraphicsCommandList::SetComputeRootConstantBufferView );
    CommitInlineDescriptors( commandList, inline_srv_, stale_srv_bitmask_,
                             &ID3D12GraphicsCommandList::SetComputeRootShaderResourceView );
    CommitInlineDescriptors( commandList, inline_uav_, stale_uav_bitmask_,
                             &ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView );
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(D3D12CommandBuffer& comandList,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
    if (!cur_descriptor_heap_ || num_free_handles_ < 1) {
        cur_descriptor_heap_ = RequestDescriptorHeap();
        cur_cpu_handle_ = cur_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
        cur_gpu_handle_ = cur_descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
        num_free_handles_ = descriptor_heap_size_;

        comandList.SetDescriptorHeap( heap_type_, cur_descriptor_heap_.Get() );

        stale_table_bitmask_ = program_table_bitmask_;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE hGPU = cur_gpu_handle_;
    device_->CopyDescriptorsSimple(1, cur_cpu_handle_, cpuDescriptor, heap_type_);

    cur_cpu_handle_.Offset(1, descriptor_increment_size_);
    cur_gpu_handle_.Offset(1, descriptor_increment_size_);
    num_free_handles_ -= 1;

    return hGPU;
}

void DynamicDescriptorHeap::Reset() {
    available_descriptor_heaps_ = descriptor_heap_pool_;
    cur_descriptor_heap_.Reset();
    cur_cpu_handle_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    cur_gpu_handle_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    num_free_handles_ = 0;
    program_table_bitmask_ = 0;
    stale_table_bitmask_ = 0;
    stale_cbv_bitmask_ = 0;
    stale_srv_bitmask_ = 0;
    stale_uav_bitmask_ = 0;

    // Reset the descriptor cache
    for (int i = 0; i < D3D12Program::kMaxRootSlot; ++i) {
        program_table_cache_[i].Reset();
        inline_cbv_[i] = 0ull;
        inline_srv_[i] = 0ull;
        inline_uav_[i] = 0ull;
    }
}

}
}
