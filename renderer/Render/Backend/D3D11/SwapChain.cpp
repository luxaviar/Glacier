#include "SwapChain.h"
#include "Exception/Exception.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D11SwapChain::D3D11SwapChain(GfxDriverD3D11* driver, HWND hWnd, uint32_t width, uint32_t height) :
    SwapChain(width, height),
    driver_(driver)
{
    auto device = driver->GetDevice();
    auto context = driver->GetContext();

    //ComPtr<IDXGIDevice1> dxgi_device;
    IDXGIDevice1* dxgi_device;
    GfxThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)));// __uuidof(IDXGIDevice1), (void**)&dxgi_device));

    // Identify the physical adapter (GPU or card) this device is running on.
    ComPtr<IDXGIAdapter> dxgi_adapter;
    GfxThrowIfFailed(dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf()));

    // And obtain the factory object that created it.
    ComPtr<IDXGIFactory2> dxgi_factory;
    GfxThrowIfFailed(dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf())));

    // Create a descriptor for the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
    swapchain_desc.Width = width;
    swapchain_desc.Height = height;
    swapchain_desc.Format = format_;
    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = 3;
    swapchain_desc.Flags = 0;
    // DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; DXGI_SWAP_EFFECT_DISCARD;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
    fullscreen_desc.Windowed = TRUE;

    // Create a SwapChain from a Win32 window.
    GfxThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(
        device,
        hWnd,
        &swapchain_desc,
        &fullscreen_desc,
        nullptr,
        &swap_chain_
    ));

    // This template does not support exclusive fullscreen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
    GfxThrowIfFailed(dxgi_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    //In Direct3D 11, applications could call GetBuffer(0, …) only once.
    //Every call to Present implicitly changed the resource identity of the returned interface.
    //Direct3D 12 no longer supports that implicit resource identity change, 
    //due to the CPU overhead requiredand the flexible resource descriptor design.
    //As a result, the application must manually call GetBuffer for every each buffer created with the swapchain.
    GfxThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_)));
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //CreateRenderTarget();
}

D3D11SwapChain::~D3D11SwapChain() {

}

void D3D11SwapChain::CreateRenderTarget() {
    assert(!render_target_);

    render_target_ = std::make_shared<D3D11RenderTarget>(width_, height_);
    auto color_tex_builder = Texture::Builder()
        .SetBackBufferTexture(0);
    auto back_texture = driver_->CreateTexture(color_tex_builder);

    auto depth_tex_builder = Texture::Builder()
        .SetDimension(width_, height_)
        .SetFormat(TextureFormat::kD24S8_UINT)
        .SetCreateFlag(D3D11_BIND_DEPTH_STENCIL);
    auto depthstencil_texture = driver_->CreateTexture(depth_tex_builder);

    render_target_->AttachColor(AttachmentPoint::kColor0, back_texture);
    render_target_->AttachDepthStencil(depthstencil_texture);
}

void D3D11SwapChain::OnResize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;

    //Reset render target
    render_target_->UnBind();
    auto backbuffer_tex = render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    auto depth_tex = render_target_->GetDepthStencil();
    render_target_->DetachColor(AttachmentPoint::kColor0);
    render_target_->DetachDepthStencil();

    backbuffer_tex->ReleaseUnderlyingResource();

    //Resize back buffer
    back_buffer_.Reset();
    DXGI_SWAP_CHAIN_DESC1 desc;
    GfxThrowIfFailed(swap_chain_->GetDesc1(&desc));
    GfxThrowIfFailed(swap_chain_->ResizeBuffers(2, width, height, desc.Format, desc.Flags));
    GfxThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_)));

    //Rebuild render target
    backbuffer_tex->RefreshBackBuffer();
    depth_tex->Resize(width, height);
    render_target_->Resize(width, height);
    render_target_->AttachColor(AttachmentPoint::kColor0, backbuffer_tex);
    render_target_->AttachDepthStencil(depth_tex);
}

void D3D11SwapChain::Wait() {
    
}

void D3D11SwapChain::Present() {
    // The first argument instructs DXGI to block until VSync, putting the application
// to sleep until the next VSync. This ensures we don't waste any cycles rendering
// frames that will never be displayed to the screen.
    if (HRESULT hr; FAILED(hr = swap_chain_->Present(vsync_ ? 1u : 0u, 0u))) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            throw GraphicsDeviceRemovedException(__LINE__, TEXT(__FILE__), driver_->GetDevice()->GetDeviceRemovedReason());
        }
        else {
            throw GfxExcept(hr);
        }
    }
}

GfxDriver* D3D11SwapChain::GetDriver() const {
    return driver_;
}

std::shared_ptr<RenderTarget>& D3D11SwapChain::GetRenderTarget() {
    return render_target_;
}

}
}