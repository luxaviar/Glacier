#pragma once

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include "Common/Uncopyable.h"
#include "Render/Base/SwapChain.h"

namespace glacier {
namespace render {

class D3D11GfxDriver;

class D3D11SwapChain : public SwapChain {
public:
    D3D11SwapChain(D3D11GfxDriver* driver, HWND hWnd, uint32_t width, uint32_t height, TextureFormat format);
    ~D3D11SwapChain();

    IDXGISwapChain1* GetSwapChain() { return swap_chain_.Get(); }
    ComPtr<ID3D11Texture2D>& GetBackBuffer() { return back_buffer_; }

    std::shared_ptr<RenderTarget>& GetRenderTarget() override;
    GfxDriver* GetDriver() const override;

    void OnResize(uint32_t width, uint32_t height) override;
    void Wait() override;

    void Present() override;

    void CreateRenderTarget();

private:
    //bool CheckTearingSupport();
    D3D11GfxDriver* driver_;
    ComPtr<IDXGISwapChain1> swap_chain_;
    ComPtr<ID3D11Texture2D> back_buffer_;

    std::shared_ptr<RenderTarget> render_target_;
};

}
}
