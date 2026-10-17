#include "CommandBuffer.h"
#include <algorithm>
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

namespace {

// The fallback warnings are a limited sample for whoever migrates a pass: the
// same transition repeats every frame, and after the first ones the count in
// RenderGraph::stats is the report to read. The budget is per run rather than
// per command buffer, since the command buffers are pooled and created lazily.
constexpr uint32_t kMaxGraphFallbackWarnings = 16;
uint32_t g_graph_fallback_warnings = 0;

} // namespace

std::unordered_map<std::string, std::shared_ptr<Texture>> D3D12CommandBuffer::texture_cache_;

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
    //textures are decoded once: several models and materials of one scene ask for
    //the same file, and the instance count of a model does not matter either
    std::string key = ToNarrow(file);
    key += srgb ? "|srgb" : "|linear";
    key += gen_mips ? "|mips" : "|no_mips";
    key += "|" + std::to_string((int)type);

    auto it = texture_cache_.find(key);
    if (it != texture_cache_.end()) {
        LOG_DEBUG("texture '{}': reusing the decoded image", key);
        return it->second;
    }

    const Image image(file, srgb);
    auto tex = std::make_shared<D3D12Texture>(this, image, gen_mips, type, true);
    tex->SetName(ToNarrow(file).c_str());

    texture_cache_.emplace(std::move(key), tex);
    return tex;
}

void D3D12CommandBuffer::ClearTextureCache() {
    texture_cache_.clear();
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
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address;
    if (resource->resource_type() == ResourceType::kTexture) {
        auto res = static_cast<const D3D12Texture*>(resource);
        gpu_address = res->GetGpuAddress();
    }
    else if (resource->resource_type() == ResourceType::kBuffer) {
        auto res = static_cast<const D3D12Buffer*>(resource);
        gpu_address = res->GetGpuAddress();
    }

    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineSRV(root_param_index, gpu_address);
}

void D3D12CommandBuffer::SetUnorderedAccessView(uint32_t root_param_index, const Resource* resource) {
    auto res = static_cast<const D3D12Texture*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineUAV(root_param_index, res->GetGpuAddress());
}

void D3D12CommandBuffer::SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const Resource* resource, bool uav) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    if (resource->resource_type() == ResourceType::kTexture) {
        auto res = static_cast<const D3D12Texture*>(resource);
        handle = uav ? res->GetUavHandle() : res->GetSrvHandle();
    }
    else if (resource->resource_type() == ResourceType::kBuffer) {
        auto res = static_cast<const D3D12Buffer*>(resource);
        handle = uav ? res->GetUavHandle() : res->GetSrvHandle();
    }

    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(root_param_index, offset, 1, handle);
}

void D3D12CommandBuffer::SetSamplerTable(uint32_t root_param_index, uint32_t offset, const Resource* resource) {
    auto res = static_cast<const D3D12Sampler*>(resource);
    dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->StageDescriptors(
        root_param_index, offset, 1, res->GetDescriptorHandle()
    );
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
    //a buffer the CPU writes every frame stays readable for every stage, and
    //the runtime rejects any other state for it
    if (resource->has_fixed_state()) {
        return;
    }

    //Validation of the render graph: while
    //a pass of the graph runs, the states it declared for a resource are what
    //it is allowed to ask for, so asking for one of them costs nothing - the
    //tracker still moves the resource when the declared state is not the one
    //the pass was entered in (a ping-pong inside one node needs both). Anything
    //else changes the state right here, the old way, and is counted - the count
    //is how much of the renderer still transitions on its own.
    if (in_graph_scope_ && graph_resources_.count(resource) != 0) {
        auto it = graph_expected_states_.find(resource);
        if (it != graph_expected_states_.end() && IsDeclaredState(it->second, after_state)) {
            resource_state_tracker_.TransitionResource(resource, (D3D12_RESOURCE_STATES)after_state, subresource);
            return;
        }

        ++graph_fallback_count_;
        if (g_graph_fallback_warnings < kMaxGraphFallbackWarnings) {
            ++g_graph_fallback_warnings;
            if (it == graph_expected_states_.end()) {
                LOG_WARN("render graph: pass '{}' transitions '{}' to {:#x} without declaring it",
                    graph_pass_name_ ? graph_pass_name_ : "?", resource->GetName(), (uint64_t)after_state);
            }
            else {
                LOG_WARN("render graph: pass '{}' transitions '{}' to {:#x}, not the declared {:#x}",
                    graph_pass_name_ ? graph_pass_name_ : "?", resource->GetName(),
                    (uint64_t)after_state, it != graph_expected_states_.end() && !it->second.empty()
                        ? (uint64_t)it->second.front() : 0ull);
            }
        }

        // the undeclared transition is the state the resource is in now; a
        // later request for it is not a second fallback
        graph_expected_states_[resource] = { after_state };
    }

    resource_state_tracker_.TransitionResource(resource, (D3D12_RESOURCE_STATES)after_state, subresource);
}

