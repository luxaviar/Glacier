#include "MaterialTemplate.h"
#include <stdexcept>
#include "imgui/imgui.h"
#include "Render/Base/GfxDriver.h"
#include "Render/Graph/PassNode.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

MaterialTemplate::MaterialTemplate(const char* name, const std::shared_ptr<Program>& program) :
    name_(name),
    program_(program)
{
}

MaterialTemplate::MaterialTemplate(const char* name, const TCHAR* vs, const TCHAR* ps) :
    name_(name)
{
    if (vs || ps) {
        program_ = GfxDriver::Get()->CreateProgram(name, vs, ps);
    }
}

void MaterialTemplate::AddPass(const char* pass_name) {
    assert(std::find(passes_.begin(), passes_.end(), pass_name) == passes_.end());
    passes_.push_back(pass_name);
}

bool MaterialTemplate::HasPass(const PassNode* pass) const {
    return std::find(passes_.begin(), passes_.end(), pass->name()) != passes_.end();
}

void MaterialTemplate::SetRasterState(const RasterStateDesc& rs) {
    program_->SetRasterState(rs);
}

void MaterialTemplate::SetInputLayout(const InputLayoutDesc& desc) {
    program_->SetInputLayout(desc);
}

void MaterialTemplate::BindPSO(GfxDriver* gfx) {
    if (program_) {
        program_->BindPSO(gfx);
    }
}

void MaterialTemplate::Bind(GfxDriver* gfx) {
    if (program_) {
        program_->Bind(gfx, this);
    }
}

void MaterialTemplate::UnBind(GfxDriver* gfx) {
    if (program_) {
        program_->UnBind(gfx, this);
    }
}

void MaterialTemplate::SetProgram(const std::shared_ptr<Program>& program) {
    assert(!program_);
    program_ = program;
}

//TODO: check duplicate?
void MaterialTemplate::SetProperty(const char* name, const std::shared_ptr<Buffer>& buf) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
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

void MaterialTemplate::SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color) 
{
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

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

void MaterialTemplate::SetProperty(const char* name, const SamplerState& ss) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : samplers_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kSampler);
            prop.sampler_state = ss;
            prop.dirty = true;
            return;
        }
    }

    samplers_.emplace_back(param, ss);

    //for (auto& prop : properties_) {
    //    if (param == prop.shader_param) {
    //        assert(prop.prop_type == MaterialPropertyType::kSampler);
    //        prop.sampler_state = ss;
    //        prop.dirty = true;
    //        return;
    //    }
    //}

    //properties_.emplace_back(param, ss);
}

void MaterialTemplate::SetProperty(const char* name, const Color& color) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

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

void MaterialTemplate::SetProperty(const char* name, const Vec4f& v) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

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

void MaterialTemplate::SetProperty(const char* name, const Matrix4x4& v) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

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

void MaterialTemplate::UpdateProperty(const char* name, const void* data) {
    auto param = program_->FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    for (auto& prop : properties_) {
        if (param == prop.shader_param) {
            assert(prop.prop_type == MaterialPropertyType::kConstantBuffer);
            prop.buffer->Update(data);
            prop.dirty = true;
            return;
        }
    }

    assert("Can't find suitable material property for UpdateProperty!" && false);
}

void MaterialTemplate::DrawInspector() {
    ImGui::Text("Pass");
    for (auto p : passes_) {
        ImGui::Bullet();
        ImGui::Selectable(p.c_str());
    }

    if (program_) {
        program_->DrawInspector();
    }

    //ImGui::Text("Property");
    //for (auto& p : properties_) {
    //    ImGui::Bullet();
    //    ImGui::Text(p.shader_param->name.c_str());
    //    if (p.prop_type == MaterialPropertyType::kTexture && p.use_default) {
    //        ImGui::SameLine(200);
    //        std::string label("##material-color");
    //        label.append(p.shader_param->name.c_str());
    //        if (ImGui::ColorEdit4(label.c_str(), &p.default_color, ImGuiColorEditFlags_NoInputs)) {
    //            p.dirty = true;
    //        }
    //    }
    //}
}

}
}
