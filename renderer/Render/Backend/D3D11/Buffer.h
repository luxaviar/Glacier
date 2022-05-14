#pragma once

#include <d3d11_1.h>
#include "Resource.h"
#include "Common/Uncopyable.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/Bufferlayout.h"
#include "Render/Base/VertexLayout.h"

namespace glacier {
namespace render {

class D3D11Buffer : public D3D11Resource {
public:
    D3D11Buffer() {}
    ID3D11Resource* GetUnderlyingResource() const override { return buffer_.Get();}

protected:
    void Init(BufferType type, size_t size, UsageType usage = UsageType::kDefault, const void* data = nullptr);
    void UpdateResource(const void* data, size_t size) const;

    ComPtr<ID3D11Buffer> buffer_;
};

class D3D11ConstantBuffer : public ConstantBuffer, public D3D11Buffer {
public:
    D3D11ConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic);
    D3D11ConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage = UsageType::kDynamic);

    void Update(const void* data, size_t size) override {
        assert(size <= size_);
        UpdateResource(data, size);
    }

    void Bind(ShaderType shader_type, uint16_t slot);
    void UnBind(ShaderType shader_type, uint16_t slot) const;
};

class D3D11VertexBuffer : public VertexBuffer, public D3D11Buffer {
public:
    D3D11VertexBuffer(size_t size, size_t stride, UsageType usage = UsageType::kDefault);
    D3D11VertexBuffer(const void* data, size_t size, size_t stride, UsageType usage = UsageType::kDefault);
    D3D11VertexBuffer(const VertexData& vdata, UsageType usage = UsageType::kDefault);

    void Update(const void* data, size_t size) override {
        assert(size <= size_);
        UpdateResource(data, size);
    }

    void Bind() const override;
    void UnBind() const override;

    void Bind(uint32_t slot, uint32_t offset) const override;
    void UnBind(uint32_t slot, uint32_t offset) const override;
};

class D3D11IndexBuffer : public IndexBuffer, public D3D11Buffer {
public:
    D3D11IndexBuffer(const void* data, size_t size, IndexFormat format, UsageType usage = UsageType::kDefault);
    D3D11IndexBuffer(const std::vector<uint32_t>& indices, UsageType usage = UsageType::kDefault);
    D3D11IndexBuffer(const std::vector<uint16_t>& indices, UsageType usage = UsageType::kDefault);

    void Update(const void* data, size_t size) override;

    void Bind() const override;
    void UnBind() const override;
};

}
}
