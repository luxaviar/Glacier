#pragma once

#include <d3d11_1.h>
#include "render/base/vertexbuffer.h"
#include "render/base/vertexlayout.h"

namespace glacier {
namespace render {

class D3D11VertexBuffer : public VertexBuffer {
public:
    D3D11VertexBuffer(const void* data, size_t size, size_t stride, UsageType usage = UsageType::kDefault);
    //vertex buffer
    D3D11VertexBuffer(const VertexData& vdata);

    D3D11VertexBuffer(D3D11VertexBuffer&& other) noexcept = default;

    void Update(const void* data) override;

    void Bind() override;
    void UnBind() override;

    void Bind(uint32_t slot, uint32_t offset) override;
    void UnBind(uint32_t slot, uint32_t offset) override;

    void* underlying_resource() const override { return buffer_.Get(); }

private:
    ComPtr<ID3D11Buffer> buffer_;
};

}
}
