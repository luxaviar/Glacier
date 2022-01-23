#include "inputlayout.h"
#include <d3dcompiler.h>
#include <sstream>
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {
namespace render {

std::map<uint32_t, std::shared_ptr<D3D11InputLayout>> D3D11InputLayout::cache_;

ComPtr<ID3DBlob> D3D11InputLayout::GenBlobFromLayout(const InputLayoutDesc& layout) {
    std::stringstream ss;
    ss << "struct VertexIn { ";
    for (size_t i = 0; i < layout.size(); ++i) {
        auto& e = layout[i];
        ss << e.type_name() << " " << e.code() << " : " << e.semantic() << ";";
    }
    ss << " };"
"float4 main(VertexIn vi) : SV_POSITION"
"{"
"return float4(0.0, 0.0, 0.0, 0.0);"
"}";

    auto str = ss.str();
    ComPtr<ID3DBlob> blob;
    GfxThrowIfFailed(D3DCompile(str.c_str(), str.size(),
        nullptr, nullptr, nullptr, "main", "vs_5_1",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, nullptr));

    return blob;
}

std::shared_ptr<D3D11InputLayout> D3D11InputLayout::Create(const InputLayoutDesc& layout) {
    auto sig = layout.signature();
    auto it = cache_.find(sig);
    if (it != cache_.end()) {
        return it->second;
    }

    auto ret = std::make_shared<D3D11InputLayout>(layout);
    cache_.emplace_hint(it, sig, ret);
    return ret;
}

void D3D11InputLayout::Clear() {
    cache_.clear();
}

D3D11InputLayout::D3D11InputLayout(const InputLayoutDesc& layout) : InputLayout(layout) {
    const auto layout_desc = layout_desc_.layout_desc<D3D11_INPUT_ELEMENT_DESC>();
    if (layout_desc.empty()) return;

    auto ptr = GenBlobFromLayout(layout);

    GfxThrowIfFailed(D3D11GfxDriver::Instance()->GetDevice()->CreateInputLayout(
        layout_desc.data(), (UINT)layout_desc.size(),
        ptr->GetBufferPointer(),
        ptr->GetBufferSize(),
        &layout_
    ));
}

void D3D11InputLayout::Bind(GfxDriver* gfx) const {
    if (gfx->layout_signature() == layout_desc_.signature()) {
        return;
    }

    gfx->layout_signature(layout_desc_.signature());
    GfxThrowIfAny(static_cast<D3D11GfxDriver*>(gfx)->GetContext()->IASetInputLayout(layout_.Get()));
}

}
}
