#include "Buffer.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

void D3D11Buffer::Init(BufferType type, size_t size, UsageType usage, const void* data)
{
    D3D11_BUFFER_DESC buf_desc = {};
    buf_desc.BindFlags = ToBindFlag(type);
    buf_desc.MiscFlags = 0;
    buf_desc.ByteWidth = size;
    buf_desc.Usage = ToUsage(usage);
    buf_desc.CPUAccessFlags = usage == UsageType::kDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    //buf_desc.StructureByteStride = (UINT)stride; //only works for structured buffer

    auto device = D3D11GfxDriver::Instance()->GetDevice();
    if (data) {
        D3D11_SUBRESOURCE_DATA res_data = {};
        res_data.pSysMem = data;

        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, &res_data, &buffer_));
    }
    else {
        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, nullptr, &buffer_));
    }
}

void D3D11Buffer::UpdateResource(const void* data, size_t size) const {
    if (size == 0) return;

    D3D11_MAPPED_SUBRESOURCE res;
    auto context = D3D11GfxDriver::Instance()->GetContext();
    GfxThrowIfFailed(context->Map(
        buffer_.Get(), 0u,
        D3D11_MAP_WRITE_DISCARD, 0u,
        &res
    ));

    memcpy(res.pData, data, size);
    GfxThrowIfAny(context->Unmap(buffer_.Get(), 0u));
}

D3D11ConstantBuffer::D3D11ConstantBuffer(const void* data, size_t size, UsageType usage) :
    ConstantBuffer(size)
{
    Init(type_, size_, usage, data);
}

D3D11ConstantBuffer::D3D11ConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage) :
    D3D11ConstantBuffer(data->data(), data->data_size(), usage)
{
    version_ = data_->version();
    data_ = data;
}

void D3D11ConstantBuffer::Bind(ShaderType shader_type, uint16_t slot) {
    if (data_ && data_->version() != version_) {
        version_ = data_->version();
        Buffer::Update(data_->data());
    }

    assert(buffer_);
    auto context = D3D11GfxDriver::Instance()->GetContext();
    const UINT offset = 0u;
    switch (shader_type)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(context->VSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(context->HSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(context->DSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(context->GSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(context->PSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(context->CSSetConstantBuffers(slot, 1u, buffer_.GetAddressOf()));
        break;
    default:
        throw std::exception("Buffer::Bind: Unimplemented shader type.");
        break;
    }
}

void D3D11ConstantBuffer::UnBind(ShaderType shader_type, uint16_t slot) const {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    ID3D11Buffer* null_buf = nullptr;
    switch (shader_type)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(context->VSSetConstantBuffers(slot, 1, &null_buf));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(context->HSSetConstantBuffers(slot, 1, &null_buf));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(context->DSSetConstantBuffers(slot, 1, &null_buf));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(context->GSSetConstantBuffers(slot, 1, &null_buf));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(context->PSSetConstantBuffers(slot, 1, &null_buf));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(context->CSSetConstantBuffers(slot, 1, &null_buf));
        break;
    default:
        throw std::exception("Buffer::Bind: Unimplemented shader type.");
        break;
    }
}

D3D11VertexBuffer::D3D11VertexBuffer(const void* data, size_t size, size_t stride, UsageType usage) :
    VertexBuffer(size, stride)
{
    Init(type_, size, usage, data);
}

D3D11VertexBuffer::D3D11VertexBuffer(size_t size, size_t stride, UsageType usage) :
    D3D11VertexBuffer(nullptr, size, stride, usage)
{
}

D3D11VertexBuffer::D3D11VertexBuffer(const VertexData& vdata, UsageType usage) :
    D3D11VertexBuffer(vdata.data(), vdata.data_size(), vdata.stride(), usage)
{
}

void D3D11VertexBuffer::Bind() const {
    Bind(0, 0);
}

void D3D11VertexBuffer::UnBind() const {
    UnBind(0, 0);
}

void D3D11VertexBuffer::UnBind(uint32_t slot, uint32_t offset) const {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    ID3D11Buffer* null_buf = nullptr;
    GfxThrowIfAny(context->IASetVertexBuffers(slot, 1, &null_buf, nullptr, &offset));
}

void D3D11VertexBuffer::Bind(uint32_t slot, uint32_t offset) const {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    GfxThrowIfAny(context->IASetVertexBuffers(slot, 1, buffer_.GetAddressOf(), &stride_, &offset));
}

D3D11IndexBuffer::D3D11IndexBuffer(const void* data, size_t size, IndexFormat format, UsageType usage) :
    IndexBuffer(size, format)
{
    Init(type_, size_, usage, data);
}

D3D11IndexBuffer::D3D11IndexBuffer(const std::vector<uint32_t>& indices, UsageType usage) :
    D3D11IndexBuffer(indices.data(), indices.size() * sizeof(uint32_t), IndexFormat::kUInt32, usage)
{
}

D3D11IndexBuffer::D3D11IndexBuffer(const std::vector<uint16_t>& indices, UsageType usage) :
    D3D11IndexBuffer(indices.data(), indices.size() * sizeof(uint16_t), IndexFormat::kUInt16, usage)
{
}

void D3D11IndexBuffer::Update(const void* data, size_t size) {
    assert(size <= size_);
    UpdateResource(data, size);

    count_ = size / ((format_ == IndexFormat::kUInt16) ? sizeof(uint16_t) : sizeof(uint32_t));
}

void D3D11IndexBuffer::Bind() const {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    GfxThrowIfAny(context->IASetIndexBuffer(
        buffer_.Get(),
        format_ == IndexFormat::kUInt32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
        0));
}

void D3D11IndexBuffer::UnBind() const {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    GfxThrowIfAny(context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0));

}


}
}
