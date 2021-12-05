#include "rendertarget.h"
#include <array>
#include <stdexcept>
#include "render/image.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

D3D11RenderTarget::D3D11RenderTarget(uint32_t width, uint32_t height) :
    RenderTarget(width, height)
{
    colors_.resize((size_t)AttachmentPoint::kNumAttachmentPoints - 1);
    color_views_.resize((size_t)AttachmentPoint::kNumAttachmentPoints - 1);

    viewport_.Width = (float)width_;
    viewport_.Height = (float)height_;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
}

void D3D11RenderTarget::Resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;

    viewport_.Width = (float)width_;
    viewport_.Height = (float)height_;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
}

void D3D11RenderTarget::AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& tex, int16_t slice, int16_t mip_slice, bool srgb) {
    assert(height_ <= tex->height());
    assert(width_ <= tex->width());
    
    int idx = (int)point;
    colors_[idx] = tex;

    D3D11_TEXTURE2D_DESC tex_desc;
    auto tex_ptr = static_cast<ID3D11Texture2D*>(tex->underlying_resource());
    tex_ptr->GetDesc(&tex_desc);
    
    // create the target view on the texture
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    //TODO: read format from texture
    rtv_desc.Format = srgb ? GetSRGBFormat(tex_desc.Format) : tex_desc.Format;
    if (slice >= 0) {
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.MipSlice = mip_slice;
        rtv_desc.Texture2DArray.FirstArraySlice = slice;
        rtv_desc.Texture2DArray.ArraySize = 1;
    } else {
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D = D3D11_TEX2D_RTV{ 0 };
    }

    auto& view = color_views_[idx];
    GfxThrowIfFailed(GfxDriverD3D11::Instance()->GetDevice()->CreateRenderTargetView(
        tex_ptr, &rtv_desc, &view
    ));
}

void D3D11RenderTarget::AttachDepthStencil(const std::shared_ptr<Texture>& tex) {
    assert(height_ <= tex->height());
    assert(width_ <= tex->width());

    depth_stencil_ = tex;

    D3D11_TEXTURE2D_DESC tex_desc;
    auto tex_ptr = static_cast<ID3D11Texture2D*>(tex->underlying_resource());
    tex_ptr->GetDesc(&tex_desc);

    // create target view of depth stencil texture
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = GetDSVFormat(tex_desc.Format);
    dsv_desc.Flags = 0;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = 0;
    GfxThrowIfFailed(GfxDriverD3D11::Instance()->GetDevice()->CreateDepthStencilView(
        tex_ptr, &dsv_desc, &depth_stencil_view_
    ));
}

void D3D11RenderTarget::DetachColor(AttachmentPoint point) {
    int idx = (int)point;
    if (colors_.size() <= idx) return;

    colors_[idx].reset();
    color_views_[idx].Reset();
}

void D3D11RenderTarget::DetachDepthStencil() {
    depth_stencil_view_.Reset();
    depth_stencil_.reset();
}

void D3D11RenderTarget::Bind() {
    constexpr int kMaxTarget = (int)AttachmentPoint::kNumAttachmentPoints - 1;
    UINT rtv_num = 0;
    ID3D11RenderTargetView* rt_views[kMaxTarget];

    for (int i = 0; i < kMaxTarget; ++i) {
        auto& rtv = color_views_[i];
        if (rtv) {
            rt_views[rtv_num++] = rtv.Get();
        }
    }

    auto context = GfxDriverD3D11::Instance()->GetContext();

    ID3D11UnorderedAccessView* null_uav = nullptr;
    GfxThrowIfAny(context->OMSetRenderTargetsAndUnorderedAccessViews(
        rtv_num, rt_views, depth_stencil_view_.Get(), 0, 0, &null_uav, nullptr));

    GfxThrowIfAny(context->RSSetViewports(1u, &viewport_));
    
    if (scissor_enable_) {
        GfxThrowIfAny(context->RSSetScissorRects(1u, &scissor_rect_));
    }
}

