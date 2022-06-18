#include "shader.h"
#include <typeinfo>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {
namespace render {

std::unordered_map<std::string, std::shared_ptr<D3D11Shader>> D3D11Shader::cache_;

HRESULT D3D11Shader::CompileFromFile(const TCHAR* file_path, ID3DBlob** ptr_blob, const D3D_SHADER_MACRO* defines,
    const char* entry_point, const char* target, UINT flags1, UINT flags2)
{
#if defined(DEBUG) || defined(_DEBUG)
    flags1 |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* error_blob = nullptr;

    HRESULT hr = D3DCompileFromFile(file_path, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point, target, flags1, flags2,
        ptr_blob, &error_blob);

    if (error_blob) {
        OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
        error_blob->Release();
    }

    return hr;
}

std::shared_ptr<D3D11Shader> D3D11Shader::Create(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const char* target)
{
    if (!target || strcmp(target, "latest") == 0) {
        target = GetLatestTarget(type);
    }

    if (!entry_point) {
        entry_point = DefaultShaderEntry[(int)type];
    }

    std::stringstream key_ss;
    key_ss << toUType(type) << "|" << file_name << "|" << entry_point << "|" << target;

    auto key = key_ss.str();
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    auto ret = std::make_shared<D3D11Shader>(type, file_name, entry_point, target);
    cache_.emplace_hint(it, key, ret);
    return ret;
}

D3D11Shader::D3D11Shader(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const char* target, const std::vector<ShaderMacroEntry>& macros) :
    Shader(type, file_name)
{
    EngineString path;
    path.append(TEXT("assets\\shader\\"));
    path.append(file_name);
    path.append(L".hlsl");

    if (!target || strcmp(target, "latest") == 0) {
        target = GetLatestTarget(type);
    }

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef GLACIER_REVERSE_Z
    std::vector<ShaderMacroEntry> real_macros;
    real_macros.reserve(macros.size() + 1);

    real_macros.push_back({ "GLACIER_REVERSE_Z", "1" });
    real_macros.insert(real_macros.end(), macros.begin(), macros.end());
    auto macro_data = (const D3D_SHADER_MACRO*)real_macros.data();
#else
    auto macro_data = (const D3D_SHADER_MACRO*)macros.data();
#endif

    GfxThrowIfFailed(CompileFromFile(path.c_str(), &blob_, macro_data, entry_point ? entry_point : DefaultShaderEntry[(int)type], target, flags, 0));

    auto dev = D3D11GfxDriver::Instance()->GetDevice();
    switch (type_)
    {
    case ShaderType::kVertex:
        GfxThrowIfFailed(dev->CreateVertexShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &vertex_shader_));
        break;
    case ShaderType::kHull:
        GfxThrowIfFailed(dev->CreateHullShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &hull_shader_));
        break;
    case ShaderType::kDomain:
        GfxThrowIfFailed(dev->CreateDomainShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &domain_shader_));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfFailed(dev->CreateGeometryShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &geometry_shader_));
        break;
    case ShaderType::kPixel:
        GfxThrowIfFailed(dev->CreatePixelShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &pixel_shader_));
        break;
    case ShaderType::kCompute:
        GfxThrowIfFailed(dev->CreateComputeShader(blob_->GetBufferPointer(), blob_->GetBufferSize(), nullptr, &compute_shader_));
        break;
    default:
        throw std::exception{ "Invalid Shader Type." };
        break;
    }

    SetupParameter();
}

void D3D11Shader::SetupParameter() {

    // Reflect the parameters from the shader.
    // Inspired by: http://members.gamedev.net/JasonZ/Heiroglyph/D3D11ShaderReflection.pdf
    ComPtr<ID3D11ShaderReflection> reflector;
    GfxThrowIfFailed(D3DReflect(blob_->GetBufferPointer(), blob_->GetBufferSize(), IID_ID3D11ShaderReflection, &reflector));

    D3D11_SHADER_DESC desc;
    GfxThrowIfFailed(reflector->GetDesc(&desc));

    // Query Resources that are bound to the shader.
    for (UINT i = 0; i < desc.BoundResources; ++i)
    {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc;
        reflector->GetResourceBindingDesc(i, &bindDesc);
        std::string resourceName = bindDesc.Name;

        ShaderParameterType parameterType = ShaderParameterType::kUnknown;

        switch (bindDesc.Type)
        {
        case D3D_SIT_CBUFFER:
            parameterType = ShaderParameterType::kCBV;
            break;
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
            parameterType = ShaderParameterType::kSRV;
            break;
        case D3D_SIT_SAMPLER:
            parameterType = ShaderParameterType::kSampler;
            break;
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWTYPED:
            parameterType = ShaderParameterType::kUAV;
            break;
        }

        ShaderParameter param(resourceName);
        param.entries[(int)type_] = {type_, parameterType, bindDesc.BindPoint, bindDesc.BindCount};


        // Create an empty shader parameter that should be filled-in by the application.
        params_.emplace(resourceName, param);
    }
}

