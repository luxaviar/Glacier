#include "GfxDriver.h"
#include "Buffer.h"
#include "PipelineState.h"
#include "Program.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "Query.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imguizmo/ImGuizmo.h"
#include "Render/LightManager.h"
#include "Render/Editor/Gizmos.h"
#include "Sampler.h"
#include "CommandBuffer.h"
#include "MipsGenerator.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

D3D12GfxDriver* D3D12GfxDriver::self_ = nullptr;

D3D12GfxDriver::D3D12GfxDriver() {

}

void D3D12GfxDriver::Init(HWND hWnd, int width, int height, TextureFormat format) {
    self_ = this;
    driver_ = this;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug1> d3d_debug;
    GfxThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d_debug)));
    d3d_debug->EnableDebugLayer();
    // Enable these if you want full validation (will slow down rendering a lot).
    //d3d_debug->SetEnableGPUBasedValidation(TRUE);
    //d3d_debug->SetEnableSynchronizedCommandQueueValidation(TRUE);
#endif

    auto adapter = CreateAdapter(false);
    device_ = CreateDevice(adapter);
    GfxThrowIfFailed(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &D3D12_options_, sizeof(D3D12_options_)));

    direct_command_queue_ = std::make_unique<D3D12CommandQueue>(this, CommandBufferType::kDirect);
    direct_command_queue_->SetName(TEXT("D3D12GfxDriver direct command queue"));

    copy_command_queue_ = std::make_unique<D3D12CommandQueue>(this, CommandBufferType::kCopy);
    copy_command_queue_->SetName(TEXT("D3D12GfxDriver copy command queue"));

    compute_command_queue_ = std::make_unique<D3D12CommandQueue>(this, CommandBufferType::kCompute);
    compute_command_queue_->SetName(TEXT("D3D12GfxDriver compute command queue"));

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        descriptor_allocators_[i] = std::make_unique<D3D12DescriptorHeapAllocator>(
            device_.Get(), static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i), 1024);
    }

    upload_allocator_ = std::make_unique<D3D12UploadBufferAllocator>(device_.Get());
    default_allocator_ = std::make_unique<D3D12DefaultBufferAllocator>(device_.Get());
    texture_allocator_ = std::make_unique<D3D12TextureResourceAllocator>(device_.Get());
    readback_allocator_ = std::make_unique<D3D12ReadbackBufferAllocator>(device_.Get());

    linear_allocator_ = std::make_unique<LinearAllocator>(LinearAllocator::kUploadPageSize);

    swap_chain_ = std::make_unique<D3D12SwapChain>(this, hWnd, width, height, format);
    swap_chain_->CreateRenderTarget();

    mips_generator_ = std::make_unique<MipsGenerator>(device_.Get());

    D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc;
    SrvHeapDesc.NumDescriptors = 1;
    SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    SrvHeapDesc.NodeMask = 0;
    GfxThrowIfFailed(device_->CreateDescriptorHeap(
        &SrvHeapDesc, IID_PPV_ARGS(imgui_srv_heap_.GetAddressOf())));

    // Init ImGui Win32 Impl
    ImGui_ImplWin32_Init(hWnd);
    // init imgui d3d impl
    ImGui_ImplDX12_Init(device_.Get(), kBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM, imgui_srv_heap_.Get(),
        imgui_srv_heap_.Get()->GetCPUDescriptorHandleForHeapStart(),
        imgui_srv_heap_.Get()->GetGPUDescriptorHandleForHeapStart());
}

ComPtr<ID3D12Device2> D3D12GfxDriver::CreateDevice(ComPtr<IDXGIAdapter4>& adapter) {
    ComPtr<ID3D12Device2> device;
    GfxThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(device.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        //pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES,				// This started happening after updating to an RTX 2080 Ti. I believe this to be an error in the validation layer itself.
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;

        GfxThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif
    return device;
}

ComPtr<IDXGIAdapter4> D3D12GfxDriver::CreateAdapter(bool use_warp) {
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    GfxThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (use_warp) {
        GfxThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        GfxThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                GfxThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    return dxgiAdapter4;
}

void D3D12GfxDriver::OnDestroy() {
    render::MaterialManager::Instance()->Clear();
    render::LightManager::Instance()->Clear();
    render::Gizmos::Instance()->OnDestroy();
    D3D12Sampler::Clear();
}

D3D12GfxDriver::~D3D12GfxDriver() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();

#ifndef  NDEBUG
    ComPtr<IDXGIDebug1> dxgi_debug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug));

    dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