void D3D11RenderTarget::BindDepthStencil() {
    ID3D11RenderTargetView* null_rtv = nullptr;
    auto context = GfxDriverD3D11::Instance()->GetContext();

    GfxThrowIfAny(context->OMSetRenderTargets(1, &null_rtv, depth_stencil_view_.Get()));

    GfxThrowIfAny(context->RSSetViewports(1u, &viewport_));

    if (scissor_enable_) {
        GfxThrowIfAny(context->RSSetScissorRects(1u, &scissor_rect_));
    }
}

void D3D11RenderTarget::UnBind() {
    ID3D11RenderTargetView* null_rtv = nullptr;
    ID3D11UnorderedAccessView* null_uav = nullptr;
    auto context = GfxDriverD3D11::Instance()->GetContext();
    GfxThrowIfAny(context->OMSetRenderTargetsAndUnorderedAccessViews(
        0, &null_rtv, nullptr, 0, 0, &null_uav, nullptr));
}

void D3D11RenderTarget::Clear(const Color& color, float depth, uint8_t stencil) {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    for (int i = 0; i < (int)AttachmentPoint::kNumAttachmentPoints - 1; ++i) {
        auto& rtv = color_views_[i];
        if (rtv) {
            GfxThrowIfAny(context->ClearRenderTargetView(rtv.Get(), &color));
        }
    }

    if (depth_stencil_view_) {
        GfxThrowIfAny(context->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil));
    }
}

void D3D11RenderTarget::ClearColor(AttachmentPoint point, const Color& color) {
    assert((int)point < (int)AttachmentPoint::kNumAttachmentPoints - 1);

    auto& rt_view = color_views_[(int)point];
    if (!rt_view) return;

    GfxThrowIfAny(GfxDriverD3D11::Instance()->GetContext()->ClearRenderTargetView(rt_view.Get(), &color));
}

void D3D11RenderTarget::ClearDepthStencil(float depth, uint8_t stencil) {
    if (!depth_stencil_view_) return;
    GfxThrowIfAny(GfxDriverD3D11::Instance()->GetContext()->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil));
}


ViewPort D3D11RenderTarget::viewport() const {
    return { viewport_.TopLeftX, viewport_.TopLeftY, viewport_.Width, viewport_.Height, viewport_.MinDepth, viewport_.MaxDepth };
}

void D3D11RenderTarget::viewport(const ViewPort& vp) {
    viewport_ = { vp.top_left_x, vp.top_left_y, vp.width, vp.height, vp.min_depth, vp.max_depth };
}

void D3D11RenderTarget::EnableScissor(const ScissorRect& rect) {
    scissor_enable_ = true;
    scissor_rect_ = { rect.left, rect.top, rect.right, rect.bottom };
}

void D3D11RenderTarget::DisableScissor() {
    scissor_enable_ = false;
}

//ID3D11Texture2D* RenderTarget::underlying_texture(AttachmentPoint point) const {
//    assert(point < AttachmentPoint::kNumAttachmentColor);
//    auto& target_view = color_views_[(int)point];
//    if (!target_view) 
//        return nullptr;
//
//    ComPtr<ID3D11Resource> res_ptr;
//    target_view->GetResource(&res_ptr);
//    if (!res_ptr)
//        return nullptr;
//
//    D3D11_RESOURCE_DIMENSION res_type;
//    res_ptr->GetType(&res_type);
//    if (res_type != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
//        return nullptr;
//
//    ID3D11Texture2D* tex_ptr = static_cast<ID3D11Texture2D*>(res_ptr.Get());
//    return tex_ptr;
//}

//ID3D11Texture2D* RenderTarget::underlying_depthstencil() const {
//    ComPtr<ID3D11Resource> res_ptr = nullptr;
//    if (!depth_stencil_view_)
//        return nullptr;
//
//    depth_stencil_view_->GetResource(&res_ptr);
//    if (!res_ptr)
//        return nullptr;
//
//    D3D11_RESOURCE_DIMENSION res_type;
//    res_ptr->GetType(&res_type);
//    if (res_type != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
//        return nullptr;
//
//    ID3D11Texture2D* tex_ptr = static_cast<ID3D11Texture2D*>(res_ptr.Get());
//    return tex_ptr;
//}

}

}
