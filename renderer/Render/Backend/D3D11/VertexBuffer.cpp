#include "vertexbuffer.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {
namespace render {

static D3D11_USAGE ToUsage(UsageType type) {
    switch (type)
    {
    case UsageType::kDefault:
        return D3D11_USAGE_DEFAULT;
    case UsageType::kImmutable:
        return D3D11_USAGE_IMMUTABLE;
    case UsageType::kDynamic:
        return D3D11_USAGE_DYNAMIC;
    case UsageType::kStage:
        return D3D11_USAGE_STAGING;
    default:
        throw std::exception{ "Bad UsageType." };
    }
}

D3D11VertexBuffer::D3D11VertexBuffer(const void* data, size_t size, size_t stride, UsageType usage) :
    VertexBuffer(size, stride)
{
    D3D11_BUFFER_DESC buf_desc = {};
    buf_desc.BindFlags = 0;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = 0;
    buf_desc.ByteWidth = size_;
    //buf_desc.StructureByteStride = (UINT)stride; //only works for structured buffer

    buf_desc.Usage = ToUsage(usage);
    buf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buf_desc.CPUAccessFlags = usage == UsageType::kDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    auto device = GfxDriverD3D11::Instance()->GetDevice();
    if (data) {
        D3D11_SUBRESOURCE_DATA res_data = {};
        res_data.pSysMem = data;

        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, &res_data, &buffer_));
    } else {
        //assert(type == ShaderParameterType::kCBuffer);
        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, nullptr, &buffer_));
    }
}

D3D11VertexBuffer::D3D11VertexBuffer(const VertexData& vdata) :
    D3D11VertexBuffer(vdata.data(), vdata.data_size(), vdata.stride())
{
}

void D3D11VertexBuffer::Update(const void* data) {
    auto context = GfxDriverD3D11::Instance()->GetContext();

    D3D11_MAPPED_SUBRESOURCE res;
    GfxThrowIfFailed(context->Map(
        buffer_.Get(), 0u,
        D3D11_MAP_WRITE_DISCARD, 0u,
        &res
    ));

    memcpy(res.pData, data, size_);
    GfxThrowIfAny(context->Unmap(buffer_.Get(), 0u));
}

void D3D11VertexBuffer::Bind() {
    Bind(0, 0);
}

void D3D11VertexBuffer::UnBind() {
    UnBind(0, 0);
}

void D3D11VertexBuffer::UnBind(uint32_t slot, uint32_t offset) {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    ID3D11Buffer* null_buf = nullptr;
    GfxThrowIfAny(context->IASetVertexBuffers(slot, 1, &null_buf, nullptr, &offset));
}

void D3D11VertexBuffer::Bind(uint32_t slot, uint32_t offset) {
    auto context = GfxDriverD3D11::Instance()->GetContext();
    GfxThrowIfAny(context->IASetVertexBuffers(slot, 1, buffer_.GetAddressOf(), &stride_, &offset));
}

}
}
