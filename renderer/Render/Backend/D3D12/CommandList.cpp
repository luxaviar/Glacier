#include "CommandList.h"
#include "GfxDriver.h"
#include "Exception/Exception.h"
#include "Common/Util.h"
#include "CommandQueue.h"
#include "Resource.h"
#include "DescriptorHeapAllocator.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

D3D12CommandList::D3D12CommandList(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) :
    closed_(true),
    type_(type),
    device_(device)
{
    GfxThrowIfFailed(device_->CreateCommandAllocator(type, IID_PPV_ARGS(command_allocator_.GetAddressOf())));
    GfxThrowIfFailed(device_->CreateCommandList(0, type, command_allocator_.Get(), nullptr,
        IID_PPV_ARGS(command_list_.GetAddressOf())));

    // Start off in a closed state. 
    // This is because the first time we refer to the command list we will Reset it,
    // and it needs to be closed before calling Reset.
    GfxThrowIfFailed(command_list_->Close());
}

D3D12CommandList::~D3D12CommandList() {
}

// Command list allocators can only be reset when the associated command lists have finished execution on the GPU.
// So we should use fences to determine GPU execution progress(see FlushCommandQueue()).
void D3D12CommandList::ResetAllocator() {
    GfxThrowIfFailed(command_allocator_->Reset());
}

// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
// Before an app calls Reset, the command list must be in the "closed" state. 
// After Reset succeeds, the command list is left in the "recording" state. 
void D3D12CommandList::Reset() {
    assert(closed_);

    GfxThrowIfFailed(command_list_->Reset(command_allocator_.Get(), nullptr));
    closed_ = false;
}

HRESULT D3D12CommandList::Close() {
    assert(!closed_);

    FlushBarriers();

    auto ok = command_list_->Close();
    closed_ = true;
    return ok;
}

void D3D12CommandList::SetComputeRootSignature(ID3D12RootSignature* rs) {
    command_list_->SetComputeRootSignature(rs);
}

void D3D12CommandList::SetPipelineState(ID3D12PipelineState* ps) {
    command_list_->SetPipelineState(ps);
}

void D3D12CommandList::SetComputeRoot32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value,
    const void* data, uint32_t offset)
{
    command_list_->SetComputeRoot32BitConstants(root_param_index, num_32bit_value, data, offset);
}

void D3D12CommandList::SetComputeRootDescriptorTable(uint32_t root_param_index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor)
{
    command_list_->SetComputeRootDescriptorTable(root_param_index, base_descriptor);
}

void D3D12CommandList::FlushBarriers() {
    UINT num = static_cast<UINT>(resource_barriers_.size());
    if (num > 0) {
        command_list_->ResourceBarrier(num, resource_barriers_.data());
        resource_barriers_.clear();
    }
}

void D3D12CommandList::TransitionBarrier(ID3D12Resource* res,
    D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state, bool flush)
{
    if (before_state == after_state)
        return;

    for (size_t i = resource_barriers_.size(); i > 0; --i) {
        auto& barrier = resource_barriers_[i - 1];
        if (barrier.Transition.pResource == res) {
            if (barrier.Transition.StateBefore == after_state && barrier.Transition.StateAfter == before_state) {
                resource_barriers_.erase(resource_barriers_.begin() + i - 1);
                return;
            }
            break;
        }
    }

    // Create barrier descriptor
    D3D12_RESOURCE_BARRIER barrier;
    ZeroMemory(&barrier, sizeof(D3D12_RESOURCE_BARRIER));

    // Describe barrier
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = res;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before_state;
    barrier.Transition.StateAfter = after_state;

    // Queue barrier
    //command_list_->ResourceBarrier(1, &barrier);
    resource_barriers_.push_back(barrier);

    if (flush) {
        FlushBarriers();
    }
}

void D3D12CommandList::AliasResource(ID3D12Resource* before, ID3D12Resource* after, bool flush) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(before, after);
    //command_list_->ResourceBarrier(1, &barrier);

    resource_barriers_.push_back(barrier);

    if (flush) {
        FlushBarriers();
    }
}

void D3D12CommandList::UavResource(ID3D12Resource* res, bool flush) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(res);
    //command_list_->ResourceBarrier(1, &barrier);

    resource_barriers_.push_back(barrier);

    if (flush) {
        FlushBarriers();
    }
}

void D3D12CommandList::CopyBufferRegion(D3D12Resource& dst, UINT64 DstOffset, D3D12Resource& src, UINT64 SrcOffset, UINT64 NumBytes) {
    FlushBarriers();

    command_list_->CopyBufferRegion(dst.GetUnderlyingResource().Get(), DstOffset, src.GetUnderlyingResource().Get(), SrcOffset, NumBytes);
}

void D3D12CommandList::CopyResource(ID3D12Resource* src, D3D12_RESOURCE_STATES src_state,
    ID3D12Resource* dst, D3D12_RESOURCE_STATES dst_state)
{
    TransitionBarrier(dst, dst_state, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionBarrier(src, src_state, D3D12_RESOURCE_STATE_COPY_SOURCE);

    FlushBarriers();

    command_list_->CopyResource(dst, src);

    TransitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST, dst_state);
    TransitionBarrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE, src_state);
}

void D3D12CommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
    FlushBarriers();

    command_list_->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}

void D3D12CommandList::ClearRenderTargetView(D3D12DescriptorRange& RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT* pRects)
{
    command_list_->ClearRenderTargetView(RenderTargetView.GetDescriptorHandle(), ColorRGBA, NumRects, pRects);
}

void D3D12CommandList::ClearDepthStencilView(D3D12DescriptorRange& DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
    command_list_->ClearDepthStencilView(DepthStencilView.GetDescriptorHandle(), ClearFlags, Depth, Stencil, NumRects, pRects);
}

void D3D12CommandList::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor)
{
    command_list_->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

void D3D12CommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports) {
    command_list_->RSSetViewports(NumViewports, pViewports);
}

void D3D12CommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects) {
    command_list_->RSSetScissorRects(NumRects, pRects);
}

void D3D12CommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) {
    command_list_->IASetVertexBuffers(StartSlot, NumViews, pViews);
}

void D3D12CommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) {
    command_list_->IASetIndexBuffer(pView);
}

void D3D12CommandList::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
    FlushBarriers();

    command_list_->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void D3D12CommandList::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
    FlushBarriers();

    command_list_->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void D3D12CommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, const D3D12Resource* BufferLocation) {
    command_list_->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation->GetGpuAddress());
}

void D3D12CommandList::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    command_list_->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}

void D3D12CommandList::Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z) {
    FlushBarriers();

    command_list_->Dispatch(thread_group_x, thread_group_y, thread_group_z);
}

void D3D12CommandList::BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    command_list_->BeginQuery(pQueryHeap, Type, Index);
}

void D3D12CommandList::EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    command_list_->EndQuery(pQueryHeap, Type, Index);
}

void D3D12CommandList::ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries,
    ID3D12Resource* pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
    command_list_->ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
}

void D3D12CommandList::ResolveSubresource(ID3D12Resource* pDstResource, UINT DstSubresource,
    ID3D12Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
    FlushBarriers();

    command_list_->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

}
}
