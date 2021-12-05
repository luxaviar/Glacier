#include "transform.h"
#include <algorithm>
#include <assert.h>
#include "gameobject.h"
#include "imgui/imgui.h"
#include "imguizmo/ImGuizmo.h"
#include "Scene.h"
#include "Render/Camera.h"
#include "Input/Input.h"

namespace glacier {

Transform::Transform(const Vec3f& pos, const Quaternion& rot, const Vec3f& scale) :
    noscale_(scale == Vec3f::one),
    version_(1),
    matrix_version_(0),
    inv_matrix_version_(0),
    local_rotation_(rot),
    local_position_(pos), 
    local_scale_(scale),
    parent_(nullptr) 
{
    gizmo_op_ = ImGuizmo::TRANSLATE;
}

Transform::Transform(const Matrix4x4& mat) :
    version_(1),
    matrix_version_(0),
    inv_matrix_version_(0),
    parent_(nullptr)
{
    mat.Decompose(local_position_, local_rotation_, local_scale_);
    noscale_ = (local_scale_ == Vec3f::zero);
}

Transform::~Transform() {
    if (parent_) parent_->RemoveChild(this);
}

void Transform::SetParent(Transform* parent) {
    if (parent_ == parent) return;

    if (parent_) {
        parent_->RemoveChild(this);
    }

    parent_ = parent;
    if (parent_) {
        parent_->AddChild(this);
    }

    MarkDirty();

    if (game_object_) {
        game_object_->SetParent(parent_ ? parent_->game_object_ : nullptr);
    }
}

void Transform::AddChild(Transform* child) {
    assert(std::find(children_.cbegin(), children_.cend(), child) == children_.cend());

    children_.push_back(child);
    child->parent_ = this;
}

void Transform::RemoveChild(Transform* child) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (*it == child) {
            children_.erase(it);
            return;
        }
    }
}

void Transform::local_position(const Vec3f& pos) {
    local_position_ = pos;
    MarkDirty();
}

void Transform::local_rotation(const Quaternion& rot) {
    local_rotation_ = rot;
    MarkDirty();
}

void Transform::local_scale(const Vec3f& scale) {
    local_scale_ = scale;
    noscale_ = (local_scale_ == Vec3f::zero);
    MarkDirty();
}

Vec3f Transform::position() const {
    if (parent_ != nullptr) {
        return parent_->ApplyTransform(local_position_);
    } else {
        return local_position_;
    }
}

void Transform::position(const Vec3f& pos) {
    if (parent_ != nullptr) {
        local_position_ = parent_->InverseTransform(pos);
    } else {
        local_position_ = pos;
    }

    MarkDirty();
}

Quaternion Transform::rotation() const {
    if (parent_ != nullptr) {
        Quaternion rot = local_rotation_;
        Transform* cur = parent_;
        while (cur != nullptr) {
            rot = cur->local_rotation_ * rot;
            cur = cur->parent_;
        }

        return rot;
    } else {
        return local_rotation_;
    }
}

void Transform::rotation(const Quaternion& rot) {
    if (parent_ != nullptr) {
        local_rotation_ = parent_->rotation().Inverse() * rot;
    } else {
        local_rotation_ = rot;
    }

    MarkDirty();
}

//Vec3f Transform::scale() const {
//
//}
//
//void Transform::scale(const Vec3f& scale) {
//
//}

void Transform::forward(const Vec3f& dir) {
    rotation(Quaternion::LookRotation(dir));
}

void Transform::right(const Vec3f& dir) {
    rotation(Quaternion::FromToRotation(Vec3f::right, dir));
}

void Transform::up(const Vec3f& dir) {
    rotation(Quaternion::FromToRotation(Vec3f::up, dir));
}

//local -> world
Vec3f Transform::ApplyTransform(const Vec3f& point) const {
    const Transform* cur = this;
    Vec3f result(point);
    while (cur != nullptr) {
        result *= local_scale_;
        result = cur->local_rotation_ * result;
        result += cur->local_position_;
        cur = cur->parent_;
    }

    return result;
}

//world -> local
Vec3f Transform::InverseTransform(const Vec3f& point) const {
    Vec3f result;
    if (parent_ != nullptr) {
        result = parent_->InverseTransform(point);
    } else {
        result = point;
    }

    result -= local_position_;
    result = local_rotation_.Inverse() * result;
    if (!noscale_) {
        result *= local_scale_.InverseSafe();
    }

    return result;
}

