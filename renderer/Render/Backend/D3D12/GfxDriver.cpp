#include "GfxDriver.h"
#include "Buffer.h"
#include "PipelineState.h"
#include "Program.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "Query.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include <ImGuizmo.h>
#include "Render/LightManager.h"
#include "Render/Editor/Gizmos.h"
#include "Render/Mesh/Model.h"
#include "Render/Skinning/BoneMatrixPool.h"
#include "Sampler.h"
#include "CommandBuffer.h"
#include "MipsGenerator.h"
#include "Inspect/Profiler.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

// Set to 1 to make every frame end with a wait for the GPU, which is how the
// renderer worked before the frame contexts were added. It is a debugging switch
// for bisecting a flicker or a device removal: the frame end still has to do the
// recycling either way, and the switch only adds the wait back.
#ifndef GLACIER_FLUSH_EVERY_FRAME
#define GLACIER_FLUSH_EVERY_FRAME 0
#endif

LUX_IMPL(GfxDriver, GfxDriver)
LUX_FUNC(GfxDriver, LGetCommandQueue)
LUX_IMPL_END

LUX_IMPL(D3D12GfxDriver, D3D12GfxDriver)
LUX_CTOR(D3D12GfxDriver)
LUX_FUNC(D3D12GfxDriver, Instance)
LUX_FUNC(D3D12GfxDriver, Init)
LUX_IMPL_END

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

#if IS_DEBUG
    // The debug layer keeps no messages unless it is told how many to keep, and
    // the queue is what a run is checked against on the way out (see
    // DumpDebugLayerMessages). The last 1024 of them are enough for a teardown.
    {
        ComPtr<ID3D12InfoQueue> info_queue;
        if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
            info_queue->SetMessageCountLimit(1024);
        }
    }
#endif

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

    D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
    SrvHeapDesc.NumDescriptors = 64;
    SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    SrvHeapDesc.NodeMask = 0;
    GfxThrowIfFailed(device_->CreateDescriptorHeap(
        &SrvHeapDesc, IID_PPV_ARGS(imgui_srv_heap_.GetAddressOf())));
    imgui_srv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    imgui_free_srv_descriptors_.reserve(SrvHeapDesc.NumDescriptors);
    for (uint32_t i = SrvHeapDesc.NumDescriptors; i > 0; --i) {
        imgui_free_srv_descriptors_.push_back(i - 1);
    }

    // Init ImGui Win32 Impl
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_InitInfo imgui_init_info;
    imgui_init_info.Device = device_.Get();
    imgui_init_info.CommandQueue = direct_command_queue_->GetNativeCommandQueue();
    imgui_init_info.NumFramesInFlight = kBufferCount;
    imgui_init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    imgui_init_info.SrvDescriptorHeap = imgui_srv_heap_.Get();
    imgui_init_info.UserData = this;
    imgui_init_info.SrvDescriptorAllocFn = &D3D12GfxDriver::ImGuiSrvDescriptorAlloc;
    imgui_init_info.SrvDescriptorFreeFn = &D3D12GfxDriver::ImGuiSrvDescriptorFree;
    GfxThrowIfFailed(ImGui_ImplDX12_Init(&imgui_init_info) ? S_OK : E_FAIL);
}

void D3D12GfxDriver::ImGuiSrvDescriptorAlloc(ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle)
{
    auto driver = static_cast<D3D12GfxDriver*>(info->UserData);
    assert(driver != nullptr && !driver->imgui_free_srv_descriptors_.empty());

    auto index = driver->imgui_free_srv_descriptors_.back();
    driver->imgui_free_srv_descriptors_.pop_back();

    auto cpu_start = driver->imgui_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    auto gpu_start = driver->imgui_srv_heap_->GetGPUDescriptorHandleForHeapStart();
    cpu_handle->ptr = cpu_start.ptr + index * driver->imgui_srv_descriptor_size_;
    gpu_handle->ptr = gpu_start.ptr + index * driver->imgui_srv_descriptor_size_;
}

