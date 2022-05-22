#include "material.h"
#include <stdexcept>
#include "common/color.h"
#include "render/base/renderable.h"
#include "imgui/imgui.h"
#include "Render/Graph/PassNode.h"
#include "render/base/Buffer.h"
#include "render/base/texture.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

Material::Material(const char* name, const std::shared_ptr<MaterialTemplate>& temp) :
    name_(name),
    template_(temp)
{
    CloneSamplers();
}

Material::Material(const char* name, const std::shared_ptr<Program>& program) :
    Material(name, std::make_shared<MaterialTemplate>(name, program))
{

}

Material::Material(const char* name, const TCHAR* vs, const TCHAR* ps):
    Material(name, std::make_shared<MaterialTemplate>(name, vs, ps))
{
}

void Material::CloneSamplers() {
    auto& props = template_->GetSamplers();
    for (auto& prop : props) {
        properties_.emplace_back(prop);
    }
}

void Material::Bind(GfxDriver* gfx) {
    template_->GetProgram()->Bind(gfx, this);
}

void Material::UnBind(GfxDriver* gfx) {
    template_->GetProgram()->UnBind(gfx, this);
}

void Material::ReBind(GfxDriver* gfx) {
    template_->GetProgram()->ReBind(gfx, this);
}

void Material::AddPass(const char* pass_name) {
    template_->AddPass(pass_name);
}

bool Material::HasPass(const PassNode* pass) const {
    return template_->HasPass(pass);
}

void Material::SetTexTilingOffset(const Vec4f& st) {
    tex_ts_ = st;
}

//TODO: check duplicate?
void Material::SetProperty(const char* name, const std::shared_ptr<Buffer>& buf) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (prop.shader_param == param && 
            (prop.prop_type == MaterialPropertyType::kConstantBuffer ||
                prop.prop_type == MaterialPropertyType::kStructuredBuffer ||
                prop.prop_type == MaterialPropertyType::kRWStructuredBuffer)) {
            if (prop.buffer != buf) {
                prop.buffer = buf;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, buf);
}

void Material::SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color) 
{
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (prop.prop_type == MaterialPropertyType::kTexture && prop.shader_param == param) {
            if (prop.texture != tex || prop.default_color != default_color)
            {
                prop.texture = tex;
                prop.default_color = default_color;
                prop.use_default = !tex;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, tex, default_color);
}

void Material::SetProperty(const char* name, const Color& color) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kColor);
            if (prop.color != color) {
                prop.color = color;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, color);
}

void Material::SetProperty(const char* name, const Vec4f& v) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kFloat4);
            if (prop.float4 != v) {
                prop.float4 = v;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, v);
}

void Material::SetProperty(const char* name, const Matrix4x4& v) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kMatrix);
            if (prop.matrix != v) {
                prop.matrix = v;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, v);
}

void Material::SetProperty(const char* name, const SamplerState& ss) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kSampler);
            if (prop.sampler_state != ss) {
                prop.sampler_state = ss;
                prop.dirty = true;
            }
            return;
        }
    }

    properties_.emplace_back(param, ss);
}

void Material::UpdateProperty(const char* name, const void* data) {
    auto param = template_->GetProgram()->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kConstantBuffer ||
                prop.prop_type == MaterialPropertyType::kStructuredBuffer ||
                prop.prop_type == MaterialPropertyType::kRWStructuredBuffer);
            prop.buffer->Update(data);
            prop.dirty = true;
            return;
        }
    }

    assert("Can't find suitable material property for UpdateProperty!" && false);
}

void Material::DrawInspector() {
    if (template_) {
        template_->DrawInspector();
    }

    ImGui::Text("Property");
    for (auto& p : properties_) {
        for (auto& param : *p.shader_param) {
            if (!param) continue;

            ImGui::Bullet();
            ImGui::Text(param.name.c_str());
            if (p.prop_type == MaterialPropertyType::kTexture && p.use_default) {
                ImGui::SameLine(200);
                std::string label("##material-color");
                label.append(param.name.c_str());
                if (ImGui::ColorEdit4(label.c_str(), &p.default_color, ImGuiColorEditFlags_NoInputs)) {
                    p.dirty = true;
                    //dirty_ = true;
                }
            }
        }
    }
}

PostProcessMaterial::PostProcessMaterial(const char* name, const TCHAR* ps) :
    Material(name, std::make_shared<MaterialTemplate>(name, TEXT("PostProcessCommon"), ps))
{
    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;

    InputLayoutDesc desc;

    template_->SetRasterState(rs);
    template_->SetInputLayout(desc);

    SamplerState ss;
    ss.filter = FilterMode::kBilinear;
    ss.warpU = ss.warpV = WarpMode::kClamp;
    SetProperty("linear_sampler", ss);
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
