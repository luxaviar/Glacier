#include "Program.h"
#include "imgui/imgui.h"
#include "GfxDriver.h"
#include "Render/Graph/PassNode.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

std::shared_ptr<Program> Program::Create(const char* name, const TCHAR* vs, const TCHAR* ps) {
    return GfxDriver::Get()->CreateProgram(name, vs, ps);
}

std::shared_ptr<Program> Program::Create(const char* name, const TCHAR* cs) {
    auto program = GfxDriver::Get()->CreateProgram(name);
    program->SetComputeShader(cs);
    return program;
}

Program::Program(const char* name) : name_(name) {
    pso_ = GfxDriver::Get()->CreatePipelineState(this, {}, {});
    pso_->SetName(ToWide(name).c_str());
}

void Program::AddPass(const char* pass_name) {
    assert(std::find(passes_.begin(), passes_.end(), pass_name) == passes_.end());
    passes_.push_back(pass_name);
}

bool Program::HasPass(const PassNode* pass) const {
    return std::find(passes_.begin(), passes_.end(), pass->name()) != passes_.end();
}

void Program::SetVertexShader(const TCHAR* vs) {
    auto shader = GfxDriver::Get()->CreateShader(ShaderType::kVertex, vs);
    SetShader(shader);
}

void Program::SetPixelShader(const TCHAR* ps) {
    auto shader = GfxDriver::Get()->CreateShader(ShaderType::kPixel, ps);
    SetShader(shader);
}

void Program::SetComputeShader(const TCHAR* cs) {
    auto shader = GfxDriver::Get()->CreateShader(ShaderType::kCompute, cs);
    SetShader(shader);
}

void Program::SetShader(const std::shared_ptr<Shader>& shader) {
    auto type = shader->type();
    assert(!GetShader(type));

    shaders_[(int)type] = shader;

    SetupShaderParameter(shader);
}

void Program::SetRasterState(const RasterStateDesc& rs) {
    pso_->SetRasterState(rs);
}

void Program::SetInputLayout(const InputLayoutDesc& desc) {
    pso_->SetInputLayout(desc);
}

void Program::SetupMaterial(Material* material) {
    material->propgram_version_ = version_;
    auto& mat_props = material->properties_;

    for (auto& [name, prop] : properties_) {
        auto it = mat_props.find(name);
        if (it == mat_props.end()) {
            mat_props.emplace_hint(it, name, prop);
        }
    }
}

void Program::SetShaderParameter(const std::string& name, const ShaderParameter& param) {
    assert(params_.find(name) == params_.end());
    params_[name] = param;
}

const ShaderParameter* Program::FindParameter(const std::string& name) const {
    auto it = params_.find(name);
    if (it != params_.end()) {
        return &it->second;
    }

    return nullptr;
}

const ShaderParameter* Program::FindParameter(const char* name) const {
    auto it = params_.find(name);
    if (it != params_.end()) {
        return &it->second;
    }

    return nullptr;
}

bool Program::HasProperty(const char* name) {
    auto it = properties_.find(name);
    return it != properties_.end();
}

void Program::SetProperty(const char* name, const std::shared_ptr<Buffer>& buf) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        properties_.emplace_hint(it, name, MaterialProperty{ param, buf });
        return;
    }

    auto& prop = it->second;
    assert(prop.shader_param == param);
    assert(param->type != ShaderParameterType::kTexture && param->type != ShaderParameterType::kSampler);

    prop.resource = buf;
    prop.dirty = true;
}

void Program::SetProperty(const char* name, const std::shared_ptr<Texture>& tex, const Color& default_color)
{
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

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

void Program::SetProperty(const char* name, const SamplerState& ss) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

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

void Program::SetProperty(const char* name, const Color& color) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

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

void Program::SetProperty(const char* name, const Vec4f& v) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

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

void Program::SetProperty(const char* name, const Matrix4x4& v) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

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

void Program::UpdateProperty(const char* name, const void* data) {
    auto param = FindParameter(name);
    if (!param) {
        LOG_WARN("Set material template property failed for {0}.{1}", name_, name);
        return;
    }

    ++version_;

    auto it = properties_.find(name);

    if (it != properties_.end()) {
        auto& prop = it->second;
        assert(param->type == ShaderParameterType::kConstantBuffer && prop.shader_param == param);
        dynamic_cast<Buffer*>(prop.resource.get())->Update(data);
        return;
    }

    assert("Can't find suitable material property for UpdateProperty!" && false);
}

void Program::DrawInspector() {
    ImGui::Text("Shader");
    for (auto s : shaders_) {
        if (s) {
            ImGui::Bullet();
            ImGui::Text(ToNarrow(s->file_name()).c_str());
        }
    }
}

}
}
