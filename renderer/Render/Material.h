/*`*/ #pragma once

#include <unordered_map>
#include "Render/Base/GfxDriver.h"
#include "MaterialProperty.h"

namespace glacier {
namespace render {

class PassNode;
class Program;

struct PbrParam {
    Vec3f f0 = Vec3f(0.04f);
    uint32_t use_normal_map = 0;
};

class Material : public Identifiable<Material> {
public:
    friend class Program;

    Material(const char* name, const std::shared_ptr<Program>& program);
    Material(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr);

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }
    bool HasParameter(const char* name) const;

    const Vec4f& GetTexTilingOffset() const { return tex_ts_; }
    void SetTexTilingOffset(const Vec4f& st);

    const std::shared_ptr<Program>& GetProgram() const { return program_; }
    const std::unordered_map<std::string, MaterialProperty>& GetProperties() const { return properties_; }

    void Bind(GfxDriver* gfx);
    void UnBind(GfxDriver* gfx);

    void AddPass(const char* pass_name);
    bool HasPass(const PassNode* pass) const;

    template<typename T>
    void SetProperty(const char* name, ConstantParameter<T>& param) {
        SetProperty(name, param.buffer());
    }

    void SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color = Color::kWhite);
    void SetProperty(const char* name, const std::shared_ptr<Buffer>& buf);
    void SetProperty(const char* name, const Color& color);
    void SetProperty(const char* name, const Vec4f& v);
    void SetProperty(const char* name, const Matrix4x4& v);
    void SetProperty(const char* name, const SamplerState& ss);
    void UpdateProperty(const char* name, const void* data);

    void DrawInspector();

protected:
    void CloneSamplers();
    void SetupBuiltinProperty();


    std::string name_;
    uint32_t propgram_version_ = 0;
    bool setup_builtin_props_ = false;
    
    Vec4f tex_ts_ = { 1.0f, 1.0f, 0.0f, 0.0f }; //xy: tilling, zw: offset

    std::shared_ptr<Program> program_;
    std::unordered_map<std::string, MaterialProperty> properties_;
};

class PostProcessMaterial : public Material {
public:
    PostProcessMaterial(const char* name, const TCHAR* ps);
};

class MaterialGuard : public Uncopyable {
public:
    MaterialGuard(GfxDriver* gfx, Material* mat) : gfx_(gfx), mat_(mat) {
        if (mat) gfx_->BindMaterial(mat);
    }
    ~MaterialGuard() {
        if (mat_) gfx_->UnBindMaterial();
    }

private:
    GfxDriver* gfx_;
    Material* mat_;
};

class MaterialManager : public Singleton<MaterialManager> {
public:
    //Material* Create(const char* name);
    void Add(std::shared_ptr<Material>&& mat);
    void Remove(const char* name);

    void Clear() {
        materials_.clear();
    }

    const std::shared_ptr<Material>& Get(const char* name);

private:
    std::unordered_map<std::string, std::shared_ptr<Material>> materials_;
};

}
}
