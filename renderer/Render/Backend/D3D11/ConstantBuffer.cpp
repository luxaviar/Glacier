#include "ConstantBuffer.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

D3D11Buffer::D3D11Buffer(std::shared_ptr<BufferData>& data) :
    ConstantBuffer(data)
{
    Init(data->data(), data->data_size(), UsageType::kDefault);
}

D3D11Buffer::D3D11Buffer(const void* data, size_t size, UsageType usage) :
    ConstantBuffer(size)
{
    Init(data, size, usage);
}

void D3D11Buffer::Init(const void* data, size_t size, UsageType usage) {
    D3D11_BUFFER_DESC buf_desc = {};
    buf_desc.BindFlags = 0;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = 0;
    buf_desc.ByteWidth = size_;
    //buf_desc.StructureByteStride = (UINT)stride; //only works for structured buffer

    buf_desc.Usage = usage == UsageType::kDefault ? D3D11_USAGE_DYNAMIC : ToUsage(usage);
    buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    auto device = GfxDriverD3D11::Instance()->GetDevice();
    if (data) {
        D3D11_SUBRESOURCE_DATA res_data = {};
        res_data.pSysMem = data;

        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, &res_data, &buffer_));
    } else {
        GfxThrowIfFailed(device->CreateBuffer(&buf_desc, nullptr, &buffer_));
    }
}

void D3D11Buffer::Update(const void* data) {
    D3D11_MAPPED_SUBRESOURCE res;
    auto context = GfxDriverD3D11::Instance()->GetContext();
    GfxThrowIfFailed(context->Map(
        buffer_.Get(), 0u,
        D3D11_MAP_WRITE_DISCARD, 0u,
        &res
    ));

    memcpy(res.pData, data, size_);
    GfxThrowIfAny(context->Unmap(buffer_.Get(), 0u));
}

void D3D11Buffer::Bind(ShaderType shader_type, uint16_t slot) {
    if (data_ && data_->version() != version_) {
        version_ = data_->version();
        Update(data_->data());
    }

    auto context = GfxDriverD3D11::Instance()->GetContext();
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

void D3D11Buffer::UnBind(ShaderType shader_type, uint16_t slot) {
    auto context = GfxDriverD3D11::Instance()->GetContext();
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

}
}
