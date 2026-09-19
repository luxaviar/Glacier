#include "material.h"
#include <stdexcept>
#include "common/color.h"
#include "Render/Base/Renderable.h"
#include <imgui.h>
#include "Render/Graph/PassNode.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/Texture.h"
#include "Render/Base/Program.h"
#include "Common/Log.h"
#include "App.h"
#include "Render/Renderer.h"
#include "Inspect/Profiler.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(Material, Material)
LUX_CTOR(Material, const char*)
LUX_IMPL_END

Material::Material(const char* name) :
    Material(name, Program::Create(name, nullptr, nullptr))
{

}

Material::Material(const char* name, const std::shared_ptr<Program>& program) :
    name_(name),
    program_(program)
{

}

Material::Material(const char* name, const TCHAR* vs, const TCHAR* ps) :
    Material(name, Program::Create(name, vs, ps))
{
}

Material::Material(const char* name, const TCHAR* cs) :
    Material(name, Program::Create(name, cs))
{

}

void Material::SetupBuiltinProperty() {
    setup_builtin_props_ = true;
    auto renderer = App::Self()->GetRenderer();
    renderer->SetupBuiltinProperty(this);
}

std::shared_ptr<Material> Material::GetVariant(const std::vector<const char*>& macro_list, const InputLayoutDesc& layout) const {
    std::string macros_name;
    for (const char* macro : macro_list) {
        macros_name += "_";
        macros_name += macro;
    }

    auto variant_name = name_ + macros_name;
    auto key = variant_name + "#" + std::to_string(layout.signature());

    auto cached = variants_.find(key);
    if (cached != variants_.end()) {
        return cached->second;
    }

    //permutation of the same shader files with skinning enabled; the macro list
    //must be null terminated because it is handed to D3DCompile as-is
    //the macro list handed to D3DCompile has to be null terminated
    std::vector<ShaderMacroEntry> macros;
    macros.reserve(macro_list.size() + 1);
    for (const char* macro : macro_list) {
        macros.push_back({ macro, "1" });
    }
    macros.push_back({ nullptr, nullptr });

    auto gfx = GfxDriver::Get();
    auto program = gfx->CreateProgram(variant_name.c_str());

    bool has_shader = false;
    for (uint32_t i = 0; i < (uint32_t)ShaderType::kUnknown; ++i) {
        auto type = (ShaderType)i;
        auto shader = program_->GetShader(type);
        if (!shader) continue;

        program->SetShader(gfx->CreateShader(type, shader->file_name().c_str(), nullptr, macros));
        has_shader = true;
    }

    if (!has_shader) {
        return nullptr;
    }

    program->SetInputLayout(layout);

    //a permutation takes part in the same render passes as the material it derives from
    for (const auto& pass : program_->passes()) {
        program->AddPass(pass.c_str());
    }

    auto variant = std::make_shared<Material>(variant_name.c_str(), program);
    variant->tex_ts_ = tex_ts_;

    //copy the property values over and re-link them to the new program, whose
    //parameters live at the same names but different addresses
    for (const auto& [key, prop] : properties_) {
        auto param = program->FindParameter(key);
        if (!param) continue;

        MaterialProperty copy = prop;
        copy.shader_param = param;
        copy.dirty = true;
        variant->properties_.emplace(key, copy);
    }

    //setup_builtin_props_ is deliberately left false: binding the variant must
    //still install the builtin properties it did not inherit (_PerFrameData)
    variants_.emplace(key, variant);
    return variant;
}

void Material::Bind(CommandBuffer* cmd_buffer) {
    PerfSample("Material Binding");

    if (!setup_builtin_props_) {
        SetupBuiltinProperty();
    }

    if (propgram_version_ != program_->version()) {
        program_->SetupMaterial(this);
    }

    program_->Bind(cmd_buffer, this);
}

void Material::AddPass(const char* pass_name) {
    program_->AddPass(pass_name);
}

