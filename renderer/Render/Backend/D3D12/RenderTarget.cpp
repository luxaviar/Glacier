#include "RenderTarget.h"
#include "Texture.h"
#include "Math/Util.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

std::shared_ptr<D3D12RenderTarget> D3D12RenderTarget::Create(uint32_t width, uint32_t height) {
    return std::shared_ptr<D3D12RenderTarget>(new D3D12RenderTarget(width, height));
}

D3D12RenderTarget::D3D12RenderTarget(uint32_t width, uint32_t height) :
    RenderTarget(width, height)
{
    viewport_.Width = (float)width_;
    viewport_.Height = (float)height_;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
}

void D3D12RenderTarget::AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& texture, int16_t slice, int16_t mip_slice, bool srgb)
{
    assert(texture);
    assert(width_ <= texture->width());
    assert(height_ <= texture->height());

    int idx = (int)point;
    attachments_[idx] = { texture, slice, mip_slice, srgb };

    auto tex = static_cast<D3D12Texture*>(texture.get());
    auto tex_desc = tex->GetDesc();

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    //TODO: read format from attachment
    rtv_desc.Format = srgb ? GetSRGBFormat(tex_desc.Format) : tex_desc.Format;
    if (slice >= 0) {
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.MipSlice = mip_slice;
        rtv_desc.Texture2DArray.FirstArraySlice = slice;
        rtv_desc.Texture2DArray.ArraySize = 1;
        rtv_desc.Texture2DArray.PlaneSlice = 0;
    }
    else {
        rtv_desc.ViewDimension = tex_desc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D = D3D12_TEX2D_RTV{ 0 };
    }

    auto driver = D3D12GfxDriver::Instance();
    auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto slot = descriptor_allocator->Allocate();
    driver->GetDevice()->CreateRenderTargetView(tex->GetUnderlyingResource().Get(), &rtv_desc, slot.GetDescriptorHandle());

    rtv_slots_[idx] = std::move(slot);
}

void D3D12RenderTarget::AttachDepthStencil(const std::shared_ptr<Texture>& texture) {
    assert(texture);
    assert(width_ <= texture->width());
    assert(height_ <= texture->height());

    attachments_[(int)AttachmentPoint::kDepthStencil].texture = texture;

    auto tex = static_cast<D3D12Texture*>(texture.get());
    auto tex_desc = tex->GetDesc();

    // create target view of depth stencil attachment
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = GetDSVFormat(tex_desc.Format);
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
    if (tex_desc.SampleDesc.Count > 1) {
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }
    else {
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    }

    auto driver = D3D12GfxDriver::Instance();
    auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    auto slot = descriptor_allocator->Allocate();
    driver->GetDevice()->CreateDepthStencilView(tex->GetUnderlyingResource().Get(), &dsv_desc, slot.GetDescriptorHandle());

    dsv_slot_ = std::move(slot);
}

void D3D12RenderTarget::DetachColor(AttachmentPoint point) {
    int idx = (int)point;
    if (attachments_.size() <= idx) return;

    attachments_[idx] = {};
    rtv_slots_[idx] = {};
}

void D3D12RenderTarget::DetachDepthStencil() {
    dsv_slot_ = {};
    attachments_[(int)AttachmentPoint::kDepthStencil] = {};
}

