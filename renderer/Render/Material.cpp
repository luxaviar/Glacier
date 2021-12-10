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

MaterialProperty::MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Texture>& tex, const Color& default_color) :
    shader_param(param),
    prop_type(MaterialPropertyType::kTexture),
    default_color(default_color),
    use_default(!tex),
    texture(tex)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const std::shared_ptr<ConstantBuffer>& buf) :
    shader_param(param),
    prop_type(MaterialPropertyType::kCBuffer),
    buffer(buf)
{}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Sampler>& sm) :
    shader_param(param),
    prop_type(MaterialPropertyType::kSampler),
    sampler(sm)
{}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Color& color) :
    shader_param(param),
    prop_type(MaterialPropertyType::kColor),
    color(color)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Vec4f& float4) :
    shader_param(param),
    prop_type(MaterialPropertyType::kFloat4),
    float4(float4)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Matrix4x4& matrix) :
    shader_param(param),
    prop_type(MaterialPropertyType::kMatrix),
    matrix(matrix)
{
}

MaterialProperty::MaterialProperty(const MaterialProperty& other) noexcept :
    dirty(other.dirty),
    shader_param(other.shader_param),
    prop_type(other.prop_type),
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
    shader_param(other.shader_param),
    prop_type(other.prop_type),
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

        texture->Bind(shader_param->shader_type,shader_param->bind_point);
        break;
    case MaterialPropertyType::kSampler:
        sampler->Bind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kCBuffer:
        buffer->Bind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kColor:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Color>(color); //std::make_shared<ConstantBuffer<Color>>(gfx, color);
        } else if (dirty) {
            buffer->Update(&color);
        }
        buffer->Bind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kFloat4:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Vec4f>(float4);// std::make_shared<ConstantBuffer<Vec4f>>(gfx, float4);
        }
        else if (dirty) {
            buffer->Update(&float4);
        }
        buffer->Bind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kMatrix:
        if (!buffer) {
            buffer = gfx->CreateConstantBuffer<Matrix4x4>(matrix);// std::make_shared<ConstantBuffer<Matrix4x4>>(gfx, matrix);
        }
        else if (dirty) {
            buffer->Update(&matrix);
        }
        buffer->Bind(shader_param->shader_type, shader_param->bind_point);
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
        texture->UnBind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kSampler:
        sampler->UnBind(shader_param->shader_type, shader_param->bind_point);
        break;
    case MaterialPropertyType::kColor:
    case MaterialPropertyType::kFloat4:
    case MaterialPropertyType::kMatrix:
    case MaterialPropertyType::kCBuffer:
        buffer->UnBind(shader_param->shader_type, shader_param->bind_point);
        break;
    default:
        throw std::exception{ "Bad parameter type for MaterialProperty::Bind." };
        break;
    }
}

Material::Material(const char* name, const std::shared_ptr<Program>& program) :
    name_(name),
    program_(program)
{
    pso_ = GfxDriver::Get()->CreatePipelineState({});
}

Material::Material(const char* name, const TCHAR* vs, const TCHAR* ps) :
    name_(name)
{
    if (vs || ps) {
        program_ = GfxDriver::Get()->CreateProgram(name, vs, ps);
    }

    pso_ = GfxDriver::Get()->CreatePipelineState({});
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
    if (pso_) {
        pso_->Bind();
    }

    if (program_) {
        program_->Bind();
    }

    for (auto& prop : properties_) {
        prop.Bind(gfx);
    }
}

void Material::UnBind() const {
    if (program_) {
        program_->Unbind();
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

void Material::SetProgram(const std::shared_ptr<Program>& program) {
    assert(!program_);
    program_ = program;
}

void Material::SetTexTilingOffset(const Vec4f& st) {
    tex_ts_ = st;
}

void Material::SetPipelineStateObject(const std::shared_ptr<PipelineState> pso) {
    pso_ = pso;
}

void Material::SetPipelineStateObject(RasterState rs) {
    pso_ = GfxDriver::Get()->CreatePipelineState(rs);
}

//TODO: check duplicate?
void Material::SetProperty(const char* name, const std::shared_ptr<ConstantBuffer>& buf) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (prop.shader_param == param && prop.prop_type == MaterialPropertyType::kCBuffer) {
            prop.buffer = buf;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, buf);
}

void Material::SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color) 
{
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (prop.prop_type == MaterialPropertyType::kTexture && prop.shader_param == param)
        {
            prop.texture = tex;
            prop.default_color = default_color;
            prop.use_default = !tex;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, tex, default_color);
}

void Material::SetProperty(const char* name, const std::shared_ptr<Sampler>& sampler) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (prop.prop_type == MaterialPropertyType::kSampler && param == prop.shader_param) 
        {
            prop.sampler = sampler;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, sampler);
}

void Material::SetProperty(const char* name, const Color& color) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kColor);
            prop.color = color;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, color);
}

void Material::SetProperty(const char* name, const Vec4f& v) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kFloat4);
            prop.float4 = v;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, v);
}

void Material::SetProperty(const char* name, const Matrix4x4& v) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kMatrix);
            prop.matrix = v;
            prop.dirty = true;
            return;
        }
    }

    properties_.emplace_back(param, v);
}

void Material::UpdateProperty(const char* name, const void* data) {
    auto param = program_->FindParameter(name);
    assert(param);

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
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
    
    if (program_) {
        program_->DrawInspector();
    }

    ImGui::Text("Property");
    for (auto& p : properties_) {
        ImGui::Bullet();
        ImGui::Text(p.shader_param->name.c_str());
        if (p.prop_type == MaterialPropertyType::kTexture && p.use_default) {
            ImGui::SameLine(200);
            std::string label("##material-color");
            label.append(p.shader_param->name.c_str());
            if (ImGui::ColorEdit4(label.c_str(), &p.default_color, ImGuiColorEditFlags_NoInputs)) {
                p.dirty = true;
                dirty_ = true;
            }
        }
    }
}

PostProcessMaterial::PostProcessMaterial(const char* name, const TCHAR* ps) :
    Material(name, TEXT("PostProcessCommon"), ps)
{
    RasterState rs;
    rs.depthWrite = false;
    rs.depthFunc = CompareFunc::kAlways;
    pso_ = GfxDriver::Get()->CreatePipelineState(rs);
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
