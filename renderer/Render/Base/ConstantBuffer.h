#pragma once

#include <memory>
#include "resource.h"
#include "render/base/bufferlayout.h"

namespace glacier {
namespace render {

class ConstantBuffer : public Resource {
public:
    ConstantBuffer(size_t size) : size_((uint32_t)size) {}

    ConstantBuffer(std::shared_ptr<BufferData>& data) :
        size_((uint32_t)data->data_size()),
        version_(data->version()),
        data_(data)
    {}

    virtual void Update(const void* data) = 0;
    virtual void Bind(ShaderType shader_type, uint16_t slot) = 0;
    virtual void UnBind(ShaderType shader_type, uint16_t slot) = 0;

protected:
    uint32_t size_;
    uint32_t version_ = 0;
    std::shared_ptr<BufferData> data_;
};

}
}
