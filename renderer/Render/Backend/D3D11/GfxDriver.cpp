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
#include "Buffer.h"
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

void D3D11GfxDriver::Init(HWND hWnd, int width, int height, TextureFormat format) {
    assert(!device_);
    assert(!driver_);

    driver_ = this;

    //width_ = width;
    //height_ = height;


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

    swap_chain_ = std::make_unique<D3D11SwapChain>(this, hWnd, width, height, format);
    swap_chain_->CreateRenderTarget();

    // Init ImGui Win32 Impl
    ImGui_ImplWin32_Init(hWnd);
    // init imgui d3d impl
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());
}

D3D11GfxDriver::~D3D11GfxDriver() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();

#if defined(_DEBUG)
    if (d3d_debug_) {
        d3d_debug_->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
#endif

    D3D11InputLayout::Clear();
    D3D11RasterState::Clear();
    D3D11Sampler::Clear();
    D3D11Shader::Clear();

    driver_ = nullptr;
    //self_ = nullptr;
}

void D3D11GfxDriver::Present() {
    if (imgui_enable_) {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
    }

#ifndef NDEBUG
    DxgiInfo::Instance()->Set();
#endif

    swap_chain_->Present();
}

void D3D11GfxDriver::BeginFrame() {
    if (imgui_enable_) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }
}

void D3D11GfxDriver::EndFrame() {

}

void D3D11GfxDriver::CheckMSAA(MSAAType msaa, uint32_t& smaple_count, uint32_t& quality_level) {
    uint32_t target_sample_count = (uint32_t)msaa;
    auto backbuffer_format = GetUnderlyingFormat(swap_chain_->GetFormat());
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

void D3D11GfxDriver::CopyResource(const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst) {
    const auto& src_res = std::dynamic_pointer_cast<D3D11Resource>(src);
    const auto& dst_res = std::dynamic_pointer_cast<D3D11Resource>(dst);

    context_->CopyResource(src_res->GetUnderlyingResource(), dst_res->GetUnderlyingResource());
}

void D3D11GfxDriver::DrawIndexed(uint32_t count) {
    GfxThrowIfAny(context_->DrawIndexed(count, 0u, 0u));
}

void D3D11GfxDriver::Draw(uint32_t count, uint32_t offset) {
    GfxThrowIfAny(context_->Draw(count, offset));
}

std::shared_ptr<IndexBuffer> D3D11GfxDriver::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
    auto ret = std::make_shared<D3D11IndexBuffer>(indices);
    return ret;
}

std::shared_ptr<IndexBuffer> D3D11GfxDriver::CreateIndexBuffer(const void* data,
    size_t size, IndexFormat type, UsageType usage) {
    auto ret = std::make_shared<D3D11IndexBuffer>(data, size, type, usage);
    return ret;
}

std::shared_ptr<VertexBuffer> D3D11GfxDriver::CreateVertexBuffer(const void* data, 
    size_t size, size_t stride, UsageType usage) 
{
    auto ret = std::make_shared<D3D11VertexBuffer>(data, size, stride, usage);
    return ret;
}

std::shared_ptr<VertexBuffer> D3D11GfxDriver::CreateVertexBuffer(const VertexData& vertices) {
    auto ret = std::make_shared<D3D11VertexBuffer>(vertices);
    return ret;
}

std::shared_ptr<ConstantBuffer> D3D11GfxDriver::CreateConstantBuffer(const void* data, size_t size, UsageType usage) {
    auto ret = std::make_shared<D3D11ConstantBuffer>(data, size, usage);
    return ret;
}

std::shared_ptr<ConstantBuffer> D3D11GfxDriver::CreateConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage) {
    auto ret = std::make_shared<D3D11ConstantBuffer>(data, usage);
    return ret;
}

std::shared_ptr<PipelineState> D3D11GfxDriver::CreatePipelineState(RasterStateDesc rs, const InputLayoutDesc& layout) {
    auto ret = std::make_shared<D3D11PipelineState>(rs, layout);
    return ret;
}

std::shared_ptr<Shader> D3D11GfxDriver::CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point,
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

std::shared_ptr<Program> D3D11GfxDriver::CreateProgram(const char* name, const TCHAR* vs, const TCHAR* ps) {
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

std::shared_ptr<Texture> D3D11GfxDriver::CreateTexture(const TextureDescription& desc) {
    auto ret = std::make_shared<D3D11Texture>(desc);
    return ret;
}

std::shared_ptr<Texture> D3D11GfxDriver::CreateTexture(SwapChain* swapchain) {
    return std::make_shared<D3D11Texture>(swapchain);
}

std::shared_ptr<RenderTarget> D3D11GfxDriver::CreateRenderTarget(uint32_t width, uint32_t height) {
    return std::make_shared<D3D11RenderTarget>(width, height);
}

std::shared_ptr<Query> D3D11GfxDriver::CreateQuery(QueryType type, int capacity) {
    auto ret = std::make_shared<D3D11Query>(this, type, capacity);
    return ret;
}


void D3D11GfxDriver::BindMaterial(Material* mat) {
    if (!mat || material_ == mat) return;

    auto temp = mat->GetTemplate().get();

    if (temp != material_template_) {
        if (material_template_) {
            material_template_->UnBind(this);
        }

        temp->Bind(this);
        material_template_ = temp;
    }

    if (material_) {
        material_->UnBind(this);
    }

    material_ = mat;
    material_->Bind(this);

    return;
}

void D3D11GfxDriver::UnBindMaterial() {
    if (material_) {
        material_->UnBind(this);
        material_ = nullptr;
    }

    if (material_template_) {
        material_template_->UnBind(this);
        material_template_ = nullptr;
    }
}

}
}
