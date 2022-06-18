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

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i] = std::make_unique<DynamicDescriptorHeap>(device_, (D3D12_DESCRIPTOR_HEAP_TYPE)i, 1024);
        descriptor_heaps_[i] = nullptr;
    }
}

D3D12CommandList::~D3D12CommandList() {
}

HRESULT D3D12CommandList::SetName(const TCHAR* Name) {
    return command_list_->SetName(Name);
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
    resource_state_tracker_.Reset();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->Reset();
        descriptor_heaps_[i] = nullptr;
    }

    closed_ = false;
}

void D3D12CommandList::Close() {
    assert(!closed_);

    FlushBarriers();

    GfxThrowIfFailed(command_list_->Close());

    //auto ok = ;
    closed_ = true;
    //return ok;
}

bool D3D12CommandList::Close(D3D12CommandList* pending_cmd_list) {
    assert(!closed_);

    FlushBarriers();
    GfxThrowIfFailed(command_list_->Close());
    closed_ = true;

    auto pending_barriers = resource_state_tracker_.FlushPendingResourceBarriers(pending_cmd_list);
    resource_state_tracker_.CommitFinalStates();

    return pending_barriers > 0;
}


void D3D12CommandList::SetPipelineState(ID3D12PipelineState* pso) {
    if (pso != pso_) {
        command_list_->SetPipelineState(pso);
    }
}

void D3D12CommandList::SetComputeRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program) {
    if (root_signature == root_signature_) return;

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i]->ParseRootSignature(program);
    }

    command_list_->SetComputeRootSignature(root_signature);
}

void D3D12CommandList::SetGraphicsRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program) {
    if (root_signature == root_signature_) return;

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i]->ParseRootSignature(program);
    }

    command_list_->SetGraphicsRootSignature(root_signature);
}

void D3D12CommandList::SetCompute32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data)
{
    command_list_->SetComputeRoot32BitConstants(root_param_index, num_32bit_value, data, 0);
}

void D3D12CommandList::SetGraphics32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data)
{
    command_list_->SetGraphicsRoot32BitConstants(root_param_index, num_32bit_value, data, 0);
}

void D3D12CommandList::SetConstantBufferView(uint32_t root_param_index, const D3D12Resource* res) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineCBV(root_param_index, res->GetGpuAddress());
}

void D3D12CommandList::SetShaderResourceView(uint32_t root_param_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineSRV(root_param_index, address);
}

void D3D12CommandList::SetUnorderedAccessView(uint32_t root_param_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineUAV(root_param_index, address);
}

void D3D12CommandList::SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const D3D12Resource* res) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->
        StageDescriptors(root_param_index, offset, 1, res->GetDescriptorHandle());
}

void D3D12CommandList::SetSamplerTable(uint32_t root_param_index, uint32_t offset, const D3D12Resource* res) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->
        StageDescriptors(root_param_index, offset, 1, res->GetDescriptorHandle());
}

void D3D12CommandList::SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->
        StageDescriptors(root_param_index, offset, range->num, range->GetDescriptorHandle());
}

void D3D12CommandList::SetSamplerTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->
        StageDescriptors(root_param_index, offset, range->num, range->GetDescriptorHandle());
}

void D3D12CommandList::FlushBarriers() {
    resource_state_tracker_.FlushResourceBarriers(this);
}

void D3D12CommandList::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) {
    command_list_->ResourceBarrier(NumBarriers, pBarriers);
}

void D3D12CommandList::TransitionBarrier(D3D12Resource* res, D3D12_RESOURCE_STATES after_state, UINT subresource) {
    resource_state_tracker_.TransitionResource(res, after_state, subresource);
}

void D3D12CommandList::TransitionBarrier(const std::shared_ptr<D3D12Resource>& res,
    D3D12_RESOURCE_STATES after_state, UINT subresource)
{
    resource_state_tracker_.TransitionResource(res.get(), after_state, subresource);
}

void D3D12CommandList::AliasResource(ID3D12Resource* before, ID3D12Resource* after) {
    resource_state_tracker_.AliasBarrier(before, after);
}

void D3D12CommandList::UavResource(ID3D12Resource* res) {
    resource_state_tracker_.UAVBarrier(res);
}

void D3D12CommandList::CopyBufferRegion(ID3D12Resource* dst, UINT64 DstOffset, ID3D12Resource* src, UINT64 SrcOffset, UINT64 NumBytes) {
    FlushBarriers();

    command_list_->CopyBufferRegion(dst, DstOffset, src, SrcOffset, NumBytes);
}

void D3D12CommandList::CopyResource(D3D12Resource* src, D3D12Resource* dst)
{
    TransitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionBarrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE);

    FlushBarriers();

    command_list_->CopyResource(dst->GetUnderlyingResource().Get(), src->GetUnderlyingResource().Get());
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
    FlushBarriers();

    command_list_->ClearRenderTargetView(RenderTargetView.GetDescriptorHandle(), ColorRGBA, NumRects, pRects);
}

void D3D12CommandList::ClearDepthStencilView(D3D12DescriptorRange& DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
    FlushBarriers();
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
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
    }

    command_list_->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void D3D12CommandList::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
    FlushBarriers();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
    }

    command_list_->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void D3D12CommandList::Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z) {
    FlushBarriers();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDispatch(*this);
    }

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


void D3D12CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap) {
    if (descriptor_heaps_[type] != heap) {
        descriptor_heaps_[type] = heap;
        BindDescriptorHeaps();
    }
}

void D3D12CommandList::BindDescriptorHeaps() {
    UINT numDescriptorHeaps = 0;
    ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        ID3D12DescriptorHeap* descriptorHeap = descriptor_heaps_[i];
        if (descriptorHeap) {
            descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
        }
    }

    command_list_->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}


}
}
