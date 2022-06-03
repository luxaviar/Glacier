#pragma once

#include <vector>
#include <array>
#include "texture.h"
#include "Common/TypeTraits.h"
#include "Common/Signal.h"
#include "resource.h"

namespace glacier {
namespace render {

struct ViewPort {
    float top_left_x;
    float top_left_y;
    float width;
    float height;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};

struct ScissorRect {
    long left;
    long top;
    long right;
    long bottom;
};

class GfxDriver;

struct AttachmentTexture {
    std::shared_ptr<Texture> texture;
    int16_t slice = -1;
    int16_t mip_slice = 0;
    bool srgb = false;

    operator bool() const {
        return !!texture;
    }
};

class RenderTarget : public Resource {
public:
    using UpdateSignal = Signal<>;

    RenderTarget(uint32_t width, uint32_t height) : width_(width), height_(height)
    {
    }

    virtual void Resize(uint32_t width, uint32_t height) = 0;
    
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

    int target_num() const noexcept {
        int count = 0;
        for (int i = 0; i < (int)AttachmentPoint::kDepthStencil; ++i) {
            if (attachments_[i]) {
                ++count;
            }
        }
        return count;
    }

    std::shared_ptr<Texture>& GetColorAttachment(AttachmentPoint point) { return attachments_[toUType(point)].texture; }
    std::shared_ptr<Texture>& GetDepthStencil() { return  attachments_[(int)AttachmentPoint::kDepthStencil].texture; }

    virtual void Bind(GfxDriver* gfx) = 0;
    virtual void UnBind(GfxDriver* gfx) = 0;
    virtual void BindDepthStencil(GfxDriver* gfx) = 0;
    virtual void BindColor(GfxDriver* gfx) = 0;

#ifdef GLACIER_REVERSE_Z
    virtual void Clear(const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 0.0f, uint8_t stencil = 0u) = 0;
    virtual void ClearDepthStencil(float depth = 0.0f, uint8_t stencil = 0u) = 0;
#else
    virtual void Clear(const Color& color = { 0.0f,0.0f,0.0f,0.0f }, float depth = 1.0f, uint8_t stencil = 0u) = 0;
    virtual void ClearDepthStencil(float depth = 1.0f, uint8_t stencil = 0u) = 0;
#endif
    virtual void ClearColor(AttachmentPoint point = AttachmentPoint::kColor0, const Color& color = { 0.0f,0.0f,0.0f,0.0f }) = 0;

    virtual ViewPort viewport() const = 0;
    virtual void viewport(const ViewPort& vp) = 0;

    virtual void EnableScissor(const ScissorRect& rect) = 0;
    virtual void DisableScissor() = 0;

    virtual void AttachColor(AttachmentPoint point, const std::shared_ptr<Texture>& tex, int16_t slice=-1, int16_t mip_slice=0, bool srgb=false) = 0;
    virtual void AttachDepthStencil(const std::shared_ptr<Texture>& tex) = 0;

    virtual void DetachColor(AttachmentPoint point) = 0;
    virtual void DetachDepthStencil() = 0;

    UpdateSignal& signal() { return signal_; }

protected:
    bool scissor_enable_ = false;

    uint32_t width_;
    uint32_t height_;

    std::array<AttachmentTexture, (size_t)AttachmentPoint::kNumAttachmentPoints> attachments_;

    UpdateSignal signal_;
};

using RenderTargetBindingGuard = BindingGuard<RenderTarget>;

}
}
