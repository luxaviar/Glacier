#pragma once

#include "Resource.h"
#include "RasterStateDesc.h"
#include "InputLayout.h"
#include "Common/Util.h"

namespace glacier {
namespace render {

class Program;
class CommandBuffer;

class PipelineState : public Resource {
public:
    PipelineState(Program* program, const RasterStateDesc& rs, const InputLayoutDesc& layout) :
        program_(program),
        state_(rs),
        layout_signature_(layout.signature())
    {}

    virtual void SetInputLayout(const InputLayoutDesc& layout) = 0;
    virtual void SetRasterState(const RasterStateDesc& rs) = 0;

    virtual void Bind(CommandBuffer* cmd_buffer) = 0;
    virtual void SetName(const TCHAR* name) = 0;

    const RasterStateDesc state() const { return state_; }
    const InputLayoutDesc::Signature& layout_signature() const { return layout_signature_; }

protected:
    EngineString name_;
    Program* program_ = nullptr;
    RasterStateDesc state_;
    InputLayoutDesc::Signature layout_signature_;
};

}
}
