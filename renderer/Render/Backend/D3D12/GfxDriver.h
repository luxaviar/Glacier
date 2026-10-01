#pragma once

#include <queue>
#include <memory>
#include <vector>
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

struct ImGui_ImplDX12_InitInfo;

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

    // cmd_buffer is the one the copy was recorded into: it is submitted after the
    // task is queued (see Renderer::CaptureScreen), so it is the command buffer
    // that knows the fence the readback is done under
    void EnqueueReadback(CommandBuffer* cmd_buffer, D3D12Texture::ReadbackTask&& task);

    ID3D12Device2* GetDevice() const { return device_.Get(); }
    SwapChain* GetSwapChain() const { return swap_chain_.get(); }

    void GenerateMipMaps(D3D12CommandBuffer* cmd_buffer, D3D12Texture* texture);

    void BeginFrame() override;
    void EndFrame() override;

    void Present(CommandBuffer* cmd_buffer) override;

    void CheckMSAA(uint32_t sample_count, uint32_t& smaple_count, uint32_t& quality_level) override;

    bool vsync() const override { return swap_chain_ ? swap_chain_->vsync() : vsync_; }
    void vsync(bool v) override {
        vsync_ = v;

        if (swap_chain_) {
            swap_chain_->vsync(v);
        }
    }

    std::shared_ptr<Buffer> CreateIndexBuffer(size_t size, IndexFormat type) override;
    std::shared_ptr<Buffer> CreateVertexBuffer(size_t size, size_t stride, CreateFlags flags = CreateFlags::kNone) override;

    std::shared_ptr<Buffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic) override;
    std::shared_ptr<Buffer> CreateStructuredBuffer(size_t element_size, size_t element_count, bool uav=false) override;
    std::shared_ptr<Buffer> CreateDynamicStructuredBuffer(size_t element_size, size_t element_count) override;
    std::shared_ptr<Buffer> CreateByteAddressBuffer(size_t size, bool uav = false) override;

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

    // How many frames the CPU is allowed to run ahead of the GPU, which is also
    // how many frames of resources have to stay alive at a time. It is what the
    // swapchain lets it run ahead by (kBufferCount - 1 buffers, see
    // SetMaximumFrameLatency), so the two limiters agree on it.
    static constexpr uint32_t kFramesInFlight = D3D12SwapChain::kBufferCount - 1;

    struct FrameContext {
        // the fence the frame got when it was submitted; everything the frame
        // used is free to reuse once this completed
        uint64_t fence = 0;
    };

    void ProcessReadback();

    static void ImGuiSrvDescriptorAlloc(ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle,
        D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle);
    static void ImGuiSrvDescriptorFree(ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

    ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4>& adapter);
    ComPtr<IDXGIAdapter4> CreateAdapter(bool use_warp);

    static D3D12GfxDriver* self_;

    ComPtr<ID3D12Device2> device_;
    D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12_options_;

    ComPtr<ID3D12DescriptorHeap> imgui_srv_heap_;
    ComPtr<ID3D12GraphicsCommandList> imgui_command_list_;
    UINT imgui_srv_descriptor_size_ = 0;
    std::vector<uint32_t> imgui_free_srv_descriptors_;

    std::unique_ptr<D3D12DescriptorHeapAllocator> descriptor_allocators_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    std::unique_ptr<D3D12UploadBufferAllocator> upload_allocator_;
    std::unique_ptr<D3D12DefaultBufferAllocator> default_allocator_;
    std::unique_ptr<D3D12TextureResourceAllocator> texture_allocator_;
    std::unique_ptr<D3D12ReadbackBufferAllocator> readback_allocator_;
    
    std::unique_ptr<LinearAllocator> linear_allocator_;

    // a readback that is waiting for the GPU to have copied it
    struct ReadbackEntry {
        D3D12Texture::ReadbackTask task;
        CommandBuffer* cmd_buffer = nullptr;
    };

    std::unique_ptr<D3D12SwapChain> swap_chain_;
    std::unique_ptr<MipsGenerator> mips_generator_;
    std::queue<ReadbackEntry> readback_queue_;

    std::unique_ptr<D3D12CommandQueue> direct_command_queue_;
    std::unique_ptr<D3D12CommandQueue> copy_command_queue_;
    std::unique_ptr<D3D12CommandQueue> compute_command_queue_;

    FrameContext frame_contexts_[kFramesInFlight];
    uint32_t frame_index_ = 0;
};

}
}