void D3D12RenderTarget::Clear(const Color& color, float depth, uint8_t stencil) {
    auto command_list = D3D12GfxDriver::Instance()->GetCommandList();

    for (size_t i = 0; i < (size_t)AttachmentPoint::kDepthStencil; ++i) {
        auto texture = GetAttachementTexture(i);
        if (texture) {
            command_list->TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->ClearRenderTargetView(rtv_slots_[i], &color, 0, nullptr);
        }
    }

    auto texture = GetAttachementTexture(AttachmentPoint::kDepthStencil);
    if (texture) {
        command_list->TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command_list->ClearDepthStencilView(dsv_slot_, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
    }
}

void D3D12RenderTarget::ClearColor(AttachmentPoint point, const Color& color) {
    auto tex = GetAttachementTexture(point);
    if (tex) {
        auto command_list = D3D12GfxDriver::Instance()->GetCommandList();
        command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list->ClearRenderTargetView(rtv_slots_[(int)point], &color, 0, nullptr);
    }
}

void D3D12RenderTarget::ClearDepthStencil(float depth, uint8_t stencil) {
    auto tex = GetAttachementTexture(AttachmentPoint::kDepthStencil);
    if (tex) {
        auto command_list = D3D12GfxDriver::Instance()->GetCommandList();
        command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command_list->ClearDepthStencilView(dsv_slot_, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
    }
}

void D3D12RenderTarget::Bind(GfxDriver* gfx) {
    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto command_list = driver->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_descriptor[(size_t)AttachmentPoint::kDepthStencil];

    uint32_t rtv_count = 0;
    // Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
    for (size_t i = 0; i < (size_t)AttachmentPoint::kDepthStencil; ++i) {
        auto tex = GetAttachementTexture(i);
        if (tex) {
            command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtv_descriptor[rtv_count++] = rtv_slots_[i].GetDescriptorHandle();
        }
    }

    auto tex = GetAttachementTexture(AttachmentPoint::kDepthStencil);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_descriptor(D3D12_DEFAULT);
    if (tex) {
        command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        dsv_descriptor = dsv_slot_.GetDescriptorHandle();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* dsv_handle = dsv_descriptor.ptr != 0 ? &dsv_descriptor : nullptr;

    GfxThrowIfAny(command_list->OMSetRenderTargets(rtv_count, rtv_descriptor, FALSE, dsv_handle));
    GfxThrowIfAny(command_list->RSSetViewports(1, &viewport_));
    GfxThrowIfAny(command_list->RSSetScissorRects(1, &scissor_rect_));

    driver->SetCurrentRenderTarget(shared_from_this());
}

void D3D12RenderTarget::UnBind(GfxDriver* gfx) {
    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto command_list = driver->GetCommandList();

    driver->SetCurrentRenderTarget(nullptr);
}

void D3D12RenderTarget::BindColor(GfxDriver* gfx) {
    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto command_list = driver->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_descriptor[(size_t)AttachmentPoint::kDepthStencil];

    uint32_t rtv_count = 0;
    // Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
    for (size_t i = 0; i < (size_t)AttachmentPoint::kDepthStencil; ++i) {
        auto tex = GetAttachementTexture(i);
        if (tex) {
            command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtv_descriptor[rtv_count++] = rtv_slots_[i].GetDescriptorHandle();
        }
    }

    GfxThrowIfAny(command_list->OMSetRenderTargets(rtv_count, rtv_descriptor, FALSE, nullptr));
    GfxThrowIfAny(command_list->RSSetViewports(1, &viewport_));
    GfxThrowIfAny(command_list->RSSetScissorRects(1, &scissor_rect_));

    driver->SetCurrentRenderTarget(shared_from_this());
}

void D3D12RenderTarget::BindDepthStencil(GfxDriver* gfx) {
    auto tex = GetAttachementTexture(AttachmentPoint::kDepthStencil);
    if (!tex) return;

    auto command_list = static_cast<D3D12GfxDriver*>(gfx)->GetCommandList();
    command_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    auto dsv_descriptor = dsv_slot_.GetDescriptorHandle();

    D3D12_CPU_DESCRIPTOR_HANDLE* dsv_handle = dsv_descriptor.ptr != 0 ? &dsv_descriptor : nullptr;

    GfxThrowIfAny(command_list->OMSetRenderTargets(0, nullptr, FALSE, dsv_handle));
    GfxThrowIfAny(command_list->RSSetViewports(1, &viewport_));
    GfxThrowIfAny(command_list->RSSetScissorRects(1, &scissor_rect_));
}

void D3D12RenderTarget::Resize(uint32_t width, uint32_t height) {
    if (width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;

    viewport_.Width = (float)width_;
    viewport_.Height = (float)height_;

    for (int i = 0; i < (int)AttachmentPoint::kDepthStencil; ++i) {
        auto& attachment = attachments_[i];
        if (attachment) {
            attachment.texture->Resize(width, height);
            AttachColor((AttachmentPoint)i, attachment.texture, attachment.slice, attachment.mip_slice, attachment.srgb);
        }
    }

    auto deptch_attachment = attachments_[(int)AttachmentPoint::kDepthStencil];
    if (deptch_attachment) {
        deptch_attachment.texture->Resize(width, height);
        AttachDepthStencil(deptch_attachment.texture);
    }
}

D3D12_RT_FORMAT_ARRAY D3D12RenderTarget::GetRenderTargetFormats() const {
    D3D12_RT_FORMAT_ARRAY rtv_formats = {};

    for (int i = (int)AttachmentPoint::kColor0; i <= (int)AttachmentPoint::kColor7; ++i) {
        auto tex = GetAttachementTexture(i);
        if (tex) {
            rtv_formats.RTFormats[rtv_formats.NumRenderTargets++] = GetUnderlyingFormat(tex->GetFormat());
        }
    }

    return rtv_formats;
}

DXGI_FORMAT D3D12RenderTarget::GetDepthStencilFormat() const {
    auto tex = GetAttachementTexture(AttachmentPoint::kDepthStencil);
    if (tex) {
        return GetDSVFormat(GetUnderlyingFormat(tex->GetFormat()));
    }

    return DXGI_FORMAT_UNKNOWN;
}

DXGI_SAMPLE_DESC D3D12RenderTarget::GetSampleDesc() const {
    auto tex = attachments_[(int)AttachmentPoint::kColor0].texture;
    if (!tex) {
        tex = attachments_[(int)AttachmentPoint::kDepthStencil].texture;
    }

    assert(tex);
    return DXGI_SAMPLE_DESC{ tex->GetSampleCount(), tex->GetSampleQuality() };
}


ViewPort D3D12RenderTarget::viewport() const {
    return { viewport_.TopLeftX, viewport_.TopLeftY, viewport_.Width, viewport_.Height, viewport_.MinDepth, viewport_.MaxDepth };
}

void D3D12RenderTarget::viewport(const ViewPort& vp) {
    viewport_ = { vp.top_left_x, vp.top_left_y, vp.width, vp.height, vp.min_depth, vp.max_depth };
}

void D3D12RenderTarget::EnableScissor(const ScissorRect& rect) {
    scissor_enable_ = true;
    scissor_rect_ = { rect.left, rect.top, rect.right, rect.bottom };
}

void D3D12RenderTarget::DisableScissor() {
    scissor_enable_ = false;
    scissor_rect_ = { 0, 0, LONG_MAX, LONG_MAX };
}

D3D12Texture* D3D12RenderTarget::GetAttachementTexture(AttachmentPoint point) const {
    return GetAttachementTexture((size_t)point);
}

D3D12Texture* D3D12RenderTarget::GetAttachementTexture(size_t i) const {
    auto tex = attachments_[i].texture;
    if (!tex) {
        return nullptr;
    }

    return static_cast<D3D12Texture*>(tex.get());
}


}
}
