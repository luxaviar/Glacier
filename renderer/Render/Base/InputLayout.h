#pragma once

#include "resource.h"
#include "vertexlayout.h"

namespace glacier {
namespace render {

class GfxDriver;

class InputLayout : public Resource {
public:
    InputLayout(const InputLayoutDesc& layout) : layout_desc_(layout) {}
    
    virtual void Bind(GfxDriver* gfx) const = 0;

    const InputLayoutDesc& layout() const noexcept { return layout_desc_; }
    uint32_t signature() const { return layout_desc_.signature(); }

protected:
    InputLayoutDesc layout_desc_;
};

}
}