void D3D12GfxDriver::ImGuiSrvDescriptorFree(ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
{
    auto driver = static_cast<D3D12GfxDriver*>(info->UserData);
    assert(driver != nullptr);

    auto cpu_start = driver->imgui_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    auto offset = cpu_handle.ptr - cpu_start.ptr;
    assert(driver->imgui_srv_descriptor_size_ != 0 && offset % driver->imgui_srv_descriptor_size_ == 0);

    auto index = static_cast<uint32_t>(offset / driver->imgui_srv_descriptor_size_);
    assert(index < 64);
    driver->imgui_free_srv_descriptors_.push_back(index);
    (void)gpu_handle;
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

#if IS_DEBUG
// The driver is held by the Lua side and outlives the log, so the messages of
// the debug layer are taken while the run is being torn down and the log is
// still there to take them (see the definition below)
static void DumpDebugLayerMessages(ID3D12Device* device);
#endif

void D3D12GfxDriver::OnDestroy() {
    render::Model::ClearCache();
    render::MaterialManager::Instance()->Clear();
    render::LightManager::Instance()->Clear();
    render::Gizmos::Instance()->OnDestroy();
    render::BoneMatrixPool::Instance()->Release();
    D3D12Sampler::Clear();
    D3D12CommandBuffer::ClearTextureCache();

#if IS_DEBUG
    DumpDebugLayerMessages(device_.Get());
#endif
}

#if IS_DEBUG
// The debug layer reports to the debugger, and a Debug run started by hand has
// none attached, so everything it says would be lost. The info queue keeps the
// messages instead: they are drained into the log on the way out, which is what
// says whether a Debug run of the three scenes was clean.
static void DumpDebugLayerMessages(ID3D12Device* device) {
    ComPtr<ID3D12InfoQueue> info_queue;
    if (device == nullptr || FAILED(device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        return;
    }

    // A run stores a lot of messages, most of them information about what the
    // layer watched happen; writing all of them would take longer than the run.
    // The newest ones are the ones of the teardown, so a bounded window of them
    // is scanned; the count is what says whether anything was reported, and the
    // lines of the two severities above information are what a run is judged on.
    constexpr UINT64 kMaxScanned = 8192;
    constexpr UINT64 kMaxPrinted = 128;

    const UINT64 count = info_queue->GetNumStoredMessages();
    const UINT64 first = count > kMaxScanned ? count - kMaxScanned : 0;
    UINT64 errors = 0;
    UINT64 warnings = 0;
    UINT64 printed = 0;

    for (UINT64 i = first; i < count; ++i) {
        SIZE_T size = 0;
        if (FAILED(info_queue->GetMessage(i, nullptr, &size)) || size == 0) {
            continue;
        }

        std::vector<uint8_t> buffer(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
        if (FAILED(info_queue->GetMessage(i, message, &size))) {
            continue;
        }

        const bool is_error = message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
            message->Severity == D3D12_MESSAGE_SEVERITY_ERROR;
        const bool is_warning = message->Severity == D3D12_MESSAGE_SEVERITY_WARNING;
        if (!is_error && !is_warning) {
            continue;
        }

        if (is_error) {
            ++errors;
        }
        else {
            ++warnings;
        }

        if (printed >= kMaxPrinted) {
            continue;
        }

        ++printed;
        if (is_error) {
            LOG_ERR("d3d12 debug layer: [{}] {}", (int)message->ID, message->pDescription);
        }
        else {
            LOG_WARN("d3d12 debug layer: [{}] {}", (int)message->ID, message->pDescription);
        }
    }

    LOG_LOG("d3d12 debug layer: {} stored messages, last {} scanned, {} errors, {} warnings, {} printed",
        count, count - first, errors, warnings, printed);

    // A Debug run can end without the file being closed - the debug layer of the
    // build is what has been seen to do that - and what is still in the buffer
    // of the log would be lost with it. The dump is pushed out now instead.
    Logging::Instance()->Flush();
}
#endif

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

void D3D12GfxDriver::EnqueueReadback(CommandBuffer* cmd_buffer, D3D12Texture::ReadbackTask&& task) {
    readback_queue_.push({ std::move(task), cmd_buffer });
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

    // the one limiter the frame still has to respect: the swapchain waits until
    // the display has room for another frame
    swap_chain_->Wait();

    // give back what the GPU finished with before the frame asks for any of it:
    // command lists (and the temporary resources recorded into them) and the
    // upload pages of the linear allocator
    direct_command_queue_->CollectCompletedCommandLists();
    linear_allocator_->BeginFrame(direct_command_queue_->GetCompletedFenceValue());

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

#if GLACIER_FLUSH_EVERY_FRAME
    // the way the loop worked before the frame contexts: the frame ends only once
    // the GPU is done with it, which serializes the two of them
    direct_command_queue_->Flush();
#endif

    // the frame is submitted and accounted for, but not waited for. Everything
    // the frame used is retired under this fence, and the frame after the next
    // one only takes it back once the GPU says it is done (see BeginFrame and
    // D3D12CommandQueue::CollectCompletedCommandLists).
    const uint64_t frame_fence = direct_command_queue_->Signal();
    frame_contexts_[frame_index_].fence = frame_fence;
    linear_allocator_->EndFrame(frame_fence);

    ProcessReadback();

    frame_index_ = (frame_index_ + 1) % kFramesInFlight;

    // the frame that used the context being taken up again has to be done; with
    // the swapchain holding the CPU back as well, this is normally no wait at all
    const FrameContext& context = frame_contexts_[frame_index_];
    if (context.fence != 0) {
        direct_command_queue_->WaitForFenceValue(context.fence, "WaitForFrameContext");
    }
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
        auto& entry = readback_queue_.front();
        // the copy was recorded into a command buffer that gets submitted after
        // the task was queued, so the fence is asked for here rather than when
        // the task was queued (0 means it is not submitted yet)
        const uint64_t fence = entry.cmd_buffer != nullptr ? entry.cmd_buffer->submitted_fence() : 0;
        if (fence == 0 || fence > complete_value) {
            break;
        }

        entry.task.Process();
        readback_queue_.pop();
    }
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateIndexBuffer(size_t size, IndexFormat type) {
    return std::make_shared<D3D12IndexBuffer>(size, type);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateVertexBuffer(size_t size, size_t stride, CreateFlags flags) {
    return std::make_shared<D3D12VertexBuffer>(size, stride, flags);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateConstantBuffer(const void* data, size_t size, UsageType usage) {
    return std::make_shared<D3D12ConstantBuffer>(data, size, usage);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateStructuredBuffer(size_t element_size, size_t element_count, bool uav) {
    if (uav) {
        return std::make_shared<D3D12RWStructuredBuffer>(element_size, element_count);
    }
    else {
        return std::make_shared<D3D12StructuredBuffer>(element_size, element_count);
    }
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateDynamicStructuredBuffer(size_t element_size, size_t element_count) {
    return std::make_shared<D3D12DynamicStructuredBuffer>(element_size, element_count);
}

std::shared_ptr<Buffer> D3D12GfxDriver::CreateByteAddressBuffer(size_t size, bool uav) {
    if (uav) {
        return std::make_shared<D3D12RWByteAddressBuffer>(size);
    }
    else {
        return std::make_shared<D3D12ByteAddressBuffer>(size);
    }
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
