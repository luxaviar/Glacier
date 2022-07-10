#include "CommandBuffer.h"
#include "GfxDriver.h"
#include "Exception/Exception.h"
#include "Common/Util.h"
#include "CommandQueue.h"
#include "Resource.h"
#include "DescriptorHeapAllocator.h"
#include "Common/Log.h"
#include "Render/Image.h"
#include "Buffer.h"
#include "Texture.h"
#include "Sampler.h"

namespace glacier {
namespace render {

D3D12CommandBuffer::D3D12CommandBuffer(GfxDriver* driver, CommandBufferType type) :
    CommandBuffer(driver, type)
{
    auto d3d12_driver = static_cast<D3D12GfxDriver*>(driver);
    device_ = d3d12_driver->GetDevice();
    native_type_ = GetNativeCommandBufferType(type);

    GfxThrowIfFailed(device_->CreateCommandAllocator(native_type_, IID_PPV_ARGS(command_allocator_.GetAddressOf())));
    GfxThrowIfFailed(device_->CreateCommandList(0, native_type_, command_allocator_.Get(), nullptr,
        IID_PPV_ARGS(command_list_.GetAddressOf())));

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i] = std::make_unique<DynamicDescriptorHeap>(device_, (D3D12_DESCRIPTOR_HEAP_TYPE)i, 1024);
        descriptor_heaps_[i] = nullptr;
    }
}

D3D12CommandBuffer::~D3D12CommandBuffer() {
}

void D3D12CommandBuffer::SetName(const char* name) {
    command_list_->SetName(ToWide(name).c_str());
}

std::shared_ptr<Texture> D3D12CommandBuffer::CreateTextureFromFile(const TCHAR* file, bool srgb, 
    bool gen_mips, TextureType type)
{
    const Image image(file, srgb);
    auto tex = std::make_shared<D3D12Texture>(this, image, gen_mips, type);
    tex->SetName(ToNarrow(file).c_str());

    return tex;
}

std::shared_ptr<Texture> D3D12CommandBuffer::CreateTextureFromColor(const Color& color, bool srgb) {
    auto dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorRGBA32 col = color;
    Image image(8, 8, srgb);
    image.Clear(col);

    return std::make_shared<D3D12Texture>(this, image, false);
}

void D3D12CommandBuffer::GenerateMipMaps(Texture* texture) {
    auto gfx = D3D12GfxDriver::Instance();
    auto d3d12_tex = dynamic_cast<D3D12Texture*>(texture);

    if (type_ == CommandBufferType::kCopy) {
        if (!compute_cmd_buffer_) {
            compute_cmd_buffer_ = gfx->GetCommandBuffer(CommandBufferType::kCompute);
        }

        compute_cmd_buffer_->GenerateMipMaps(texture);
    }
    else {
        gfx->GenerateMipMaps(this, d3d12_tex);
    }
}

// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
// Before an app calls Reset, the command list must be in the "closed" state. 
// After Reset succeeds, the command list is left in the "recording" state. 
void D3D12CommandBuffer::Reset() {
    assert(closed_);

    material_ = nullptr;
    program_ = nullptr;

    GfxThrowIfFailed(command_allocator_->Reset());
    GfxThrowIfFailed(command_list_->Reset(command_allocator_.Get(), nullptr));
    resource_state_tracker_.Reset();
    inflight_resources_.clear();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i]->Reset();
        descriptor_heaps_[i] = nullptr;
    }

    closed_ = false;
}

void D3D12CommandBuffer::Close() {
    assert(!closed_);

    FlushBarriers();

    GfxThrowIfFailed(command_list_->Close());

    closed_ = true;
}

bool D3D12CommandBuffer::Close(CommandBuffer* pending_cmd_list) {
    assert(!closed_);

    FlushBarriers();
    GfxThrowIfFailed(command_list_->Close());
    closed_ = true;

    auto native_cmd_list = static_cast<D3D12CommandBuffer*>(pending_cmd_list);
    auto pending_barriers = resource_state_tracker_.FlushPendingResourceBarriers(native_cmd_list);
    resource_state_tracker_.CommitFinalStates();

    return pending_barriers > 0;
}

void D3D12CommandBuffer::SetPipelineState(ID3D12PipelineState* pso) {
    command_list_->SetPipelineState(pso);
}

void D3D12CommandBuffer::SetComputeRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program) {
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i]->ParseRootSignature(program);
    }

    command_list_->SetComputeRootSignature(root_signature);
}

void D3D12CommandBuffer::SetGraphicsRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program) {
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        dynamic_descriptor_heap_[i]->ParseRootSignature(program);
    }

    command_list_->SetGraphicsRootSignature(root_signature);
}

void D3D12CommandBuffer::SetCompute32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data)
{
    command_list_->SetComputeRoot32BitConstants(root_param_index, num_32bit_value, data, 0);
}

void D3D12CommandBuffer::SetGraphics32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data)
{
    command_list_->SetGraphicsRoot32BitConstants(root_param_index, num_32bit_value, data, 0);
}

void D3D12CommandBuffer::SetConstantBufferView(uint32_t root_param_index, const Resource* resource) {
    auto buffer = static_cast<const D3D12Buffer*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineCBV(root_param_index, buffer->GetGpuAddress());
}

void D3D12CommandBuffer::SetShaderResourceView(uint32_t root_param_index, const Resource* resource) {
    auto res = static_cast<const D3D12Texture*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineSRV(root_param_index, res->GetGpuAddress());
}

void D3D12CommandBuffer::SetUnorderedAccessView(uint32_t root_param_index, const Resource* resource) {
    auto res = static_cast<const D3D12Texture*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineUAV(root_param_index, res->GetGpuAddress());
}

