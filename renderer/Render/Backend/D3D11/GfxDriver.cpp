#include "gfxdriver.h"
#include <sstream>
#include <cmath>
#include <array>
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "Common/Util.h"
#include "exception/exception.h"
#include "render/camera.h"
#include "render/material.h"
#include "inputlayout.h"
#include "indexbuffer.h"
#include "vertexbuffer.h"
#include "ConstantBuffer.h"
#include "pipelinestate.h"
#include "sampler.h"
#include "shader.h"
#include "texture.h"
#include "rendertarget.h"
#include "query.h"
#include "imguizmo/ImGuizmo.h"
#include "Program.h"
#include "Render/Base/Util.h"

namespace glacier {
namespace render {

//GfxDriverD3D11* GfxDriverD3D11::self_ = nullptr;

void GfxDriverD3D11::Init(HWND hWnd, int width, int height ) {
    assert(!device_);
    assert(!driver_);

    driver_ = this;
    //self_ = this;

    width_ = width;
    height_ = height;


    D3D_FEATURE_LEVEL feature_level[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
    };

    UINT device_create_flags = 0u;
#if defined(_DEBUG)
    device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_level_;
    GfxThrowIfFailed(D3D11CreateDevice(
        nullptr,                            // specify nullptr to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_create_flags,
        feature_level,
        static_cast<UINT>(std::size(feature_level)),
        D3D11_SDK_VERSION,
        &device_,    // returns the Direct3D device created
        &feature_level_,    // returns feature level of device created
        &context_    // returns the device immediate context
    ));

//ensures that a common but harmless warning message is suppressed in non - Production builds, 
//and enables ‘break on’ functionality in Debug builds if there are any serious corruption or error messages.
#if defined(_DEBUG)
    HRESULT hr = device_->QueryInterface(__uuidof(ID3D11Debug), &d3d_debug_);
    if (SUCCEEDED(hr)) {
        ComPtr<ID3D11InfoQueue> d3d_infoqueue;
        hr = d3d_debug_.As(&d3d_infoqueue);
        if (SUCCEEDED(hr))
        {
            d3d_infoqueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3d_infoqueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            //d3d_infoqueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);

            D3D11_MESSAGE_ID hide[] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3d_infoqueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    swap_chain_ = std::make_unique<D3D11SwapChain>(this, hWnd, width, height);
    swap_chain_->CreateRenderTarget();

    // Init ImGui Win32 Impl
    ImGui_ImplWin32_Init(hWnd);
    // init imgui d3d impl
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());
}

GfxDriverD3D11::~GfxDriverD3D11() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();

#if defined(_DEBUG)
    if (d3d_debug_) {
        d3d_debug_->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
#endif

    D3D11InputLayout::Clear();
    D3D11PipelineState::Clear();
    D3D11Sampler::Clear();
    D3D11Shader::Clear();

    driver_ = nullptr;
    //self_ = nullptr;
}

void GfxDriverD3D11::Present() {
    if (imgui_enable_) {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
    }

#ifndef NDEBUG
    DxgiInfo::Instance()->Set();
#endif

    swap_chain_->Present();
}

void GfxDriverD3D11::BeginFrame() {
    if (imgui_enable_) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }
}

void GfxDriverD3D11::EndFrame() {

}

void GfxDriverD3D11::CheckMSAA(MSAAType msaa, uint32_t& smaple_count, uint32_t& quality_level) {
    uint32_t target_sample_count = (uint32_t)msaa;
    auto backbuffer_format = swap_chain_->GetFormate();
    for (smaple_count = target_sample_count; smaple_count > 1; smaple_count--)
    {
        quality_level = 0;
        if (device_->CheckMultisampleQualityLevels(backbuffer_format, smaple_count, &quality_level))
            continue;

        if (quality_level > 0) {
            --quality_level;
            break;
        }
    }

    if (smaple_count < 2)
    {
        throw std::exception("MSAA not supported");
    }
}

void GfxDriverD3D11::ResolveMSAA(std::shared_ptr<Texture>& src, std::shared_ptr<Texture>& dst, TextureFormat format) {
    GfxThrowIfAny(context_->ResolveSubresource((ID3D11Texture2D*)dst->underlying_resource(), 0, 
        (ID3D11Texture2D*)src->underlying_resource(), 0, GetUnderlyingFormat(format)));
}

void GfxDriverD3D11::DrawIndexed(uint32_t count) {
    GfxThrowIfAny(context_->DrawIndexed(count, 0u, 0u));
}

void GfxDriverD3D11::Draw(uint32_t count, uint32_t offset) {
    GfxThrowIfAny(context_->Draw(count, offset));
}

std::shared_ptr<InputLayout> GfxDriverD3D11::CreateInputLayout(const InputLayoutDesc& layout) {
    auto ret = D3D11InputLayout::Create(layout);// std::make_shared<D3D11InputLayout>(layout);
    return ret;
}

std::shared_ptr<IndexBuffer> GfxDriverD3D11::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
    auto ret = std::make_shared<D3D11IndexBuffer>(indices);
    return ret;
}

std::shared_ptr<VertexBuffer> GfxDriverD3D11::CreateVertexBuffer(const VertexData& vertices) {
    auto ret = std::make_shared<D3D11VertexBuffer>(vertices);
    return ret;
}

std::shared_ptr<VertexBuffer> GfxDriverD3D11::CreateVertexBuffer(const void* data, 
    size_t size, size_t stride, UsageType usage) 
{
    auto ret = std::make_shared<D3D11VertexBuffer>(data, size, stride, usage);
    return ret;
}

std::shared_ptr<ConstantBuffer> GfxDriverD3D11::CreateConstantBuffer(const void* data, size_t size, UsageType usage) {
    auto ret = std::make_shared<D3D11Buffer>(data, size, usage);
    return ret;
}

std::shared_ptr<ConstantBuffer> GfxDriverD3D11::CreateConstantBuffer(std::shared_ptr<BufferData>& data) {
    auto ret = std::make_shared<D3D11Buffer>(data);
    return ret;
}

std::shared_ptr<PipelineState> GfxDriverD3D11::CreatePipelineState(RasterState rs) {
    auto ret = D3D11PipelineState::Create(rs);
    return ret;
}

std::shared_ptr<Sampler> GfxDriverD3D11::CreateSampler(const SamplerBuilder& builder) {
    auto ret = std::make_shared<D3D11Sampler>(builder);
    return ret;
}

std::shared_ptr<Sampler> GfxDriverD3D11::CreateSampler(const SamplerState& ss) {
    auto ret = D3D11Sampler::Create(ss);
    return ret;
}

std::shared_ptr<Shader> GfxDriverD3D11::CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const std::vector<ShaderMacroEntry>& macros, const char* target)
{
    if (macros.empty()) {
        auto ret = D3D11Shader::Create(type, file_name, entry_point, target);
        return ret;
    } else { //permutations are not cached
        auto ret = std::make_shared<D3D11Shader>(type, file_name, entry_point, target, macros);
        return ret;
    }
}

std::shared_ptr<Program> GfxDriverD3D11::CreateProgram(const char* name, const TCHAR* vs, const TCHAR* ps) {
    auto program = std::make_shared<D3D11Program>(name);
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

std::shared_ptr<Texture> GfxDriverD3D11::CreateTexture(const TextureBuilder& builder) {
    auto ret = std::make_shared<D3D11Texture>(builder);
    return ret;
}

std::shared_ptr<RenderTarget> GfxDriverD3D11::CreateRenderTarget(uint32_t width, uint32_t height) {
    return std::make_shared<D3D11RenderTarget>(width, height);
}

std::shared_ptr<Query> GfxDriverD3D11::CreateQuery(QueryType type, int capacity) {
    auto ret = std::make_shared<D3D11Query>(type, capacity);
    return ret;
}

}
}
