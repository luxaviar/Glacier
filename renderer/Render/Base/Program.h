#pragma once

#include <unordered_map>
#include <vector>
#include <array>
#include "Shader.h"
#include "PipelineState.h"

namespace glacier {
namespace render {

class Program : private Uncopyable {
public:
    Program(const char* name);
    virtual ~Program() {}

    const std::string& name() const { return name_; }

    void SetPipelineStateObject(const std::shared_ptr<PipelineState>& pso) {
        pso_ = pso;
    }

    void SetShader(const std::shared_ptr<Shader>& shader);
    const Shader* GetShader(ShaderType type) const {
        return shaders_[(size_t)type].get();
    }

    template<typename T>
    const T* GetNativeShader(ShaderType type) {
        auto sh = GetShader(type);
        if (sh) {
            return static_cast<const T*>(sh);
        }

        return nullptr;
    }

    void Bind();
    void Unbind();

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

    virtual void DrawInspector();
    
protected:
    virtual void SetupShaderParameter(const std::shared_ptr<Shader>& shader) = 0;

    std::string name_;
    std::array<std::shared_ptr<Shader>, (size_t)ShaderType::kUnknown> shaders_;
    std::unordered_map<std::string, ShaderParameter> params_;
    std::shared_ptr<PipelineState> pso_;
};

}
}
