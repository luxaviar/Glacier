#pragma once

#include "resource.h"

namespace glacier {
namespace render {

class VertexBuffer : public Resource {
public:
    VertexBuffer(size_t size, size_t stride) : size_((uint32_t)size), stride_((uint32_t)stride) {}

    virtual void Update(const void* data) = 0;

    virtual void Bind() = 0;
    virtual void UnBind() = 0;

    virtual void Bind(uint32_t slot, uint32_t offset) = 0;
    virtual void UnBind(uint32_t slot, uint32_t offset) = 0;

    auto size() const { return size_; }

protected:
    uint32_t size_;
    uint32_t stride_;
};

}
}