void D3D12CommandBuffer::SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const Resource* resource) {
    auto res = static_cast<const D3D12Texture*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->
        StageDescriptors(root_param_index, offset, 1, res->GetDescriptorHandle());
}

void D3D12CommandBuffer::SetSamplerTable(uint32_t root_param_index, uint32_t offset, const Resource* resource) {
    auto res = static_cast<const D3D12Sampler*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->
        StageDescriptors(root_param_index, offset, 1, res->GetDescriptorHandle());
}

void D3D12CommandBuffer::SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->
        StageDescriptors(root_param_index, offset, range->num, range->GetDescriptorHandle());
}

void D3D12CommandBuffer::SetSamplerTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range) {
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->
        StageDescriptors(root_param_index, offset, range->num, range->GetDescriptorHandle());
}

void D3D12CommandBuffer::FlushBarriers() {
    resource_state_tracker_.FlushResourceBarriers(this);
}

void D3D12CommandBuffer::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) {
    command_list_->ResourceBarrier(NumBarriers, pBarriers);
}

void D3D12CommandBuffer::AliasResource(Resource* before, Resource* after) {
    auto src = before ? static_cast<ID3D12Resource*>(before->GetNativeResource()) : nullptr;
    auto dst = after ? static_cast<ID3D12Resource*>(after->GetNativeResource()) : nullptr;

    AliasResource(src, dst);
}

void D3D12CommandBuffer::UavResource(Resource* resource) {
    auto res = static_cast<ID3D12Resource*>(resource->GetNativeResource());
    UavResource(res);
}

void D3D12CommandBuffer::TransitionBarrier(Resource* resource, ResourceAccessBit after_state, uint32_t subresource) {
    resource_state_tracker_.TransitionResource(resource, (D3D12_RESOURCE_STATES)after_state, subresource);
}

void D3D12CommandBuffer::AliasResource(ID3D12Resource* before, ID3D12Resource* after) {
    resource_state_tracker_.AliasBarrier(before, after);
}

void D3D12CommandBuffer::UavResource(ID3D12Resource* res) {
    resource_state_tracker_.UAVBarrier(res);
}

void D3D12CommandBuffer::CopyBufferRegion(ID3D12Resource* dst, UINT64 DstOffset, ID3D12Resource* src, UINT64 SrcOffset, UINT64 NumBytes) {
    FlushBarriers();

    command_list_->CopyBufferRegion(dst, DstOffset, src, SrcOffset, NumBytes);
}

void D3D12CommandBuffer::CopyResource(Resource* src, Resource* dst)
{
    TransitionBarrier(dst, ResourceAccessBit::kCopyDest);
    TransitionBarrier(src, ResourceAccessBit::kCopySource);

    FlushBarriers();

    auto src_res = static_cast<ID3D12Resource*>(src->GetNativeResource());
    auto dst_res = static_cast<ID3D12Resource*>(dst->GetNativeResource());
    command_list_->CopyResource(dst_res, src_res);
}

void D3D12CommandBuffer::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
    FlushBarriers();

    command_list_->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}

void D3D12CommandBuffer::ClearRenderTargetView(D3D12DescriptorRange& RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT* pRects)
{
    FlushBarriers();

    command_list_->ClearRenderTargetView(RenderTargetView.GetDescriptorHandle(), ColorRGBA, NumRects, pRects);
}

void D3D12CommandBuffer::ClearDepthStencilView(D3D12DescriptorRange& DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
    FlushBarriers();
    command_list_->ClearDepthStencilView(DepthStencilView.GetDescriptorHandle(), ClearFlags, Depth, Stencil, NumRects, pRects);
}

void D3D12CommandBuffer::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor)
{
    command_list_->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

void D3D12CommandBuffer::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports) {
    command_list_->RSSetViewports(NumViewports, pViewports);
}

void D3D12CommandBuffer::RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects) {
    command_list_->RSSetScissorRects(NumRects, pRects);
}

void D3D12CommandBuffer::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) {
    command_list_->IASetVertexBuffers(StartSlot, NumViews, pViews);
}

void D3D12CommandBuffer::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) {
    command_list_->IASetIndexBuffer(pView);
}

void D3D12CommandBuffer::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
    FlushBarriers();
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
    }

    command_list_->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void D3D12CommandBuffer::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
    FlushBarriers();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
    }

    command_list_->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void D3D12CommandBuffer::Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z) {
    FlushBarriers();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDispatch(*this);
    }

    command_list_->Dispatch(thread_group_x, thread_group_y, thread_group_z);
}

void D3D12CommandBuffer::BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    command_list_->BeginQuery(pQueryHeap, Type, Index);
}

void D3D12CommandBuffer::EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    command_list_->EndQuery(pQueryHeap, Type, Index);
}

void D3D12CommandBuffer::ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries,
    ID3D12Resource* pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
    command_list_->ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
}

void D3D12CommandBuffer::ResolveSubresource(ID3D12Resource* pDstResource, UINT DstSubresource,
    ID3D12Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
    FlushBarriers();

    command_list_->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}


void D3D12CommandBuffer::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap) {
    if (descriptor_heaps_[type] != heap) {
        descriptor_heaps_[type] = heap;
        BindDescriptorHeaps();
    }
}

void D3D12CommandBuffer::BindDescriptorHeaps() {
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

void D3D12CommandBuffer::AddInflightResource(ResourceLocation&& res) {
    inflight_resources_.emplace_back(std::move(res));
}

void D3D12CommandBuffer::AddInflightResource(D3D12DescriptorRange&& slot) {
    inflight_resources_.emplace_back(std::move(slot));
}

void D3D12CommandBuffer::AddInflightResource(std::shared_ptr<Resource>&& res) {
    inflight_resources_.emplace_back(std::move(res));
}

}
}