bool Material::HasPass(const PassNode* pass) const {
    return program_->HasPass(pass);
}

bool Material::HasParameter(const char* name) const {
    return program_->FindParameter(name) != nullptr;
}

void Material::SetTexTilingOffset(const Vec4f& st) {
    tex_ts_ = st;
}

void Material::SetProperty(const char* name, const std::shared_ptr<Buffer>& buf) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, buf });
        return;
    }

    auto& prop = it->second;
    assert(prop.shader_param == param);
    assert(param->type != ShaderParameterType::kTexture &&
        param->type != ShaderParameterType::kSampler);

    prop.resource = buf;
    prop.dirty = true;
}

void Material::SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color)
{
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, tex, default_color });
        return;
    }

    auto& prop = it->second;
    assert(param->type == ShaderParameterType::kTexture && prop.shader_param == param);

    prop.resource = tex;
    prop.default_color = default_color;
    prop.use_default = !tex;
    prop.dirty = true;
}

void Material::SetProperty(const char* name, const Color& color) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, color });
        return;
    }

    auto& prop = it->second;
    assert(prop.prop_type == ConstantPropertyType::kColor && prop.shader_param == param);

    prop.color = color;
    prop.dirty = true;
}

void Material::SetProperty(const char* name, const Vec4f& v) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, v });
        return;
    }

    auto& prop = it->second;
    assert(prop.prop_type == ConstantPropertyType::kFloat4 && prop.shader_param == param);

    prop.float4 = v;
    prop.dirty = true;
}

void Material::SetProperty(const char* name, const Matrix4x4& v) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, v });
        return;
    }

    auto& prop = it->second;
    assert(prop.prop_type == ConstantPropertyType::kMatrix && prop.shader_param == param);

    prop.matrix = v;
    prop.dirty = true;
}

void Material::SetProperty(const char* name, const SamplerState& ss) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, ss });
        return;
    }

    auto& prop = it->second;
    assert(param->type == ShaderParameterType::kSampler && prop.shader_param == param);

    prop.sampler_state = ss;
    prop.dirty = true;
}

void Material::UpdateProperty(const char* name, const void* data) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    auto it = properties_.find(name);

    if (it != properties_.end()) {
        auto& prop = it->second;
        assert(param->type == ShaderParameterType::kConstantBuffer && prop.shader_param == param);
        dynamic_cast<Buffer*>(prop.resource.get())->Update(data);
        return;
    }

    assert("Can't find suitable material property for UpdateProperty!" && false);
}

void Material::DrawInspector() {
    if (program_) {
        program_->DrawInspector();
    }

    ImGui::Text("Property");
    for (auto& [_, p] : properties_) {
        auto param = p.shader_param;
        ImGui::Bullet();
        ImGui::Text(p.shader_param->name.c_str());
        if (param->type == ShaderParameterType::kTexture && p.use_default) {
            ImGui::SameLine(200);
            std::string label("##material-color");
            label.append(p.shader_param->name.c_str());
            if (ImGui::ColorEdit4(label.c_str(), &p.default_color, ImGuiColorEditFlags_NoInputs)) {
                p.dirty = true;
                //dirty_ = true;
            }
        }
    }
}

PostProcessMaterial::PostProcessMaterial(const char* name, const TCHAR* ps) :
    Material(name, Program::Create(name, TEXT("PostProcessCommon"), ps))
{
    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;

    InputLayoutDesc desc;

    program_->SetRasterState(rs);
    program_->SetInputLayout(desc);
}

LUX_IMPL(MaterialManager, MaterialManager)
// LUX_CTOR(Material, const char*)
LUX_FUNC(MaterialManager, Instance)
LUX_FUNC(MaterialManager, Get)
LUX_IMPL_END

void MaterialManager::Add(std::shared_ptr<Material>&& mat) {
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

const std::shared_ptr<Material>& MaterialManager::Get(const char* name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return it->second;
    }

    return nullptr;
}

}
}
