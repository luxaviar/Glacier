#include "camera.h"
#include "Math/Util.h"
#include "Render/Mesh/Geometry.h"
#include "Core/GameObject.h"
#include "render/base/renderable.h"
#include "imgui/imgui.h"
#include "render/editor/gizmos.h"
#include "Core/Scene.h"

namespace glacier {
namespace render {

Camera* Camera::Create(CameraType type) {
    auto& go = GameObject::Create("Camera");
    auto camera = go.AddComponent<Camera>(type);
    return camera;
}

Camera::Camera(CameraType type) noexcept :
    type_(type)
{
}

void Camera::LookAt(const Vec3f& target, const Vec3f& up) {
    auto& tx = transform();
    auto  rot = Quaternion::LookRotation(target - tx.position(), up);
    tx.rotation(rot);
}

Vec3f Camera::forward() const {
    return transform().forward();
}

Matrix4x4 Camera::view() const noexcept {
    return Matrix4x4::LookToLH(transform().position(), forward(), Vec3f::up);
}

Matrix4x4 Camera::projection() const noexcept {
    if (type_ == CameraType::kPersp) {
        return Matrix4x4::PerspectiveFovLH(param_[0] * math::kDeg2Rad, param_[1], param_[2], param_[3]);
    }
    else {
        return Matrix4x4::OrthoLH(param_[0] * param_[4], param_[1] * param_[4], param_[2], param_[3]);
    }
}

Matrix4x4 Camera::projection_reversez() const noexcept {
    if (type_ == CameraType::kPersp) {
        return Matrix4x4::PerspectiveFovLH(param_[0] * math::kDeg2Rad, param_[1], param_[3], param_[2]);
    }
    else {
        return Matrix4x4::OrthoLH(param_[0] * param_[4], param_[1] * param_[4], param_[3], param_[2]);
    }
}

Vec3f Camera::CameraToWorld(const Vec3f& pos) const {
    return transform().ApplyTransform(pos);
}

Vec3f Camera::WorldToCamera(const Vec3f& pos) const {
    return transform().InverseTransform(pos);
}

Vec3f Camera::ViewCenter() const {
    float z = nearz() + (farz() - nearz()) / 2.0f;
    return transform().ApplyTransform(Vec3f(0, 0, z));
}

void Camera::FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount]) const {
    FetchFrustumCorners(corners, param_[2], param_[3]);
}

void Camera::FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount], float n, float f) const {
    if (type_ == CameraType::kPersp) {
        geometry::FetchFrustumCorners(corners, type_, param_[0], param_[1], n, f);
    }
    else {
        geometry::FetchFrustumCorners(corners, type_, param_[0] * param_[4], param_[1] * param_[4], n, f);
    }

    for (int i = 0; i < (int)FrustumCorner::kCount; ++i) {
        corners[i] = CameraToWorld(corners[i]);
    }
}

void Camera::Cull(const std::vector<Renderable*>& objects, 
    std::vector<Renderable*>& result, const CullFilter& filter) const 
{
    Frustum frustum(*this);

    result.reserve(objects.size());
    for (auto o : objects) {
        if (o->IsActive() && o->GetMaterial() 
            && frustum.Intersect(o->world_bounds()) && (!filter || filter(o))) {
            result.push_back(o);
        }
    }
}

void Camera::Cull(const List<Renderable>& objects,
    std::vector<Renderable*>& result, const CullFilter& filter) const 
{
    Frustum ft(*this);

    result.reserve(objects.size());
    for (auto& node : objects) {
        auto o = node.data;
        if (o->IsActive() && o->GetMaterial() && 
            ft.Intersect(o->world_bounds()) && (!filter || filter(o))) {
            result.push_back(o);
        }
    }
}

void Camera::OnDrawSelectedGizmos() {
    auto gizmo = Gizmos::Instance();

    if (SceneManager::Instance()->CurrentScene()->GetMainCamera() != this) {
        gizmo->DrawFrustum(*this);
    }
}

void Camera::DrawInspector() {
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        int ty = (int)type_;
        ImGui::Text("Type"); ImGui::SameLine(80);
        if (ImGui::Combo("##camera-type", &ty, "Persp\0Ortho")) {
            if (ty != (int)type_) {
                type_ = (CameraType)ty;
                if (type_ == CameraType::kPersp) {
                    param_[0] = 60.0f;
                    param_[1] = 16.0f / 9.0f;
                    param_[2] = 0.5f;
                    param_[3] = 100.0f;
                } else {
                    param_[0] = 16.0f;
                    param_[1] = 9.0f;
                    param_[2] = 0.5f;
                    param_[3] = 100.0f;
                }
            }
        }

        if (type_ == CameraType::kPersp) {
            ImGui::Text("Fov");
            ImGui::SameLine(80); ImGui::DragFloat("##camera-fov", &param_[0], 0.05f, 1.0f, 169.0f);
            ImGui::Text("Aspect");
            ImGui::SameLine(80); ImGui::DragFloat("##camera-aspect", &param_[1], 0.01f, 0.1f, 10.0f);
            ImGui::Text("Near");
            ImGui::SameLine(80); ImGui::DragFloat("##camera-near", &param_[2], 0.05f, 0.01f, param_[3]);
            ImGui::Text("Far");
            ImGui::SameLine(80); ImGui::DragFloat("##camera-far", &param_[3], 0.05f, param_[2] + 0.001f, 2000.0f);
        } else {
            //ImGui::Text("Width");
            //ImGui::SameLine(80); ImGui::DragFloat("##camera-width", &param_[0], 0.05f, 1.0f, 2000.0f);
            //ImGui::Text("Height");
            //ImGui::SameLine(80); ImGui::DragFloat("##camera-height", &param_[1], 0.01f, 1.0f, 2000.0f);
            //ImGui::Text("Near");
            //ImGui::SameLine(80); ImGui::DragFloat("##camera-near", &param_[2], 0.05f, 0.01f, param_[3]);
            //ImGui::Text("Far");
            //ImGui::SameLine(80); ImGui::DragFloat("##camera-far", &param_[3], 0.05f, param_[2] + 0.001f, 2000.0f);
            ImGui::Text("Scale");
            ImGui::SameLine(80); ImGui::DragFloat("##camera-scale", &param_[4], 0.05f, 0.1f, 10.0f);
        }
    }
}

}
}