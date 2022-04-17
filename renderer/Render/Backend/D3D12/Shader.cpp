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
    path.append(TEXT("Assets\\Shader\\"));
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
