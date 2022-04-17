#pragma once

#include <unordered_map>
#include <vector>
#include <array>
#include "Shader.h"
#include "PipelineState.h"
#include "SamplerState.h"

namespace glacier {
namespace render {

class Material;
class MaterialTemplate;
class GfxDriver;

struct SamplerParameter {
    SamplerState state;
    const ShaderParameter* param;
};

class Program : private Uncopyable {
public:
    Program(const char* name);
    virtual ~Program() {}

    const std::string& name() const { return name_; }

    void SetRasterState(const RasterStateDesc& rs);
    void SetInputLayout(const InputLayoutDesc& desc);
    //void SetSampler(const char* name, const SamplerState& ss);

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

    virtual void Bind(GfxDriver* gfx, Material* mat) = 0;
    virtual void UnBind(GfxDriver* gfx, Material* mat) = 0;
    virtual void ReBind(GfxDriver* gfx, Material* mat) {}

    virtual void Bind(GfxDriver* gfx, MaterialTemplate* mat) = 0;
    virtual void UnBind(GfxDriver* gfx, MaterialTemplate* mat) = 0;

    const ShaderParameterSet* FindParameter(const std::string& name) const {
        return FindParameter(name.c_str());
    }

    const ShaderParameterSet* FindParameter(const char* name) const {
        auto it = params_.find(name);
        if (it != params_.end()) {
            return &it->second;
        }

        return nullptr;
    }

    virtual void DrawInspector();
    
protected:
    virtual void SetupShaderParameter(const std::shared_ptr<Shader>& shader) = 0;
    ShaderParameterSet& FetchShaderParameterSet(const std::string& name);
    void SetShaderParameter(const std::string& name, const ShaderParameter& param);

    std::string name_;
    std::array<std::shared_ptr<Shader>, (size_t)ShaderType::kUnknown> shaders_;
    std::unordered_map<std::string, ShaderParameterSet> params_;
    std::shared_ptr<PipelineState> pso_;

    //std::vector<SamplerParameter> sampler_params_;
};

}
}
