#include "indexbuffer.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {
namespace render {

D3D11IndexBuffer::D3D11IndexBuffer(const void* data, size_t size, IndexType type) :
    IndexBuffer(size, type)
{
    assert(data);
    D3D11_BUFFER_DESC buf_desc = {};
    buf_desc.BindFlags = 0;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = 0;
    buf_desc.ByteWidth = size_;

    buf_desc.Usage = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;

    auto device = GfxDriverD3D11::Instance()->GetDevice();
    D3D11_SUBRESOURCE_DATA res_data = {};
    res_data.pSysMem = data;

    GfxThrowIfFailed(device->CreateBuffer(&buf_desc, &res_data, &buffer_));
}

D3D11IndexBuffer::D3D11IndexBuffer(const std::vector<uint32_t>& indices) :
    D3D11IndexBuffer(indices.data(), indices.size() * sizeof(uint32_t), IndexType::kUInt32)
{
}

D3D11IndexBuffer::D3D11IndexBuffer(const std::vector<uint16_t>& indices) :
    D3D11IndexBuffer(indices.data(), indices.size() * sizeof(uint16_t), IndexType::kUInt16)
{
}

void D3D11IndexBuffer::Bind() {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    GfxThrowIfAny(context->IASetIndexBuffer(
        buffer_.Get(), 
        type_ == IndexType::kUInt32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 
        0));
}

void D3D11IndexBuffer::UnBind() {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    GfxThrowIfAny(context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0));
}

}
}
