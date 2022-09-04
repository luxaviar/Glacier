#pragma once

#include <unordered_map>
#include <vector>
#include <array>
#include "Shader.h"
#include "PipelineState.h"
#include "SamplerState.h"
#include "Render/MaterialProperty.h"

namespace glacier {
namespace render {

class Material;
class PassNode;
class CommandBuffer;

class Program : private Uncopyable, public Identifiable<Program> {
public:
    static std::shared_ptr<Program> Create(const char* name, const TCHAR* vs, const TCHAR* ps);
    static std::shared_ptr<Program> Create(const char* name, const TCHAR* cs);

    Program(const char* name);
    virtual ~Program() {}

    const std::string& name() const { return name_; }
    uint32_t version() const { return version_; }
    bool IsCompute() const { return is_compute_; }

    void SetRasterState(const RasterStateDesc& rs);
    void SetInputLayout(const InputLayoutDesc& desc);

    void SetupMaterial(Material* material);

    void SetShader(const std::shared_ptr<Shader>& shader);

    void SetVertexShader(const TCHAR* vs);
    void SetPixelShader(const TCHAR* ps);
    void SetComputeShader(const TCHAR* cs);

    const Shader* GetShader(ShaderType type) const { return shaders_[(size_t)type].get(); }

    template<typename T>
    const T* GetNativeShader(ShaderType type) {
        auto sh = GetShader(type);
        if (sh) {
            return static_cast<const T*>(sh);
        }

        return nullptr;
    }

    virtual void BindPSO(CommandBuffer* cmd_buffer) = 0;
    virtual void Bind(CommandBuffer* cmd_buffer, Material* mat) = 0;
    virtual void RefreshTranstientBuffer(CommandBuffer* cmd_buffer) = 0;

    void AddPass(const char* pass_name);
    bool HasPass(const PassNode* pass) const;

    const ShaderParameter* FindParameter(const std::string& name) const;
    const ShaderParameter* FindParameter(const char* name) const;
    bool HasProperty(const char* name);

    template<typename T>
    void SetProperty(const char* name, ConstantParameter<T>& param) {
        SetProperty(name, param.buffer());
    }

    void SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color = Color::kWhite);
    void SetProperty(const char* name, const std::shared_ptr<Buffer>& buf);
    void SetProperty(const char* name, const SamplerState& ss);
    void SetProperty(const char* name, const Color& color);
    void SetProperty(const char* name, const Vec4f& v);
    void SetProperty(const char* name, const Matrix4x4& v);
    void UpdateProperty(const char* name, const void* data);

    virtual void DrawInspector();
    
protected:
    virtual void SetupShaderParameter(const std::shared_ptr<Shader>& shader) = 0;
    void SetShaderParameter(const std::string& name, const ShaderParameter& param);

    uint32_t version_ = 0;
    bool is_compute_ = false;
    std::string name_;
    std::array<std::shared_ptr<Shader>, (size_t)ShaderType::kUnknown> shaders_;
    std::unordered_map<std::string, ShaderParameter> params_;
    std::shared_ptr<PipelineState> pso_;

    std::unordered_map<std::string, MaterialProperty> properties_;
    std::vector<std::string> passes_;
};

}
}
