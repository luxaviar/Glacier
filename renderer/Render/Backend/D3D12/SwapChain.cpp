#include "SwapChain.h"
#include "Exception/Exception.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D12SwapChain::D3D12SwapChain(D3D12GfxDriver* driver, HWND hWnd, uint32_t width, uint32_t height, TextureFormat format) :
    SwapChain(width, height, format)
{
    driver_ = driver;

    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    GfxThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    tearing_support_ = CheckTearingSupport();
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = GetUnderlyingFormat(format);
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    //swapChainDesc.SampleDesc.Count = enable4xMsaa ? 4 : 1;
    //swapChainDesc.SampleDesc.Quality = enable4xMsaa ? (QualityOf4xMsaa - 1) : 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = tearing_support_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    auto command_queue = driver->GetCommandQueue()->GetUnderlyingCommandQueue();
    ComPtr<IDXGISwapChain1> swapChain1;
    GfxThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        command_queue,
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    GfxThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ComPtr<IDXGISwapChain4> swap_chain;
    GfxThrowIfFailed(swapChain1.As(&swap_chain_));

    GfxThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_)));

    cur_backbuffer_index_ = swap_chain_->GetCurrentBackBufferIndex();
    frame_latency_waitable_object_ = swap_chain_->GetFrameLatencyWaitableObject();
    swap_chain_->SetMaximumFrameLatency(kBufferCount - 1);

    //CreateRenderTarget();
}

D3D12SwapChain::~D3D12SwapChain() {

}

void D3D12SwapChain::CreateRenderTarget() {
    for (uint32_t i = 0; i < kBufferCount; ++i) {
        ComPtr<ID3D12Resource> back_buffer;
        GfxThrowIfFailed(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

        back_buffer_textures_[i] = std::make_shared<D3D12Texture>(back_buffer, D3D12_RESOURCE_STATE_COMMON);

        // Set the names for the backbuffer textures.
        // Useful for debugging.
        back_buffer_textures_[i]->SetName((TEXT("Backbuffer[") + std::to_wstring(i) + TEXT("]")).c_str());
    }

    render_target_ = std::make_shared<D3D12RenderTarget>(width_, height_);

    auto depth_tex_desc = Texture::Description()
        .SetDimension(width_, height_)
        .SetFormat(TextureFormat::kD24S8_UINT)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource);
    auto depthstencil_texture = driver_->CreateTexture(depth_tex_desc);
    depthstencil_texture->SetName(TEXT("swapchain depth texture"));

    render_target_->AttachDepthStencil(depthstencil_texture);
    render_target_->AttachColor(AttachmentPoint::kColor0, back_buffer_textures_[cur_backbuffer_index_]);
}

void D3D12SwapChain::OnResize(uint32_t width, uint32_t height) {
    if (width == width_ || height == height_) return;

    width_ = width;
    height_ = height;

    for (uint32_t i = 0; i < kBufferCount; ++i) {
        back_buffer_textures_[i]->ReleaseUnderlyingResource();
    }

    render_target_->DetachColor(AttachmentPoint::kColor0);
    render_target_->DetachDepthStencil();
    back_buffer_.Reset();

    DXGI_SWAP_CHAIN_DESC1 desc;
    GfxThrowIfFailed(swap_chain_->GetDesc1(&desc));
    GfxThrowIfFailed(swap_chain_->ResizeBuffers(kBufferCount, width, height, desc.Format, desc.Flags));
    GfxThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_)));

    for (uint32_t i = 0; i < kBufferCount; ++i) {
        ComPtr<ID3D12Resource> back_buffer;
        GfxThrowIfFailed(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));
        back_buffer_textures_[i]->Reset(back_buffer, D3D12_RESOURCE_STATE_COMMON);
    }

    //recreate rendertarget
    render_target_->Resize(width, height);
    auto depth_tex_desc = Texture::Description()
        .SetDimension(width_, height_)
        .SetFormat(TextureFormat::kD24S8_UINT)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource);
    auto depthstencil_texture = driver_->CreateTexture(depth_tex_desc);
    depthstencil_texture->SetName(TEXT("swapchain depth texture"));

    cur_backbuffer_index_ = swap_chain_->GetCurrentBackBufferIndex();

    render_target_->AttachDepthStencil(depthstencil_texture);
    render_target_->AttachColor(AttachmentPoint::kColor0, back_buffer_textures_[cur_backbuffer_index_]);
}

bool D3D12SwapChain::CheckTearingSupport() {
    BOOL allowTearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update.
    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
        {
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing, sizeof(allowTearing));
        }
    }

    return allowTearing == TRUE;
}

void D3D12SwapChain::Wait() {
    // Wait for 1 second (should never have to wait that long...)
    DWORD result = ::WaitForSingleObjectEx(frame_latency_waitable_object_, 1000, TRUE);
}

void D3D12SwapChain::Present() {

#ifndef NDEBUG
    DxgiInfo::Instance()->Set();
#endif

    auto command_queue = driver_->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto command_list = command_queue->GetCommandList();

    auto backBuffer = back_buffer_textures_[cur_backbuffer_index_].get();

    backBuffer->TransitionBarrier(command_list, D3D12_RESOURCE_STATE_PRESENT);

    command_queue->ExecuteCommandList();

    UINT presentFlags = tearing_support_ && !full_screen_ && !vsync_ ? DXGI_PRESENT_ALLOW_TEARING : 0;

    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    if (HRESULT hr; FAILED(hr = swap_chain_->Present(vsync_ ? 1u : 0u, presentFlags))) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            throw GraphicsDeviceRemovedException(__LINE__, TEXT(__FILE__), driver_->GetDevice()->GetDeviceRemovedReason());
        }
        else {
            throw GfxExcept(hr);
        }
    }

    fence_values_[cur_backbuffer_index_] = command_queue->Signal();
    cur_backbuffer_index_ = swap_chain_->GetCurrentBackBufferIndex();
    auto fenceValue = fence_values_[cur_backbuffer_index_];
    command_queue->WaitForFenceValue(fenceValue);

    render_target_->AttachColor(AttachmentPoint::kColor0, back_buffer_textures_[cur_backbuffer_index_]);
}

std::shared_ptr<RenderTarget>& D3D12SwapChain::GetRenderTarget() {
    return render_target_;
}

}
}