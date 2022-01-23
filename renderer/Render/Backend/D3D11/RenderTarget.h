#pragma once

#include <d3d11_1.h>
#include <array>
#include "render/base/rendertarget.h"

namespace glacier {
namespace render {

class D3D11RenderTarget : public RenderTarget {
public:
    D3D11RenderTarget(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height) override;

    void Bind(GfxDriver* gfx) override;
    void UnBind(GfxDriver* gfx) override;
    void BindDepthStencil(GfxDriver* gfx) override;

    void Clear(const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 1.0f, uint8_t stencil = 0u) override;
    void ClearColor(AttachmentPoint point = AttachmentPoint::kColor0, const Color& color = { 0.0f,0.0f,0.0f,0.0f }) override;
    void ClearDepthStencil(float depth = 1.0f, uint8_t stencil = 0u) override;

    ViewPort viewport() const override;
    void viewport(const ViewPort& vp) override;

    void EnableScissor(const ScissorRect& rect) override;
    void DisableScissor() override;

    void AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& tex, int16_t slice = -1, int16_t mip_slice = 0, bool srgb = false) override;
    void AttachDepthStencil(const std::shared_ptr<Texture>& tex) override;

    void DetachColor(AttachmentPoint point) override;
    void DetachDepthStencil() override;

private:
    D3D11_VIEWPORT viewport_;
    D3D11_RECT scissor_rect_ = { 0, 0, 0, 0 };

    std::array<ComPtr<ID3D11RenderTargetView>, (int)AttachmentPoint::kNumAttachmentPoints - 1> color_views_;
    ComPtr<ID3D11DepthStencilView> depth_stencil_view_;
};

}
}
