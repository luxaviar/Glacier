#include "Program.h"
#include "imgui/imgui.h"

namespace glacier {
namespace render {

Program::Program(const char* name) : name_(name) {

}

void Program::SetShader(const std::shared_ptr<Shader>& shader) {
    auto type = shader->type();
    shaders_[(int)type] = shader;

    SetupShaderParameter(shader);
}

void Program::Bind() {
    for (auto& shader : shaders_) {
        if (shader)
            shader->Bind();
    }

    //if (pso_) {
    //    pso_->Bind();
    //}
}

void Program::Unbind() {
    for (auto& shader : shaders_) {
        if (shader)
            shader->UnBind();
    }
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
