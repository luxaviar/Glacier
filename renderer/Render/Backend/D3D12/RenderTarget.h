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

class D3D12RenderTarget : public RenderTarget, public std::enable_shared_from_this<D3D12RenderTarget> {
public:
    D3D12RenderTarget(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height) override;

    D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;
    DXGI_FORMAT GetDepthStencilFormat() const;
    DXGI_SAMPLE_DESC GetSampleDesc() const;

    void Bind(CommandBuffer* cmd_buffer) override;
    void BindDepthStencil(CommandBuffer* cmd_buffer) override;
    void BindColor(CommandBuffer* cmd_buffer) override;
    void UnBind(CommandBuffer* cmd_buffer) override;

#ifdef GLACIER_REVERSE_Z
    void Clear(CommandBuffer* cmd_buffer, const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 0.0f, uint8_t stencil = 0u) override;
    void ClearDepthStencil(CommandBuffer* cmd_buffer, float depth = 0.0f, uint8_t stencil = 0u) override;
#else
    void Clear(CommandBuffer* cmd_buffer, const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 1.0f, uint8_t stencil = 0u) override;
    void ClearDepthStencil(CommandBuffer* cmd_buffer, float depth = 1.0f, uint8_t stencil = 0u) override;
#endif
    void ClearColor(CommandBuffer* cmd_buffer, AttachmentPoint point = AttachmentPoint::kColor0, const Color& color = { 0.0f,0.0f,0.0f,0.0f }) override;
    
    void AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& tex, int16_t slice = -1, int16_t mip_slice = 0, bool srgb = false) override;
    void AttachDepthStencil(const std::shared_ptr<Texture>& tex) override;

    void DetachColor(AttachmentPoint point) override;
    void DetachDepthStencil() override;

    ViewPort viewport() const override;
    void viewport(const ViewPort& vp) override;

    void EnableScissor(const ScissorRect& rect) override;
    void DisableScissor() override;

private:

    D3D12Texture* GetAttachementTexture(AttachmentPoint point) const;
    D3D12Texture* GetAttachementTexture(size_t i) const;

    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissor_rect_ = { 0, 0, LONG_MAX, LONG_MAX };

    std::array<D3D12DescriptorRange, (int)AttachmentPoint::kNumAttachmentPoints - 1> rtv_slots_;
    D3D12DescriptorRange dsv_slot_;
};

}
}
