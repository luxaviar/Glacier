#pragma once

#include <vector>
#include "resource.h"

namespace glacier {
namespace render {

class IndexBuffer : public Resource {
public:
    IndexBuffer(size_t size, IndexType type = IndexType::kUInt32) :
        type_(type), size_((uint32_t)size) 
    {}

    IndexBuffer(const std::vector<uint32_t>& indices) :
        IndexBuffer(indices.size()) 
    {}

    IndexBuffer(const std::vector<uint16_t>& indices) : 
        IndexBuffer(indices.size(), IndexType::kUInt16) 
    {}

    virtual void Bind() = 0;
    virtual void UnBind() = 0;

protected:
    IndexType type_;
    uint32_t size_;
};

}
}