#endif

    driver_ = nullptr;
    self_ = nullptr;
}

CommandQueue* D3D12GfxDriver::GetCommandQueue(CommandBufferType type) {
    auto native_type = GetNativeCommandBufferType(type);
    return GetCommandQueue(native_type);
}

CommandBuffer* D3D12GfxDriver::GetCommandBuffer(CommandBufferType type) {
    auto native_type = GetNativeCommandBufferType(type);
    auto queue = GetCommandQueue(native_type);
    return queue->GetCommandBuffer();
}

D3D12DescriptorHeapAllocator* D3D12GfxDriver::GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE HeapType) const {
    return descriptor_allocators_[HeapType].get();
}

D3D12CommandQueue* D3D12GfxDriver::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const {
    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        return direct_command_queue_.get();
    }
    else if (type == D3D12_COMMAND_LIST_TYPE_COPY) {
        return copy_command_queue_.get();
    }
    else {
        return compute_command_queue_.get();
    }
}

D3D12CommandBuffer* D3D12GfxDriver::GetCommandList(D3D12_COMMAND_LIST_TYPE type) const {
    auto queue = GetCommandQueue(type);
    auto cmd_buffer = queue->GetCommandBuffer();
    return static_cast<D3D12CommandBuffer*>(cmd_buffer);
}

void D3D12GfxDriver::EnqueueReadback(D3D12Texture::ReadbackTask&& task) {
    readback_queue_.emplace(std::make_pair(direct_command_queue_->GetCompletedFenceValue() + 1, std::move(task)));
}

DXGI_SAMPLE_DESC D3D12GfxDriver::GetMultisampleQualityLevels(DXGI_FORMAT format, UINT numSamples,
    D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags) const
{
    DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
    qualityLevels.Format = format;
    qualityLevels.SampleCount = 1;
    qualityLevels.Flags = flags;
    qualityLevels.NumQualityLevels = 0;

    while (
        qualityLevels.SampleCount <= numSamples &&
        SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels,
            sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) &&
        qualityLevels.NumQualityLevels > 0)
    {
        // That works...
        sampleDesc.Count = qualityLevels.SampleCount;
        sampleDesc.Quality = qualityLevels.NumQualityLevels - 1;

        // But can we do better?
        qualityLevels.SampleCount *= 2;
    }

    return sampleDesc;
}

void D3D12GfxDriver::GenerateMipMaps(D3D12CommandBuffer* cmd_buffer, D3D12Texture* texture) {
    mips_generator_->Generate(cmd_buffer, texture);
}

void D3D12GfxDriver::BeginFrame() {
    PerfGuard gurad("begin frame");

    swap_chain_->Wait();

    if (imgui_enable_) {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }
}

void D3D12GfxDriver::Present(CommandBuffer* cmd_buffer) {
    PerfGuard gurad("present");

    std::vector<CommandBuffer*> cmd_buffers;
    cmd_buffers.reserve(3);

    cmd_buffers.push_back(cmd_buffer);

    if (imgui_enable_) {
        auto cmd_buffer = GetCommandBuffer(CommandBufferType::kDirect);
        swap_chain_->GetRenderTarget()->Bind(cmd_buffer);

        auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer)->GetNativeCommandList();
        cmd_list->SetDescriptorHeaps(1, imgui_srv_heap_.GetAddressOf());
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);

        cmd_buffers.push_back(cmd_buffer);
    }

    swap_chain_->Present(cmd_buffers);
}

void D3D12GfxDriver::EndFrame() {
    PerfGuard gurad("end frame");

    direct_command_queue_->Flush();

    ProcessReadback();
    linear_allocator_->Cleanup(direct_command_queue_->GetCompletedFenceValue());
}

