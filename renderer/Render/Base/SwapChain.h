#pragma once

//#include <d3d12.h>
//#include <dxgi1_6.h>
#include "Common/Uncopyable.h"

namespace glacier {
namespace render {

//class DX12Texture;
class RenderTarget;
class GfxDriver;

class SwapChain : private Uncopyable {
public:
    SwapChain(uint32_t width, uint32_t height) :
        width_(width),
        height_(height)
    {
    }

    virtual ~SwapChain() {}

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

    virtual GfxDriver* GetDriver() const = 0;
    virtual std::shared_ptr<RenderTarget>& GetRenderTarget() = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;

    virtual void Wait() = 0;
    virtual void Present() = 0;

protected:
    uint32_t width_;
    uint32_t height_;

    bool full_screen_ = false;
    bool vsync_ = false;
    bool tearing_support_ = false;
};

}
}
