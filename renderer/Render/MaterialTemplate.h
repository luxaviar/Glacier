#pragma once

#include <vector>
#include "Core/Identifiable.h"
#include "Render/Base/Program.h"
#include "MaterialProperty.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class PassNode;

class MaterialTemplate : public Identifiable<MaterialTemplate> {
public:
    MaterialTemplate(const char* name, const std::shared_ptr<Program>& program);
    MaterialTemplate(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr);

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    void AddPass(const char* pass_name);
    bool HasPass(const PassNode* pass) const;

    Program* GetProgram() const { return program_.get(); }

    const std::vector<MaterialProperty>& GetProperties() const { return properties_; }
    const std::vector<MaterialProperty>& GetSamplers() const { return samplers_; }

    void SetRasterState(const RasterStateDesc& rs);
    void SetInputLayout(const InputLayoutDesc& desc);

    void BindPSO(GfxDriver* gfx);
    void Bind(GfxDriver* gfx);
    void UnBind(GfxDriver* gfx);

    void SetProgram(const std::shared_ptr<Program>&program);

    template<typename T>
    void SetProperty(const char* name, ConstantParameter<T>& param) {
        SetProperty(name, param.buffer());
    }

    void SetProperty(const char* name, const std::shared_ptr<Texture>&tex, const Color & default_color = Color::kWhite);
    void SetProperty(const char* name, const std::shared_ptr<Buffer>& buf);
    void SetProperty(const char* name, const SamplerState& ss);
    void SetProperty(const char* name, const Color & color);
    void SetProperty(const char* name, const Vec4f & v);
    void SetProperty(const char* name, const Matrix4x4 & v);
    void UpdateProperty(const char* name, const void* data);

    void DrawInspector();

protected:
    //mutable bool dirty_ = true;
    std::string name_;
    std::shared_ptr<Program> program_;
    std::vector<MaterialProperty> properties_;
    std::vector<MaterialProperty> samplers_;
    std::vector<std::string> passes_;
};

}
}
