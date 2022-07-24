#pragma once

#include <vector>
#include <d3d12.h> 
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include "ResourceStateTracker.h"
#include "DynamicDescriptorHeap.h"
#include "Render/Base/CommandBuffer.h"

namespace glacier {
namespace render {

class D3D12CommandQueue;
struct D3D12DescriptorRange;
class D3D12Program;

class D3D12CommandBuffer : public CommandBuffer {
public:
    D3D12CommandBuffer(GfxDriver* driver, CommandBufferType type);
    virtual ~D3D12CommandBuffer();

    void SetName(const char* Name) override;
    void Reset() override;

    std::shared_ptr<Texture> CreateTextureFromFile(const TCHAR* file, bool srgb, 
        bool gen_mips, TextureType type = TextureType::kTexture2D) override;

    std::shared_ptr<Texture> CreateTextureFromColor(const Color& color, bool srgb) override;

    void GenerateMipMaps(Texture* texture) override;

    void Close() override;
    bool Close(CommandBuffer* pending_cmd_list) override;

    void SetPipelineState(ID3D12PipelineState* pso);
    void SetComputeRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program);
    void SetGraphicsRootSignature(ID3D12RootSignature* root_signature, D3D12Program* program);

    void SetGraphics32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data);
    template<typename T>
    void SetGraphics32BitConstants(uint32_t root_param_index, const T& data) {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
        SetGraphics32BitConstants(root_param_index, sizeof(T) / sizeof(uint32_t), &data);
    }

    void SetCompute32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data);
    template<typename T>
    void SetCompute32BitConstants(uint32_t root_param_index, const T& data) {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
        SetCompute32BitConstants(root_param_index, sizeof(T) / sizeof(uint32_t), &data);
    }

    void SetConstantBufferView(uint32_t root_param_index, const Resource* resource) override;
    void SetShaderResourceView(uint32_t root_param_index, const Resource* resource) override;
    void SetUnorderedAccessView(uint32_t root_param_index, const Resource* resource) override;

    void SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const Resource* resource, bool uav = false) override;
    void SetSamplerTable(uint32_t root_param_index, uint32_t offset, const Resource* resource) override;

    void SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range);
    void SetSamplerTable(uint32_t root_param_index, uint32_t offset, const D3D12DescriptorRange* range);

    void TransitionBarrier(Resource* res, ResourceAccessBit after_state,
        uint32_t subresource = BARRIER_ALL_SUBRESOURCES) override;

    void AliasResource(Resource* before, Resource* after) override;
    void UavResource(Resource* res) override;

    void ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);

    void AliasResource(ID3D12Resource* before, ID3D12Resource* after);
    void UavResource(ID3D12Resource* res);

    void CopyBufferRegion(ID3D12Resource* dst, UINT64 DstOffset, ID3D12Resource* src, UINT64 SrcOffset, UINT64 NumBytes);
    void CopyResource(Resource* src, Resource* dst) override;

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

    void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
        UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override;

    void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
        UINT StartVertexLocation, UINT StartInstanceLocation) override;

    void Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z);

    void BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
    void EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
    void ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries,
        ID3D12Resource* pDestinationBuffer, UINT64 AlignedDestinationBufferOffset);

    void ResolveSubresource(ID3D12Resource* pDstResource, UINT DstSubresource, 
        ID3D12Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);

    ID3D12CommandAllocator* GetCommandAllocator() { return command_allocator_.Get(); }
    ID3D12GraphicsCommandList* GetNativeCommandList() { return command_list_.Get(); }

    void FlushBarriers() override;

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap);
    void BindDescriptorHeaps() override;

    CommandBuffer* GetGenerateMipsCommandList() const { return compute_cmd_buffer_; }

    void AddInflightResource(ResourceLocation&& res);
    void AddInflightResource(D3D12DescriptorRange&& slot);
    void AddInflightResource(std::shared_ptr<Resource>&& res);

private:
    D3D12_COMMAND_LIST_TYPE native_type_;
    ID3D12Device* device_;

    ComPtr<ID3D12CommandAllocator> command_allocator_ = nullptr;
    ComPtr<ID3D12GraphicsCommandList> command_list_ = nullptr;

    ResourceStateTracker resource_state_tracker_;

    std::unique_ptr<DynamicDescriptorHeap> dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    ID3D12DescriptorHeap* descriptor_heaps_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    std::vector<InflightResource> inflight_resources_;
};

}
}
