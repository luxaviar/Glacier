#pragma once

#include <unordered_map>
#include <wtypes.h>
#include "render/base/gfxdriver.h"
#include "Common/Singleton.h"
#include "common/color.h"
#include "core/identifiable.h"
#include "Render/Base/Program.h"

namespace glacier {
namespace render {

class PassNode;
class Renderable;
class PipelineState;

struct MaterialProperty {
    MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Texture>& tex, 
        const Color& default_color);

    MaterialProperty(const ShaderParameter* param, const std::shared_ptr<ConstantBuffer>& buf);

    MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Sampler>& sm);

    MaterialProperty(const ShaderParameter* param, const Color& color);
    MaterialProperty(const ShaderParameter* param, const Vec4f& float4);
    MaterialProperty(const ShaderParameter* param, const Matrix4x4& matrix);

    MaterialProperty(const MaterialProperty& other) noexcept;
    MaterialProperty(MaterialProperty&& other) noexcept;

    void Bind(GfxDriver* gfx) const;
    void UnBind() const;

    mutable bool dirty = false;
    const ShaderParameter* shader_param;
    MaterialPropertyType prop_type;

    //texture
    bool use_default = false;
    mutable std::shared_ptr<Texture> texture;

    mutable std::shared_ptr<ConstantBuffer> buffer;
    std::shared_ptr<Sampler> sampler;

    union {
        Color default_color; //texture color
        Color color;
        Vec4f float4;
        Matrix4x4 matrix;
    };
};

class Material : public Identifiable<Material> {
public:
    Material(const char* name, const std::shared_ptr<Program>& program);
    Material(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr);

    Material(const Material& mat) noexcept = default;
    Material(Material&& mat) noexcept = default;

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    const Vec4f& GetTexTilingOffset() const { return tex_ts_; }
    void SetTexTilingOffset(const Vec4f& st);

    void SetPipelineStateObject(const std::shared_ptr<PipelineState> pso);
    void SetPipelineStateObject(RasterState rs);

    void Bind(GfxDriver* gfx) const;
    void ReBind(GfxDriver* gfx) const;
    void UnBind() const;

    void AddPass(const char* pass_name);
    bool HasPass(const PassNode* pass) const;

    void SetProgram(const std::shared_ptr<Program>& program);

    void SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color = Color::kWhite);
    void SetProperty(const char* name, const std::shared_ptr<Sampler>& sampler);
    void SetProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf);
    void SetProperty(const char* name, const Color& color);
    void SetProperty(const char* name, const Vec4f& v);
    void SetProperty(const char* name, const Matrix4x4& v);
    void UpdateProperty(const char* name, const void* data);

    void DrawInspector();

protected:
    mutable bool dirty_ = true;
    Vec4f tex_ts_ = {1.0f, 1.0f, 0.0f, 0.0f}; //xy: tilling, zw: offset
    std::string name_;
    std::vector<std::string> passes_;
    std::shared_ptr<Program> program_;
    std::shared_ptr<PipelineState> pso_;
    std::vector<MaterialProperty> properties_;
};

class PostProcessMaterial : public Material {
public:
    PostProcessMaterial(const char* name, const TCHAR* ps);
};

class MaterialGuard : public Uncopyable {
public:
    MaterialGuard(GfxDriver* gfx, Material* mat) : gfx_(gfx), mat_(mat) {
        if (mat) gfx_->PushMaterial(mat);
    }
    ~MaterialGuard() {
        if (mat_) gfx_->PopMaterial(mat_);
    }

private:
    GfxDriver* gfx_;
    Material* mat_;
};

class MaterialManager : public Singleton<MaterialManager> {
public:
    Material* Create(const char* name);
    void Add(std::unique_ptr<Material>&& mat);
    void Remove(const char* name);

    Material* Get(const char* name);

private:
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

}
}
