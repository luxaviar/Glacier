#include "material.h"
#include <stdexcept>
#include "common/color.h"
#include "Render/Base/Renderable.h"
#include "imgui/imgui.h"
#include "Render/Graph/PassNode.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/Texture.h"
#include "Render/Base/Program.h"
#include "Common/Log.h"
#include "App.h"
#include "Render/Renderer.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

Material::Material(const char* name, const std::shared_ptr<Program>& program) :
    name_(name),
    program_(program)
{

}

Material::Material(const char* name, const TCHAR* vs, const TCHAR* ps):
    Material(name, Program::Create(name, vs, ps))
{
}

void Material::SetupBuiltinProperty() {
    setup_builtin_props_ = true;
    auto renderer = App::Self()->GetRenderer();
    renderer->SetupBuiltinProperty(this);
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
    assert(prop.prop_type == MaterialPropertyType::kConstantBuffer ||
        prop.prop_type == MaterialPropertyType::kStructuredBuffer ||
        prop.prop_type == MaterialPropertyType::kRWStructuredBuffer);

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
    assert(prop.prop_type == MaterialPropertyType::kTexture && prop.shader_param == param);

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
    assert(prop.prop_type == MaterialPropertyType::kColor && prop.shader_param == param);

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
    assert(prop.prop_type == MaterialPropertyType::kFloat4 && prop.shader_param == param);

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
    assert(prop.prop_type == MaterialPropertyType::kMatrix && prop.shader_param == param);

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
    assert(prop.prop_type == MaterialPropertyType::kSampler && prop.shader_param == param);

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
        assert(prop.prop_type == MaterialPropertyType::kConstantBuffer && prop.shader_param == param);
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
        for (auto& param : p.shader_param->entries) {
            if (!param) continue;

            ImGui::Bullet();
            ImGui::Text(p.shader_param->name.c_str());
            if (p.prop_type == MaterialPropertyType::kTexture && p.use_default) {
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

    if (program_->FindParameter("linear_sampler")) {
        SamplerState linear_ss;
        linear_ss.filter = FilterMode::kBilinear;
        linear_ss.warpU = linear_ss.warpV = WarpMode::kClamp;
        SetProperty("linear_sampler", linear_ss);
    }

    if (program_->FindParameter("point_sampler")) {
        SamplerState point_ss;
        point_ss.filter = FilterMode::kPoint;
        point_ss.warpU = point_ss.warpV = WarpMode::kClamp;
        SetProperty("point_sampler", point_ss);
    }
}

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
