#pragma once

#include "Resource.h"
#include "RasterState.h"

namespace glacier {
namespace render {

class PipelineState : public Resource {
public:
    PipelineState(const RasterState& rs) : state_(rs) {}
    virtual void Bind() = 0;

    const RasterState state() const { return state_; }

protected:
    RasterState state_;
};

}
}
