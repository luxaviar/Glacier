#pragma once

#include <vector>
#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <atomic>              // For std::atomic_bool
#include <condition_variable>  // For std::condition_variable.
#include <cstdint>             // For uint64_t
#include "ResourceStateTracker.h"

namespace glacier {
namespace render {

class D3D12CommandQueue;
class D3D12Resource;
struct D3D12DescriptorRange;

class D3D12CommandList {
public:
    D3D12CommandList(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~D3D12CommandList();

    HRESULT SetName(const TCHAR* Name);

    void Reset();
    void ResetAllocator();

    bool IsClosed() const { return closed_; }
    void Close();
    bool Close(D3D12CommandList* pending_cmd_list);

    void SetComputeRootSignature(ID3D12RootSignature* rs);
    void SetPipelineState(ID3D12PipelineState* ps);
    void SetComputeRoot32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value,
        const void* data, uint32_t offset);
    void SetComputeRootDescriptorTable(uint32_t root_param_index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor);

    void ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);

    //void TransitionBarrier(ID3D12Resource* res, D3D12_RESOURCE_STATES before_state,
    //    D3D12_RESOURCE_STATES after_state);

    void TransitionBarrier(D3D12Resource* res, D3D12_RESOURCE_STATES after_state,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void TransitionBarrier(const std::shared_ptr<D3D12Resource>& res, D3D12_RESOURCE_STATES after_state,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

    void AliasResource(ID3D12Resource* before, ID3D12Resource* after);
    void UavResource(ID3D12Resource* res);

    void CopyBufferRegion(ID3D12Resource* dst, UINT64 DstOffset, ID3D12Resource* src, UINT64 SrcOffset, UINT64 NumBytes);

    void CopyResource(D3D12Resource* src, D3D12Resource* dst);

    void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, 
        const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox);

    void ClearRenderTargetView(D3D12DescriptorRange& RenderTargetView, const FLOAT ColorRGBA[4],
        UINT NumRects, const D3D12_RECT* pRects);

    void ClearDepthStencilView(D3D12DescriptorRange& DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
        UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects);

    void OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
        BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor);

    void RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports);
    void RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects);
    void IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews);
    void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView);

    void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
        INT BaseVertexLocation, UINT StartInstanceLocation);
    void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);

    void SetGraphicsRootConstantBufferView(UINT RootParameterIndex, const D3D12Resource* BufferLocation);
    void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

    void Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z);

    void BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
    void EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
    void ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries,
        ID3D12Resource* pDestinationBuffer, UINT64 AlignedDestinationBufferOffset);

    void ResolveSubresource(ID3D12Resource* pDstResource, UINT DstSubresource, 
        ID3D12Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);

    ID3D12CommandAllocator* GetCommandAllocator() { return command_allocator_.Get(); }
    ID3D12GraphicsCommandList* GetUnderlyingCommandList() { return command_list_.Get(); }

    void FlushBarriers();
private:

    bool closed_ = true;

    D3D12_COMMAND_LIST_TYPE type_;
    ID3D12Device* device_;

    //std::vector<D3D12_RESOURCE_BARRIER> resource_barriers_;

    ComPtr<ID3D12CommandAllocator> command_allocator_ = nullptr;
    ComPtr<ID3D12GraphicsCommandList> command_list_ = nullptr;

    ResourceStateTracker resource_state_tracker_;
};

}
}
