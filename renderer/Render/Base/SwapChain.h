#pragma once

#include "Common/Uncopyable.h"
#include "Enums.h"

namespace glacier {
namespace render {

class RenderTarget;
class GfxDriver;

class SwapChain : private Uncopyable {
public:
    SwapChain(uint32_t width, uint32_t height, TextureFormat format) :
        width_(width),
        height_(height),
        format_(format)
    {
    }

    virtual ~SwapChain() {}

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    TextureFormat GetFormat() const { return format_; }

    virtual GfxDriver* GetDriver() const = 0;
    virtual std::shared_ptr<RenderTarget>& GetRenderTarget() = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;

    virtual void Wait() = 0;
    virtual void Present() = 0;

protected:
    uint32_t width_;
    uint32_t height_;
    TextureFormat format_;

    bool full_screen_ = false;
    bool vsync_ = false;
    bool tearing_support_ = false;
};

}
}
