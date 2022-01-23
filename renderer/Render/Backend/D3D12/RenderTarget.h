#pragma once

#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include "math/Vec2.h"
#include "Render/Base/Enums.h"
#include "Render/Base/RenderTarget.h"
#include "DescriptorHeapAllocator.h"

namespace glacier {
namespace render {

class D3D12Texture;

class D3D12RenderTarget : public RenderTarget {
public:
    D3D12RenderTarget(uint32_t width, uint32_t height);

    void Resize(uint32_t width, uint32_t height) override;

    D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;
    DXGI_FORMAT GetDepthStencilFormat() const;
    DXGI_SAMPLE_DESC GetSampleDesc() const;

    void Bind(GfxDriver* gfx) override;
    void UnBind(GfxDriver* gfx) override;
    void BindDepthStencil(GfxDriver* gfx) override;

    void Clear(const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 1.0f, uint8_t stencil = 0u) override;
    void ClearColor(AttachmentPoint point = AttachmentPoint::kColor0, const Color& color = { 0.0f,0.0f,0.0f,0.0f }) override;
    void ClearDepthStencil(float depth = 1.0f, uint8_t stencil = 0u) override;

    void AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& tex, int16_t slice = -1, int16_t mip_slice = 0, bool srgb = false) override;
    void AttachDepthStencil(const std::shared_ptr<Texture>& tex) override;

    void DetachColor(AttachmentPoint point) override;
    void DetachDepthStencil() override;

    ViewPort viewport() const override;
    void viewport(const ViewPort& vp) override;

    void EnableScissor(const ScissorRect& rect) override;
    void DisableScissor() override;

private:
    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissor_rect_ = { 0, 0, LONG_MAX, LONG_MAX };

    std::array<D3D12DescriptorRange, (int)AttachmentPoint::kNumAttachmentPoints - 1> rtv_slots_;
    D3D12DescriptorRange dsv_slot_;
};

}
}
