#pragma once

#include <d3d11_1.h>
#include "render/base/indexbuffer.h"

namespace glacier {
namespace render {

class D3D11IndexBuffer : public IndexBuffer {
public:
    D3D11IndexBuffer(const void* data, size_t size, IndexType type = IndexType::kUInt32);
    D3D11IndexBuffer(const std::vector<uint32_t>& indices);
    D3D11IndexBuffer(const std::vector<uint16_t>& indices);

    D3D11IndexBuffer(D3D11IndexBuffer&& other) noexcept = default;

    void Bind() override;
    void UnBind() override;

    void* underlying_resource() const override { return buffer_.Get(); }

protected:
    ComPtr<ID3D11Buffer> buffer_;
};

}
}
