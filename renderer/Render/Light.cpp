#include "light.h"
#include "camera.h"
#include "Math/Util.h"
#include "Core/GameObject.h"
#include "imgui/imgui.h"
#include "LightManager.h"
#include "render/base/Buffer.h"

namespace glacier {
namespace render {

Light::Light(LightType type, const Color& color, float intensity)
{
    data_.type = type;
    data_.intensity = intensity;
    data_.enable = true;
    SetColor(color);
}

void Light::OnEnable() {
    LightManager::Instance()->Add(this);
}

void Light::OnDisable() {
    LightManager::Instance()->Remove(this);
}

void Light::SetColor(const Color& color) {
    color_ = color;
    data_.color = GammaToLinearSpace(color);
}

float Light::AttenuateApprox(float dist_sq) const {
    return 1.0f / (kConstantFactor + (kQuadraticFactor / (data_.range * data_.range)) * dist_sq);
}

float Light::LuminessAtPoint(const Vec3f& pos) {
    auto lum = ColorToGreyValue(data_.color) * data_.intensity;
    if (data_.type == LightType::kDirectinal) {
        if (data_.shadow_enable) return lum * 16;
        return lum;
    } else {
        float dist_sq = (pos - Vec3f(data_.position.x, data_.position.y, data_.position.z)).MagnitudeSq();
        float atten = AttenuateApprox(dist_sq);
        return lum * atten;
    }
}

void Light::DrawInspector() {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {

        ImGui::Text("Type");
        int ty = (int)data_.type;
        ImGui::SameLine(80); ImGui::Combo("##light-type", &ty, "Directional\0Point\0Spot");

        ImGui::Text("Color");
        ImGui::SameLine(80);
        if (ImGui::ColorEdit4("##light-color", color_.value)) {
            SetColor(color_);
        }

        ImGui::Text("Intensity");
        ImGui::SameLine(80); ImGui::DragFloat("##light-intensity", &data_.intensity, 0.05f, 0.0f, 1000.0f);
        ImGui::Text("Shadow");
        ImGui::SameLine(80); ImGui::Checkbox("##light-shadow", (bool*)&data_.shadow_enable);

        if (data_.type == LightType::kPoint || data_.type == LightType::kPoint) {
            ImGui::Text("Range");
            ImGui::SameLine(80); ImGui::DragFloat("##light-range", &data_.range, 0.05f, 0.0f, 1000.0f);
        }
    }
}

DirectionalLight::DirectionalLight(const Color& color, float intensity) :
    Light(LightType::kDirectinal, color, intensity)
{
}

void DirectionalLight::Update(GfxDriver* gfx) noexcept {
    auto& view_mat = gfx->view();
    auto lookat = transform().forward();
    data_.position = lookat;
    data_.view_position = view_mat.MultiplyVector(lookat);
}

PointLight::PointLight(float range, const Color& color, float intensity) :
    Light(LightType::kPoint, color, intensity)
{
    data_.range = range;
}

void PointLight::Update(GfxDriver* gfx) noexcept {
    data_.position = transform().position();

    auto& view_mat = gfx->view();
    Vec3f pos{ data_.position.x, data_.position.y, data_.position.z };
    auto pos_vs = view_mat.MultiplyPoint3X4(pos);

    data_.view_position = pos_vs;
    data_.enable = IsActive();
}

OldPointLight::OldPointLight( GfxDriver& gfx, Vec3f pos,float radius ) {
    light_cbuffer_ = gfx.CreateConstantBuffer<PointLightCBuf>();
    light_data_ = {
        pos,
        { 0.05f,0.05f,0.05f },
        { 1.0f,1.0f,1.0f },
        1.0f,
        1.0f,
        0.045f,
        0.0075f,
    };
}

void OldPointLight::Bind(const Matrix4x4& view) const noexcept
{
    auto dataCopy = light_data_;
    dataCopy.pos = view.MultiplyPoint3X4(light_data_.pos);

    light_cbuffer_->Update(&dataCopy );
    //light_cbuffer_->Bind(ShaderType::kPixel, 0);
}

}
}
