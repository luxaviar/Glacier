#include "material.h"
#include <stdexcept>
#include "common/color.h"
#include "render/base/renderable.h"
#include "imgui/imgui.h"
#include "Render/Graph/PassNode.h"
#include "render/base/ConstantBuffer.h"
#include "render/base/texture.h"
#include "render/base/sampler.h"

namespace glacier {
namespace render {

MaterialProperty::MaterialProperty(const char* name, const std::shared_ptr<Texture>& tex,
    ShaderType shader_type, uint16_t slot, const Color& default_color) :
    name(name), 
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kTexture),
    slot(slot),
    default_color(default_color),
    use_default(!tex),
    texture(tex)
{
}

MaterialProperty::MaterialProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf,
    ShaderType shader_type, uint16_t slot) :
    name(name),
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kCBuffer),
    slot(slot),
    buffer(buf)
{}

MaterialProperty::MaterialProperty(const char* name, const std::shared_ptr<Sampler>& sm,
    ShaderType shader_type, uint16_t slot) :
    name(name),
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kSampler),
    slot(slot),
    sampler(sm)
{}

MaterialProperty::MaterialProperty(const char* name, const Color& color, ShaderType shader_type, uint16_t slot) :
    name(name),
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kColor),
    slot(slot),
    color(color)
{
}

MaterialProperty::MaterialProperty(const char* name, const Vec4f& float4, ShaderType shader_type, uint16_t slot) :
    name(name),
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kFloat4),
    slot(slot),
    float4(float4)
{
}

MaterialProperty::MaterialProperty(const char* name, const Matrix4x4& matrix, ShaderType shader_type, uint16_t slot) :
    name(name),
    shader_type(shader_type),
    prop_type(MaterialPropertyType::kMatrix),
    slot(slot),
    matrix(matrix)
{
}

MaterialProperty::MaterialProperty(const MaterialProperty& other) noexcept :
    dirty(other.dirty),
    name(other.name),
    shader_type(other.shader_type),
    prop_type(other.prop_type),
    slot(other.slot),
    use_default(other.use_default),
    texture(other.texture),
    buffer(other.buffer),
    sampler(other.sampler)
{
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        default_color = other.default_color;
        break;
    case MaterialPropertyType::kColor:
        color = other.color;
        break;
    case MaterialPropertyType::kFloat4:
        float4 = other.float4;
        break;
    case MaterialPropertyType::kMatrix:
        matrix = other.matrix;
        break;
    default:
        break;
    }
}

MaterialProperty::MaterialProperty(MaterialProperty&& other) noexcept :
    dirty(other.dirty),
    name(other.name),
    shader_type(other.shader_type),
    prop_type(other.prop_type),
    slot(other.slot),
    use_default(other.use_default),
    texture(std::move(other.texture)),
    buffer(std::move(other.buffer)),
    sampler(std::move(other.sampler))
{
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        default_color = other.default_color;
        break;
    case MaterialPropertyType::kColor:
        color = other.color;
        break;
    case MaterialPropertyType::kFloat4:
        float4 = other.float4;
        break;
    case MaterialPropertyType::kMatrix:
        matrix = other.matrix;
        break;
    default:
        break;
    }
}

void MaterialProperty::Bind(GfxDriver* gfx) const {
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        if ((use_default && dirty) || !texture) {
            auto builder = Texture::Builder()
                .SetColor(color)
                .SetDimension(8, 8);
            texture = gfx->CreateTexture(builder); //gfx, default_color, 8, 8);
        }

        texture->Bind(shader_type, slot);
        break;
    case MaterialPropertyType::kSampler:
        sampler->Bind(shader_type, slot);
        break;
    case MaterialPropertyType::kCBuffer:
        buffer->Bind(shader_type, slot);
        break;
    case MaterialPropertyType::kColor:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Color>(color); //std::make_shared<ConstantBuffer<Color>>(gfx, color);
        } else if (dirty) {
            buffer->Update(&color);
        }
        buffer->Bind(shader_type, slot);
        break;
    case MaterialPropertyType::kFloat4:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Vec4f>(float4);// std::make_shared<ConstantBuffer<Vec4f>>(gfx, float4);
        }
        else if (dirty) {
            buffer->Update(&float4);
        }
        buffer->Bind(shader_type, slot);
        break;
    case MaterialPropertyType::kMatrix:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Matrix4x4>(matrix);// std::make_shared<ConstantBuffer<Matrix4x4>>(gfx, matrix);
        }
        else if (dirty) {
            buffer->Update(&matrix);
        }
        buffer->Bind(shader_type, slot);
        break;
    default:
        throw std::exception{ "Bad parameter type for MaterialProperty::Bind." };
        break;
    }
    dirty = false;
}

void MaterialProperty::UnBind() const {
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        texture->UnBind(shader_type, slot);
        break;
    case MaterialPropertyType::kSampler:
        sampler->UnBind(shader_type, slot);
        break;
    case MaterialPropertyType::kColor:
    case MaterialPropertyType::kFloat4:
    case MaterialPropertyType::kMatrix:
    case MaterialPropertyType::kCBuffer:
        buffer->UnBind(shader_type, slot);
        break;
    default:
        throw std::exception{ "Bad parameter type for MaterialProperty::Bind." };
        break;
    }
}

Material::Material(const char* name) : name_(name) {
     shaders_.resize((int)ShaderType::kUnknown);
}

