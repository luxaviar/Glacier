#pragma once

#include <unordered_map>
#include <array>
#include <d3dcommon.h>
#include "Common/Util.h"
#include "resource.h"

namespace glacier {
namespace render {

struct ShaderMacroEntry {
    const char* name;
    const char* definition;
};

struct ShaderParameter {
    struct Entry {
        ShaderType shader_type = ShaderType::kUnknown;
        ShaderParameterType category = ShaderParameterType::kUnknown;
        uint32_t bind_point;
        uint32_t bind_count = 1;
        uint32_t register_space = 0;

        operator bool() const { return shader_type != ShaderType::kUnknown; }
    };

    ShaderParameter(const std::string& name) : name(name) {}

    std::string name;
    std::array<Entry, (size_t)ShaderType::kUnknown> entries;

    operator bool() const { return !name.empty(); }
};

class Shader : public Resource {
public:
    static HRESULT CompileFromFile(const TCHAR* file_path, ID3DBlob** ptr_blob, const D3D_SHADER_MACRO* defines,
        const char* entry_point, const char* target, UINT flags1 = 0, UINT flags2 = 0);

    Shader(ShaderType type, const TCHAR* file_name) : 
        type_(type),
        file_name_(file_name)
    {}
    
    virtual void Bind() {};
    virtual void UnBind() {};

    ShaderType type() const { return type_; }
    const EngineString& file_name() const { return file_name_; }
    
    ID3DBlob* GetBytecode() const noexcept {
        return blob_ ? blob_.Get() : nullptr;
    }

    size_t GetBytecodeSize() const {
        return blob_ ? blob_->GetBufferSize() : 0;
    }

    const ShaderParameter* FindParameter(const std::string& name) const {
        return FindParameter(name.c_str());
    }

    const ShaderParameter* FindParameter(const char* name) const {
        auto it = params_.find(name);
        if (it != params_.end()) {
            return &it->second;
        }

        return nullptr;
    }

    const std::unordered_map<std::string, ShaderParameter>& GetAllParameters() const { return params_; }

protected:
    ShaderType type_ = ShaderType::kUnknown;
    EngineString file_name_;
    ComPtr<ID3DBlob> blob_;
    std::unordered_map<std::string, ShaderParameter> params_;
};

}
}