//local -> world
Vec3f Transform::ApplyTransformVector(const Vec3f& dir) const {
    return rotation() * dir;
}

//world -> local
Vec3f Transform::InverseTransformVector(const Vec3f& dir) const {
    return rotation().Inverse() * dir;
}

void Transform::SetPositionAndRotation(const Vec3f& pos, const Quaternion& rot) {
    if (parent_ != nullptr) {
        local_position_ = parent_->InverseTransform(pos);
        local_rotation_ = parent_->rotation().Inverse() * rot;
    } else {
        local_position_ = pos;
        local_rotation_ = rot;
    }

    MarkDirty();
}

const Matrix4x4& Transform::LocalToWorldMatrix() const {
    if (matrix_version_ != version_) {
        if (parent_ != nullptr) {
            local_to_world_ = parent_->LocalToWorldMatrix() * LocalToParentMatrix();
        } else {
            local_to_world_ = LocalToParentMatrix();
        }

        matrix_version_ = version_;
    }

    return local_to_world_;
}

const Matrix4x4& Transform::WorldToLocalMatrix() const {
    if (inv_matrix_version_ == version_) {
        return world_to_local_;
    }

    world_to_local_ = Matrix4x4::TR(local_position_, local_rotation_);
    world_to_local_.InverseOrthonormal();
    if (!noscale_) {
        Matrix4x4 inv_scale = Matrix4x4::Scale(local_scale_.InverseSafe());
        world_to_local_ = inv_scale * world_to_local_;
    }

    if (parent_) {
        Matrix4x4 parent_mat = parent_->WorldToLocalMatrix();
        world_to_local_ = world_to_local_ * parent_mat;
    }

    inv_matrix_version_ = version_;

    return world_to_local_;
}

Matrix4x4 Transform::LocalToParentMatrix() const {
    if (noscale_) {
        return Matrix4x4::TR(local_position_, local_rotation_);
    } else {
        return Matrix4x4::TRS(local_position_, local_rotation_, local_scale_);
    }
}

void Transform::MarkDirty() {
    ++version_;
    for (auto child : children_) {
        child->MarkDirty();
    }
}

void Transform::DrawInspector() {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto pos = position();
        auto rot = rotation();
        auto euler = rot.ToEuler() * math::kRad2Deg;
        auto scale = local_scale_;
        Vec3i change;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Position"); ImGui::SameLine(80);
        if (ImGui::DragFloat3("##tf-position", pos.value, 0.05f)) {
            position(pos);
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotation"); ImGui::SameLine(80);
        if (ImGui::DragFloat3("##tf-rotation", euler.value, 0.05f)) {
            rot = Quaternion::FromEuler(euler * math::kDeg2Rad);
            rotation(rot);
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scale"); ImGui::SameLine(80);
        if (ImGui::DragFloat3("##tf-scale", scale.value, 0.01f)) {
            local_scale(scale);
        }
    }
}

void Transform::OnDrawSelectedGizmos() {
    auto scene = SceneManager::Instance()->CurrentScene();
    if (!scene) return;

    auto camera = scene->GetMainCamera();
    if (!camera) return;

    auto view = camera->view().Transpose();
    auto proj = camera->projection().Transpose();
    auto model = LocalToWorldMatrix().Transpose();

    if (!Input::IsRelativeMode()) {
        if (Input::IsJustKeyDown(Keyboard::T)) {
            gizmo_op_ = ImGuizmo::TRANSLATE;
        } else if (Input::IsJustKeyDown(Keyboard::R)) {
            gizmo_op_ = ImGuizmo::ROTATE;
        }
        else if (Input::IsJustKeyDown(Keyboard::S)) {
            gizmo_op_ = ImGuizmo::SCALE;
        }
    }

    auto op = (ImGuizmo::OPERATION)gizmo_op_;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    if (ImGuizmo::Manipulate(view, proj, op, ImGuizmo::LOCAL, model)) {
        auto m = model.Transpose();
        Vec3f pos, scale;
        Quaternion rot;
        m.Decompose(pos, rot, scale);

        if (op == ImGuizmo::TRANSLATE) {
            position(pos);
        }
        else if (op == ImGuizmo::ROTATE) {
            rotation(rot);
        }
        else {

        }
    }
}

}

