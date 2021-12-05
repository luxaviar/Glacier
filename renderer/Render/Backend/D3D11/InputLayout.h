#pragma once

#include <d3d11_1.h>
#include <map>
#include "render/base/inputlayout.h"

namespace glacier {
namespace render {

class D3D11InputLayout : public InputLayout {
public:
    static std::shared_ptr<D3D11InputLayout> Create(const InputLayoutDesc& layout);
    static void Clear();

    D3D11InputLayout(const InputLayoutDesc& layout);

    void Bind() const override;

    void* underlying_resource() const override { return layout_.Get(); }

private:
    ComPtr<ID3D11InputLayout> layout_;

private:
    static ComPtr<ID3DBlob> GenBlobFromLayout(const InputLayoutDesc& layout);
    static  std::map<uint32_t, std::shared_ptr<D3D11InputLayout>> cache_;
};

}
}