bool D3D12CommandBuffer::IsDeclaredState(const std::vector<ResourceAccessBit>& states, ResourceAccessBit after_state) {
    for (auto state : states) {
        if (((uint64_t)after_state & (uint64_t)state) == (uint64_t)after_state) {
            return true;
        }
    }

    return false;
}

void D3D12CommandBuffer::BeginGraphScope() {
    in_graph_scope_ = true;
    graph_pass_name_ = nullptr;
    graph_fallback_count_ = 0;
    graph_request_base_ = resource_state_tracker_.transition_requests();
    graph_change_base_ = resource_state_tracker_.changed_transitions();
    graph_skip_base_ = resource_state_tracker_.skipped_transitions();
    graph_resources_.clear();
    graph_expected_states_.clear();
}

void D3D12CommandBuffer::EndGraphScope() {
    graph_request_count_ = (uint32_t)(resource_state_tracker_.transition_requests() - graph_request_base_);
    graph_change_count_ = (uint32_t)(resource_state_tracker_.changed_transitions() - graph_change_base_);
    graph_skip_count_ = (uint32_t)(resource_state_tracker_.skipped_transitions() - graph_skip_base_);

    in_graph_scope_ = false;
    graph_pass_name_ = nullptr;
    graph_resources_.clear();
    graph_expected_states_.clear();
}

void D3D12CommandBuffer::AddGraphResource(Resource* resource) {
    if (resource && !resource->has_fixed_state()) {
        graph_resources_.insert(resource);
    }
}

void D3D12CommandBuffer::BeginGraphPass(const char* name) {
    graph_pass_name_ = name;
}

void D3D12CommandBuffer::EndGraphPass() {
    graph_pass_name_ = nullptr;
    // the next pass declares its own states; a use the pass body made with a
    // state of its own is not carried over
    graph_expected_states_.clear();
}

void D3D12CommandBuffer::SetGraphExpectedState(Resource* resource, ResourceAccessBit state) {
    if (resource) {
        auto& states = graph_expected_states_[resource];
        if (std::find(states.begin(), states.end(), state) == states.end()) {
            states.push_back(state);
        }
    }
}

void D3D12CommandBuffer::GraphTransitionBarrier(Resource* resource, ResourceAccessBit state) {
    if (resource->has_fixed_state()) {
        return;
    }

    resource_state_tracker_.TransitionResource(resource, (D3D12_RESOURCE_STATES)state,
        BARRIER_ALL_SUBRESOURCES);
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

void D3D12CommandBuffer::ClearUAV(Buffer* buffer) {
    auto buf = static_cast<D3D12Buffer*>(buffer);
    ClearUnorderAccessView(buf);
}

void D3D12CommandBuffer::ClearUAV(Texture* texture) {
    auto tex = static_cast<D3D12Texture*>(texture);
    ClearUnorderAccessView(tex);
}

void D3D12CommandBuffer::ClearUnorderAccessView(D3D12Buffer* buffer)
{
    FlushBarriers();

    auto cpu_handle = buffer->GetUavHandle();
    auto gpu_handle = dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CopyDescriptor(*this, cpu_handle);
    auto res = static_cast<ID3D12Resource*>(buffer->GetNativeResource());

    const UINT clear_color[4] = {};
    command_list_->ClearUnorderedAccessViewUint(gpu_handle, cpu_handle, res, clear_color, 0, nullptr);
}

void D3D12CommandBuffer::ClearUnorderAccessView(D3D12Texture* texture)
{
    FlushBarriers();

    auto cpu_handle = texture->GetUavHandle();
    auto gpu_handle = dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CopyDescriptor(*this, cpu_handle);
    auto res = static_cast<ID3D12Resource*>(texture->GetNativeResource());
    CD3DX12_RECT clear_rect(0, 0, (LONG)texture->width(), (LONG)texture->height());

    const float* clear_color = texture->GetClearValue().Color;
    command_list_->ClearUnorderedAccessViewFloat(gpu_handle, cpu_handle, res, clear_color, 1, &clear_rect);
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
