#pragma once

#include <queue>
#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <queue>
#include "DescriptorHeapAllocator.h"
#include "MemoryHeapAllocator.h"
#include "LinearAllocator.h"
#include "Common/Singleton.h"
#include "CommandQueue.h"
#include "SwapChain.h"
#include "DescriptorTableHeap.h"
#include "Render/Base/GfxDriver.h"
#include "Resource.h"
#include "Texture.h"

namespace glacier {
namespace render {

class D3D12RenderTarget;
class MipsGenerator;

class D3D12GfxDriver : public GfxDriver, public Singleton<D3D12GfxDriver> {
public:
    D3D12GfxDriver();
    ~D3D12GfxDriver();

    void Init(HWND hWnd, int width, int height, TextureFormat format) override;
    void OnDestroy() override;

    CommandQueue* GetCommandQueue(CommandBufferType type) override;
    CommandBuffer* GetCommandBuffer(CommandBufferType type) override;

    D3D12UploadBufferAllocator* GetUploadBufferAllocator() const { return upload_allocator_.get(); }
    D3D12DefaultBufferAllocator* GetDefaultBufferAllocator() const { return default_allocator_.get(); }
    D3D12TextureResourceAllocator* GetTextureResourceAllocator() const { return texture_allocator_.get(); }
    D3D12ReadbackBufferAllocator* GetReadbackBufferAllocator() const { return readback_allocator_.get(); }

    LinearAllocator* GetLinearAllocator() const { return linear_allocator_.get(); }

    D3D12DescriptorHeapAllocator* GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE HeapType) const;

    D3D12CommandBuffer* GetCommandList(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;
    D3D12CommandQueue* GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    DXGI_SAMPLE_DESC GetMultisampleQualityLevels(DXGI_FORMAT format, 
        UINT numSamples = D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT,
        D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE) const;

    void EnqueueReadback(D3D12Texture::ReadbackTask&& task);

    ID3D12Device2* GetDevice() const { return device_.Get(); }
    SwapChain* GetSwapChain() const { return swap_chain_.get(); }

    void GenerateMipMaps(D3D12CommandBuffer* cmd_buffer, D3D12Texture* texture);

    void BeginFrame() override;
    void EndFrame() override;

    void Present(CommandBuffer* cmd_buffer) override;

    void CheckMSAA(uint32_t sample_count, uint32_t& smaple_count, uint32_t& quality_level) override;

    std::shared_ptr<Buffer> CreateIndexBuffer(size_t size, IndexFormat type) override;
    std::shared_ptr<Buffer> CreateVertexBuffer(size_t size, size_t stride) override;

    std::shared_ptr<Buffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic) override;

    template<typename T>
    std::shared_ptr<Buffer> CreateConstantBuffer(const T& data, UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(&data, sizeof(data), usage);
    }

    template<typename T>
    std::shared_ptr<Buffer> CreateConstantBuffer(UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(nullptr, sizeof(T), usage);
    }

    std::shared_ptr<PipelineState> CreatePipelineState(Program* program, RasterStateDesc rs, const InputLayoutDesc& layout) override;
    std::shared_ptr<Shader> CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr,
        const std::vector<ShaderMacroEntry>& macros = { {nullptr, nullptr} }, const char* target = nullptr) override;

    std::shared_ptr<Program> CreateProgram(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr) override;

    std::shared_ptr<Texture> CreateTexture(const TextureDescription& desc) override;
    std::shared_ptr<Texture> CreateTexture(SwapChain* swapchain) override;
    std::shared_ptr<Query> CreateQuery(QueryType type, int capacity) override;

    std::shared_ptr<RenderTarget> CreateRenderTarget(uint32_t width, uint32_t height) override;

private:
    static constexpr uint32_t kQueryArraySize = 1024;

    void ProcessReadback();

    ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4>& adapter);
    ComPtr<IDXGIAdapter4> CreateAdapter(bool use_warp);

    static D3D12GfxDriver* self_;

    ComPtr<ID3D12Device2> device_;
    D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12_options_;

    ComPtr<ID3D12DescriptorHeap> imgui_srv_heap_;
    ComPtr<ID3D12GraphicsCommandList> imgui_command_list_;

    std::unique_ptr<D3D12DescriptorHeapAllocator> descriptor_allocators_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    std::unique_ptr<D3D12UploadBufferAllocator> upload_allocator_;
    std::unique_ptr<D3D12DefaultBufferAllocator> default_allocator_;
    std::unique_ptr<D3D12TextureResourceAllocator> texture_allocator_;
    std::unique_ptr<D3D12ReadbackBufferAllocator> readback_allocator_;
    
    std::unique_ptr<LinearAllocator> linear_allocator_;

    std::unique_ptr<D3D12SwapChain> swap_chain_;
    std::unique_ptr<MipsGenerator> mips_generator_;
    std::queue<std::pair<uint64_t, D3D12Texture::ReadbackTask>> readback_queue_;

    std::unique_ptr<D3D12CommandQueue> direct_command_queue_;
    std::unique_ptr<D3D12CommandQueue> copy_command_queue_;
    std::unique_ptr<D3D12CommandQueue> compute_command_queue_;
};

}
}