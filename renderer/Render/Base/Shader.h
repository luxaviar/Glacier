#pragma once

#include <unordered_map>
#include "Common/Util.h"
#include "resource.h"

namespace glacier {
namespace render {

struct ShaderMacroEntry {
    const char* name;
    const char* definition;
};

struct ShaderParameter {
    std::string name;
    ShaderType shader_type = ShaderType::kUnknown;
    ShaderParameterType type = ShaderParameterType::kUnknown;
    uint32_t bind_point;
    uint32_t bind_count = 1;
    uint32_t register_space = 0;
};

class Shader : public Resource {
public:
    Shader(ShaderType type, const TCHAR* file_name) : 
        type_(type),
        file_name_(file_name)
    {}
    
    virtual void Bind() = 0;
    virtual void UnBind() = 0;

    ShaderType type() const { return type_; }
    const EngineString& file_name() const { return file_name_; }

protected:
    ShaderType type_ = ShaderType::kUnknown;
    EngineString file_name_;
    std::unordered_map<std::string, ShaderParameter> params_;
};

}
}