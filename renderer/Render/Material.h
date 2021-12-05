#pragma once

#include <unordered_map>
#include <wtypes.h>
#include "render/base/gfxdriver.h"
#include "Common/Singleton.h"
#include "common/color.h"
#include "core/identifiable.h"

namespace glacier {
namespace render {

class PassNode;
class Renderable;

struct MaterialProperty {
    MaterialProperty(const char* name, const std::shared_ptr<Texture>& tex,
        ShaderType shader_type, uint16_t slot, const Color& default_color);

    MaterialProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf,
        ShaderType shader_type, uint16_t slot);

    MaterialProperty(const char* name, const std::shared_ptr<Sampler>& sm,
        ShaderType shader_type, uint16_t slot);

    MaterialProperty(const char* name, const Color& color, ShaderType shader_type, uint16_t slot);
    MaterialProperty(const char* name, const Vec4f& float4, ShaderType shader_type, uint16_t slot);
    MaterialProperty(const char* name, const Matrix4x4& matrix, ShaderType shader_type, uint16_t slot);

    MaterialProperty(const MaterialProperty& other) noexcept;
    MaterialProperty(MaterialProperty&& other) noexcept;

    void Bind(GfxDriver* gfx) const;
    void UnBind() const;

    mutable bool dirty = false;
    std::string name;
    ShaderType shader_type;
    MaterialPropertyType prop_type;
    uint16_t slot;

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
    //vertex -> b0: model/view/project matrix;
    static constexpr uint32_t kReservedVertBufferSlot = 1;
    //pixel -> b0: shadow data; b1: light list;
    static constexpr uint32_t kReservedPixelBufferSlot = 2;
    //pixel -> t0: shadow texture; t1: brdf_lut; t2: radiance texture; t3: irradiance texture
    static constexpr uint32_t kReservedTextureSlot = 4;
    //pixle -> s0: shadow sampler;
    static constexpr uint32_t kReservedSamplerSlot = 1;

    Material(const char* name);
    Material(const Material& mat) noexcept = default;
    Material(Material&& mat) noexcept = default;

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }
    const Vec4f& GetTexTilingOffset() const { return tex_ts_; }
    void SetTexTilingOffset(const Vec4f& st);

    void Bind(GfxDriver* gfx) const;
    void ReBind(GfxDriver* gfx) const;
    void UnBind() const;

    void AddPass(const char* pass_name);
    bool HasPass(const PassNode* pass) const;


    void SetShader(const std::shared_ptr<Shader>& shader);

    void SetProperty(const char* name, const std::shared_ptr<Texture>& tex,
        ShaderType shader_type, uint16_t slot, const Color& default_color = Color::kWhite);

    void SetProperty(const char* name, const std::shared_ptr<Sampler>& sampler,
        ShaderType shader_type, uint16_t slot);

    void SetProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf,
        ShaderType shader_type, uint16_t slot);

    void SetProperty(const char* name, const Color& color,
        ShaderType shader_type, uint16_t slot);

    void SetProperty(const char* name, const Vec4f& v,
        ShaderType shader_type, uint16_t slot);

    void SetProperty(const char* name, const Matrix4x4& v,
        ShaderType shader_type, uint16_t slot);

    void UpdateProperty(const void* data, ShaderType shader_type, uint16_t slot);

    void DrawInspector();

private:
    mutable bool dirty_ = true;
    Vec4f tex_ts_ = {1.0f, 1.0f, 0.0f, 0.0f}; //xy: tilling, zw: offset
    std::string name_;
    std::vector<std::string> passes_;
    std::vector<std::shared_ptr<Shader>> shaders_;
    std::vector<MaterialProperty> properties_;
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
