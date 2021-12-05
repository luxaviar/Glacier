#pragma once

#include <d3d11_1.h>
#include "Common/Uncopyable.h"
#include "render/base/ConstantBuffer.h"

namespace glacier {
namespace render {

class D3D11Buffer : public ConstantBuffer {
public:
    D3D11Buffer(const void* data, size_t size, UsageType usage = UsageType::kDefault);
    D3D11Buffer(std::shared_ptr<BufferData>& data);

    D3D11Buffer(D3D11Buffer&& other) noexcept = default;

    void Update(const void* data) override;
    void Bind(ShaderType shader_type, uint16_t slot) override;
    void UnBind(ShaderType shader_type, uint16_t slot) override;

    void* underlying_resource() const override { return buffer_.Get(); }

private:
    void Init(const void* data, size_t size, UsageType usage);

    ComPtr<ID3D11Buffer> buffer_;
};

}
}
