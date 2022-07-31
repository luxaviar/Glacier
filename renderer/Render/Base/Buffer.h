#pragma once

#include <memory>
//#include <cstdlib>
#include "Resource.h"
#include "Enums.h"
//#include "BufferLayout.h"

namespace glacier {
namespace render {

class CommandBuffer;

class Buffer : public Resource {
public:
    Buffer(BufferType type, size_t size, size_t stride = 0, UsageType usage = UsageType::kDefault) :
        type_(type),
        usage_(usage),
        size_(size),
        stride_(stride)
    {
    }

    BufferType type() const { return type_; }
    UsageType usage() const { return usage_; }

    size_t size() const { return size_; }
    size_t stride() const { return stride_; }

    size_t count() const { return count_; }

    bool IsDynamic() const { return usage_ == UsageType::kDynamic; }

    virtual void Bind(CommandBuffer* cmd_buffer) = 0;

    virtual void Upload(CommandBuffer* cmd_buffer, const void* data, size_t size) = 0;
    virtual void Update(const void* data, size_t size) = 0;

    void Update(const void* data) { Update(data, size_); }

protected:
    BufferType type_;
    UsageType usage_;
    size_t size_;
    size_t stride_; //for vertex/index/structure
    size_t count_ = 0; //for index
};

template<typename T>
class ConstantParameter {
public:
    ConstantParameter() {}
    ConstantParameter(std::shared_ptr<Buffer>& buf) : buffer_(buf) {}
    ConstantParameter(std::shared_ptr<Buffer>& buf, const T& v) : buffer_(buf), param_(v) {}

    ConstantParameter<T>& operator=(const T& v) {
        param_ = v;
        buffer_->Update(&param_);
        return *this;
    }

    T& param() { return param_; }
    const T& param() const { return param_; }
    std::shared_ptr<Buffer>& buffer() { return buffer_; }

    void Update() { buffer_->Update(&param_); }

private:
    std::shared_ptr<Buffer> buffer_;
    T param_;
};

}
}
