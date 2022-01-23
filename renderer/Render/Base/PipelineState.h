#pragma once

#include "Resource.h"
#include "RasterStateDesc.h"
#include "InputLayout.h"

namespace glacier {
namespace render {

class PipelineState : public Resource {
public:
    PipelineState(const RasterStateDesc& rs, const InputLayoutDesc& layout) :
        state_(rs), layout_signature_(layout.signature())
    {}

    virtual void SetInputLayout(const InputLayoutDesc& layout) = 0;
    virtual void SetRasterState(const RasterStateDesc& rs) = 0;

    virtual void Bind(GfxDriver* gfx) = 0;

    const RasterStateDesc state() const { return state_; }
    const InputLayoutDesc::Signature& layout_signature() const { return layout_signature_; }

protected:
    RasterStateDesc state_;
    InputLayoutDesc::Signature layout_signature_;
};

}
}
