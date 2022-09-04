#pragma once

#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include "Render/Base/Shader.h"

namespace glacier {
namespace render {

class Resource;
struct D3D12DescriptorRange;

//struct D3D12ShaderParameter {
//    std::string name;
//    ShaderType shader_type = ShaderType::kUnknown;
//    ShaderParameterCategory category = ShaderParameterCategory::kUnknown;
//    D3D_SHADER_INPUT_TYPE type;
//    uint32_t bind_point;
//    uint32_t bind_count = 1;
//    uint32_t register_space = 0;
//    const Resource* resource = nullptr;
//};

class D3D12Shader : public Shader {
public:
    static std::shared_ptr<D3D12Shader> Create(ShaderType type, const TCHAR* file_name, const char* entry_point,
        const char* target);

    D3D12Shader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr,
        const char* target = nullptr, const std::vector<ShaderMacroEntry>& macros = { {0,0} });
protected:
    static const char* GetLatestTarget(ShaderType type);
};

}
}