void Material::ReBind(GfxDriver* gfx) const {
    if (!dirty_) return;

    dirty_ = false;
    for (auto& prop : properties_) {
        if (prop.dirty) {
            prop.Bind(gfx);
        }
    }
}

void Material::Bind(GfxDriver* gfx) const {
    dirty_ = false;
    for (auto& shader : shaders_) {
        if (shader) {
            shader->Bind();
        }
    }

    for (auto& prop : properties_) {
        prop.Bind(gfx);
    }
}

void Material::UnBind() const {
    for (auto& shader : shaders_) {
        if (shader) {
            shader->UnBind();
        }
    }

    for (auto& prop : properties_) {
        prop.UnBind();
    }
}

void Material::AddPass(const char* pass_name) {
    assert(std::find(passes_.begin(), passes_.end(), pass_name) == passes_.end());
    passes_.push_back(pass_name);
}

bool Material::HasPass(const PassNode* pass) const {
    return std::find(passes_.begin(), passes_.end(), pass->name()) != passes_.end();
}

void Material::SetTexTilingOffset(const Vec4f& st) {
    tex_ts_ = st;
}

void Material::SetShader(const std::shared_ptr<Shader>& shader) {
    assert(shader->type() != ShaderType::kUnknown);
    assert(!shaders_[(int)shader->type()]);

    shaders_[(int)shader->type()] = shader;
}

//TODO: check duplicate?
void Material::SetProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf,
    ShaderType shader_type, uint16_t slot) {

    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type &&
            prop.prop_type == MaterialPropertyType::kCBuffer &&
            prop.slot == slot) 
        {
            prop.buffer = buf;
            prop.name = name;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, buf, shader_type, slot);
}

void Material::SetProperty(const char* name, const std::shared_ptr<Texture>& tex,
    ShaderType shader_type, uint16_t slot, const Color& default_color) 
{
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type && 
            prop.prop_type == MaterialPropertyType::kTexture &&
            prop.slot == slot) 
        {
            prop.texture = tex;
            prop.name = name;
            prop.default_color = default_color;
            prop.use_default = !tex;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, tex, shader_type, slot, default_color);
}

void Material::SetProperty(const char* name, const std::shared_ptr<Sampler>& sampler,
    ShaderType shader_type, uint16_t slot) {
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type &&
            prop.prop_type == MaterialPropertyType::kSampler &&
            prop.slot == slot) 
        {
            prop.sampler = sampler;
            prop.name = name;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, sampler, shader_type, slot);
}

void Material::SetProperty(const char* name, const Color& color, ShaderType shader_type, uint16_t slot) {
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type && prop.slot == slot) {
            assert(prop.prop_type == MaterialPropertyType::kColor);
            prop.color = color;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, color, shader_type, slot);
}

void Material::SetProperty(const char* name, const Vec4f& v, ShaderType shader_type, uint16_t slot) {
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type && prop.slot == slot) {
            assert(prop.prop_type == MaterialPropertyType::kFloat4);
            prop.float4 = v;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, v, shader_type, slot);
}

void Material::SetProperty(const char* name, const Matrix4x4& v, ShaderType shader_type, uint16_t slot) {
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type && prop.slot == slot) {
            assert(prop.prop_type == MaterialPropertyType::kMatrix);
            prop.matrix = v;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(name, v, shader_type, slot);
}

void Material::UpdateProperty(const void* data, ShaderType shader_type, uint16_t slot) {
    for (auto& prop : properties_) {
        if (prop.shader_type == shader_type && prop.slot == slot) {
            assert(prop.prop_type == MaterialPropertyType::kCBuffer);
            prop.buffer->Update(data);
            prop.dirty = true;
            return;
        }
    }

    assert("Can't find suitable material property for UpdateProperty!" && false);
}

void Material::DrawInspector() {
    ImGui::Text("Pass");
    for (auto p : passes_) {
        ImGui::Bullet();
        ImGui::Selectable(p.c_str());
    }

    ImGui::Text("Shader");
    for (auto s : shaders_) {
        if (s) {
            ImGui::Bullet();
            ImGui::Text(ToNarrow(s->file_name()).c_str());
        }
    }

    ImGui::Text("Property");
    for (auto& p : properties_) {
        ImGui::Bullet();
        ImGui::Text(p.name.c_str());
        if (p.prop_type == MaterialPropertyType::kTexture && p.use_default) {
            ImGui::SameLine(200);
            std::string label("##material-color");
            label.append(p.name);
            if (ImGui::ColorEdit4(label.c_str(), &p.default_color, ImGuiColorEditFlags_NoInputs)) {
                p.dirty = true;
                dirty_ = true;
            }
        }
    }
}

Material* MaterialManager::Create(const char* name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return nullptr;
    }

    auto mat = std::make_unique<Material>(name);
    auto ptr = mat.get();
    materials_.emplace(name, std::move(mat));

    return ptr;
}

void MaterialManager::Add(std::unique_ptr<Material>&& mat) {
    auto& name = mat->name();
    auto it = materials_.find(name);
    ///TODO: throw exception;
    assert(it == materials_.end());

    materials_.emplace(name, std::move(mat));
}

void MaterialManager::Remove(const char* name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        materials_.erase(it);
    }
}

Material* MaterialManager::Get(const char* name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return it->second.get();
    }

    return nullptr;
}

}
}
