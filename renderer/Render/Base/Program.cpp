#include "Program.h"
#include "imgui/imgui.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

Program::Program(const char* name) : name_(name) {
    pso_ = GfxDriver::Get()->CreatePipelineState({}, {});
}

void Program::SetShader(const std::shared_ptr<Shader>& shader) {
    auto type = shader->type();
    shaders_[(int)type] = shader;

    SetupShaderParameter(shader);
}

void Program::SetRasterState(const RasterStateDesc& rs) {
    pso_->SetRasterState(rs);
}

void Program::SetInputLayout(const InputLayoutDesc& desc) {
    pso_->SetInputLayout(desc);
}

//void Program::SetSampler(const char* name, const SamplerState& ss) {
//    for (auto it = sampler_params_.begin(); it != sampler_params_.end(); ++it) {
//        if (it->param->name == name) {
//            it->state = ss;
//            return;
//        }
//    }
//
//    auto param = FindParameter(name);
//    assert(param && param->category == ShaderParameterCatetory::kSampler);
//
//    sampler_params_.push_back(SamplerParameter{ ss, param });
//}

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
