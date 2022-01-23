#include "shader.h"
#include <typeinfo>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

std::shared_ptr<D3D12Shader> D3D12Shader::Create(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const char* target)
{
    if (!target || strcmp(target, "latest") == 0) {
        target = GetLatestTarget(type);
    }

    if (!entry_point) {
        entry_point = DefaultShaderEntry[(int)type];
    }

    auto ret = std::make_shared<D3D12Shader>(type, file_name, entry_point, target);
    return ret;
}

D3D12Shader::D3D12Shader(ShaderType type, const TCHAR* file_name, const char* entry_point,
    const char* target, const std::vector<ShaderMacroEntry>& macros)  :
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
    GfxThrowIfFailed(CompileFromFile(path.c_str(), &blob_, (const D3D_SHADER_MACRO*)macros.data(), entry_point ? entry_point : DefaultShaderEntry[(int)type], target, flags, 0));
}

const char* D3D12Shader::GetLatestTarget(ShaderType type) {
    switch (type)
    {
    case ShaderType::kVertex:
        return "vs_5_1";
    case ShaderType::kDomain:
        return "ds_5_1";
    case ShaderType::kHull:
        return "hs_5_1";
    case ShaderType::kGeometry:
        return "gs_5_1";
    case ShaderType::kPixel:
        return "ps_5_1";
    case ShaderType::kCompute:
        return "cs_5_1";
    }

    throw std::exception{ "Can't found avialable shader target." };
}

}
}
