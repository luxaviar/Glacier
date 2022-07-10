#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include "Common/Uncopyable.h"
#include "Render/Base/SwapChain.h"
#include "Util.h"

namespace glacier {
namespace render {

class D3D12Texture;
class D3D12RenderTarget;
class D3D12GfxDriver;

class D3D12SwapChain : public  SwapChain {
public:
    constexpr static uint32_t kBufferCount = 3;

    D3D12SwapChain(D3D12GfxDriver* driver, HWND hWnd, uint32_t width, uint32_t height, TextureFormat format);
    ~D3D12SwapChain();

    void CreateRenderTarget();

    IDXGISwapChain4* GetSwapChain() { return swap_chain_.Get(); }
    ID3D12Resource* GetBackBuffer() { return back_buffer_.Get(); }

    std::shared_ptr<RenderTarget>& GetRenderTarget() override;
    DXGI_FORMAT GetNativeFormat() const { return GetUnderlyingFormat(format_); }

    GfxDriver* GetDriver() const override { return nullptr; }
    void OnResize(uint32_t width, uint32_t height) override;

    void Wait() override;
    void Present(std::vector<CommandBuffer*>& cmd_buffers) override;

private:
    bool CheckTearingSupport();

    D3D12GfxDriver* driver_;
    ComPtr<IDXGISwapChain4> swap_chain_;
    ComPtr<ID3D12Resource> back_buffer_;

    // A handle to a waitable object. Used to wait for the swapchain before presenting.
    HANDLE frame_latency_waitable_object_;
    uint32_t cur_backbuffer_index_ = 0;

    UINT64 fence_values_[kBufferCount];
    std::shared_ptr<D3D12Texture> back_buffer_textures_[kBufferCount];
    std::shared_ptr<RenderTarget> render_target_;
};

}
}