void D3D12GfxDriver::CheckMSAA(uint32_t target_sample_count, uint32_t& smaple_count, uint32_t& quality_level) {
    auto backbuffer_format = swap_chain_->GetNativeFormat();
    for (smaple_count = target_sample_count; smaple_count > 1; smaple_count--)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms_level;
        ms_level.Format = backbuffer_format;
        ms_level.SampleCount = smaple_count;
        ms_level.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        if (device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms_level, sizeof(ms_level)) == S_OK) {
            quality_level = ms_level.NumQualityLevels;

            if (quality_level > 0) {
                --quality_level;
            }

            break;
        }
    }

    if (smaple_count < 2)
    {
        throw std::exception("MSAA not supported");
    }
}

void D3D12GfxDriver::ProcessReadback() {
    auto complete_value = direct_command_queue_->GetCompletedFenceValue();
    while (!readback_queue_.empty()) {
        if (readback_queue_.front().first > complete_value) {
            break;
        }

        readback_queue_.front().second.Process();
        readback_queue_.pop();
    }
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateIndexBuffer(size_t size, IndexFormat type) {
    return std::make_shared<D3D12Buffer>(size, type);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateVertexBuffer(size_t size, size_t stride) {
    return std::make_shared<D3D12Buffer>(size, stride);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateConstantBuffer(const void* data, size_t size, UsageType usage) {
    return std::make_shared<D3D12Buffer>(data, size, usage);
}

std::shared_ptr<PipelineState> D3D12GfxDriver::CreatePipelineState(Program* program, RasterStateDesc rs, const InputLayoutDesc& layout) {
    return std::make_shared<D3D12PipelineState>(program, rs, layout);
}

std::shared_ptr<Shader> D3D12GfxDriver::CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const std::vector<ShaderMacroEntry>& macros, const char* target)
{
    if (macros.empty()) {
        auto ret = D3D12Shader::Create(type, file_name, entry_point, target);
        return ret;
    }
    else { //permutations are not cached
        auto ret = std::make_shared<D3D12Shader>(type, file_name, entry_point, target, macros);
        return ret;
    }
}

std::shared_ptr<Program> D3D12GfxDriver::CreateProgram(const char* name, const TCHAR* vs, const TCHAR* ps) {
    auto program = std::make_shared<D3D12Program>(name);
    if (vs) {
        auto shader = CreateShader(ShaderType::kVertex, vs);
        program->SetShader(shader);
    }

    if (ps) {
        auto shader = CreateShader(ShaderType::kPixel, ps);
        program->SetShader(shader);
    }

    return program;
}

std::shared_ptr<Texture> D3D12GfxDriver::CreateTexture(const TextureDescription& desc) {
    if ((desc.create_flags & (uint32_t)CreateFlags::kDepthStencil) > 0) {
        // Specify optimized clear values for the depth buffer.
        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = GetDSVFormat(GetUnderlyingFormat(desc.format));
#ifdef GLACIER_REVERSE_Z
        optimizedClearValue.DepthStencil = { 0.0F, 0 };
#else
        optimizedClearValue.DepthStencil = { 1.0F, 0 };
#endif

        return std::make_shared<D3D12Texture>(desc, &optimizedClearValue);
    }
    else {
        return std::make_shared<D3D12Texture>(desc);
    }
}

std::shared_ptr<Texture> D3D12GfxDriver::CreateTexture(SwapChain* swapchain) {
    auto res = static_cast<D3D12SwapChain*>(swapchain)->GetBackBuffer();
    return std::make_shared<D3D12Texture>(res);
}

std::shared_ptr<RenderTarget> D3D12GfxDriver::CreateRenderTarget(uint32_t width, uint32_t height) {
    return std::make_shared<D3D12RenderTarget>(width, height);
}

std::shared_ptr<Query> D3D12GfxDriver::CreateQuery(QueryType type, int capacity) {
    return std::make_shared<D3D12Query>(this, type, capacity);
}

}
}
