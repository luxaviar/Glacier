#pragma once

#include <vector>
#include "resource.h"
#include "BufferLayout.h"

namespace glacier {
namespace render {

class Buffer : public Resource {
public:
    Buffer(BufferType type, size_t size) : type_(type), size_(size) {}
    virtual ~Buffer() {}

    BufferType type() const { return type_; }
    size_t size() const { return size_; }

    virtual void Update(const void* data, size_t size) = 0;
    void Update(const void* data) { Update(data, size_); }

protected:
    BufferType type_;
    size_t size_;
};

class ConstantBuffer : public Buffer {
public:
    ConstantBuffer(size_t size, UsageType usage) : Buffer(BufferType::kConstantBuffer, size), usage_(usage) {}

    bool IsDynamic() const { return usage_ == UsageType::kDynamic; }

protected:
    mutable uint32_t version_ = 0;
    UsageType usage_;
    std::shared_ptr<BufferData> data_;
};

template<typename T>
class ConstantParameter {
public:
    ConstantParameter() {}
    ConstantParameter(std::shared_ptr<ConstantBuffer>& buf) : buffer_(buf) {}
    ConstantParameter(std::shared_ptr<ConstantBuffer>& buf, const T& param) : buffer_(buf), param_(param) {}

    ConstantParameter<T>& operator=(const T& v) {
        param_ = v;
        buffer_->Update(&param_);
        return *this;
    }

    T& param() { return param_; }
    const T& param() const { return param_; }
    std::shared_ptr<ConstantBuffer>& buffer() { return buffer_; }

    void Update() { buffer_->Update(&param_); }

private:
    std::shared_ptr<ConstantBuffer> buffer_;
    T param_;
};

class StructuredBuffer : public Buffer {
public:
    StructuredBuffer(size_t element_size, size_t element_count) :
        Buffer(BufferType::kStructuredBuffer, element_count * element_size),
        element_size_(element_size)
    {}

    size_t element_size() const { return element_size_; }
    size_t element_count() const { return size_ / element_size_; }

private:
    size_t element_size_;
};

class VertexBuffer : public Buffer {
public:
    VertexBuffer(size_t size, uint32_t stride) :
        Buffer(BufferType::kVertexBuffer, size),
        stride_(stride),
        count_(size/stride)
    {}

    size_t count() const { return count_; }

    virtual void Bind() const = 0;
    virtual void UnBind() const = 0;

    virtual void Bind(uint32_t slot, uint32_t offset) const = 0;
    virtual void UnBind(uint32_t slot, uint32_t offset) const = 0;

protected:
    uint32_t stride_ = 0;
    size_t count_ = 1;
};

class IndexBuffer : public Buffer {
public:
    IndexBuffer(size_t size, IndexFormat format) :
        Buffer(BufferType::kIndexBuffer, size),
        format_(format),
        count_(size / (format == IndexFormat::kUInt16 ? sizeof(uint16_t) : sizeof(uint32_t)))
    {}

    size_t count() const { return count_; }

    virtual void Bind() const = 0;
    virtual void UnBind() const = 0;

protected:
    IndexFormat format_ = IndexFormat::kUInt16;
    mutable size_t count_ = 1;
};

}
}