ID3DBlob* D3D11Shader::GetBytecode() const noexcept {
    return blob_ ? blob_.Get() : nullptr;
}

void D3D11Shader::Bind() {
    auto ctx = D3D11GfxDriver::Instance()->GetContext();
    switch (type_)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(ctx->VSSetShader(vertex_shader_.Get(), nullptr, 0u));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(ctx->HSSetShader(hull_shader_.Get(), nullptr, 0u));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(ctx->DSSetShader(domain_shader_.Get(), nullptr, 0u));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(ctx->GSSetShader(geometry_shader_.Get(), nullptr, 0u));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(ctx->PSSetShader(pixel_shader_.Get(), nullptr, 0u));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(ctx->CSSetShader(compute_shader_.Get(), nullptr, 0u));
        break;
    default:
        throw std::exception{ "Invalid Shader Type." };
        break;
    }
}

void D3D11Shader::UnBind() {
    auto ctx = D3D11GfxDriver::Instance()->GetContext();
    switch (type_)
    {
    case ShaderType::kVertex:
        GfxThrowIfAny(ctx->VSSetShader(nullptr, nullptr, 0u));
        break;
    case ShaderType::kHull:
        GfxThrowIfAny(ctx->HSSetShader(nullptr, nullptr, 0u));
        break;
    case ShaderType::kDomain:
        GfxThrowIfAny(ctx->DSSetShader(nullptr, nullptr, 0u));
        break;
    case ShaderType::kGeometry:
        GfxThrowIfAny(ctx->GSSetShader(nullptr, nullptr, 0u));
        break;
    case ShaderType::kPixel:
        GfxThrowIfAny(ctx->PSSetShader(nullptr, nullptr, 0u));
        break;
    case ShaderType::kCompute:
        GfxThrowIfAny(ctx->CSSetShader(nullptr, nullptr, 0u));
        break;
    default:
        throw std::exception{ "Invalid Shader Type." };
        break;
    }
}

const char* D3D11Shader::GetLatestTarget(ShaderType type) {
    // Query the current feature level:
    D3D_FEATURE_LEVEL featureLevel = D3D11GfxDriver::Instance()->GetDevice()->GetFeatureLevel();

    switch (type)
    {
    case ShaderType::kVertex:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "vs_5_0";
            break;
        case D3D_FEATURE_LEVEL_10_1:
            return "vs_4_1";
            break;
        case D3D_FEATURE_LEVEL_10_0:
            return "vs_4_0";
            break;
        case D3D_FEATURE_LEVEL_9_3:
            return "vs_4_0_level_9_3";
            break;
        case D3D_FEATURE_LEVEL_9_2:
        case D3D_FEATURE_LEVEL_9_1:
            return "vs_4_0_level_9_1";
            break;
        }
        break;
    case ShaderType::kDomain:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "ds_5_0";
            break;
        }
        break;
    case ShaderType::kHull:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "hs_5_0";
            break;
        }
        break;
    case ShaderType::kGeometry:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "gs_5_0";
            break;
        case D3D_FEATURE_LEVEL_10_1:
            return "gs_4_1";
            break;
        case D3D_FEATURE_LEVEL_10_0:
            return "gs_4_0";
            break;
        }
        break;
    case ShaderType::kPixel:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "ps_5_0";
            break;
        case D3D_FEATURE_LEVEL_10_1:
            return "ps_4_1";
            break;
        case D3D_FEATURE_LEVEL_10_0:
            return "ps_4_0";
            break;
        case D3D_FEATURE_LEVEL_9_3:
            return "ps_4_0_level_9_3";
            break;
        case D3D_FEATURE_LEVEL_9_2:
        case D3D_FEATURE_LEVEL_9_1:
            return "ps_4_0_level_9_1";
            break;
        }
        break;
    case ShaderType::kCompute:
        switch (featureLevel)
        {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "cs_5_0";
            break;
        case D3D_FEATURE_LEVEL_10_1:
            return "cs_4_1";
            break;
        case D3D_FEATURE_LEVEL_10_0:
            return "cs_4_0";
            break;
        }
    } // switch( type )

    throw std::exception{ "Can't found avialable shader target." };
}

}
}
