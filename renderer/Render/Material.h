/*`*/ #pragma once

#include <unordered_map>
#include "MaterialTemplate.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

class PassNode;

class Material : public Identifiable<Material> {
public:
    Material(const char* name, const std::shared_ptr<MaterialTemplate>& temp);
    Material(const char* name, const std::shared_ptr<Program>& program);
    Material(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr);

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    const Vec4f& GetTexTilingOffset() const { return tex_ts_; }
    void SetTexTilingOffset(const Vec4f& st);

    const std::shared_ptr<MaterialTemplate>& GetTemplate() const { return template_; }
    const std::vector<MaterialProperty>& GetProperties() const { return properties_; }

    void Bind(GfxDriver* gfx);
    void UnBind(GfxDriver* gfx);
    void ReBind(GfxDriver* gfx);

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

    //mutable bool dirty_ = true;
    std::string name_;
    
    Vec4f tex_ts_ = { 1.0f, 1.0f, 0.0f, 0.0f }; //xy: tilling, zw: offset

    std::shared_ptr<MaterialTemplate> template_;
    std::vector<MaterialProperty> properties_;
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
    void Add(std::unique_ptr<Material>&& mat);
    void Remove(const char* name);

    void Clear() {
        materials_.clear();
    }

    Material* Get(const char* name);

private:
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

}
}
