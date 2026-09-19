#include "SkinnedMeshRenderer.h"
#include <algorithm>
#include <imgui.h>
#include "Animation/NodeLookup.h"
#include "Animation/Animator.h"
#include "Core/Transform.h"
#include "Core/GameObject.h"
#include "Render/Graph/PassNode.h"
#include "Render/Material.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/Buffer.h"
#include "Common/Log.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

SkinnedMeshRenderer::SkinnedMeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material) :
    MeshRenderer(mesh, material)
{
}

std::shared_ptr<Mesh> SkinnedMeshRenderer::skinned_mesh() const {
    const auto& list = meshes();
    if (list.empty()) {
        return nullptr;
    }

    return list.front();
}

size_t SkinnedMeshRenderer::bone_count() const {
    auto mesh = skinned_mesh();
    return mesh ? mesh->bones().size() : 0;
}

void SkinnedMeshRenderer::RefreshBones() {
    resolved_ = false;
    bone_matrices_cached_ = false;
}

void SkinnedMeshRenderer::SetNodeTable(std::shared_ptr<const NodeTransformTable> nodes) {
    node_table_ = std::move(nodes);
    RefreshBones();
}

void SkinnedMeshRenderer::ResolveBones() const {
    resolved_ = true;
    bone_matrices_cached_ = false;

    auto mesh = skinned_mesh();
    if (!mesh || !mesh->IsSkinned()) {
        return;
    }

    const auto& bones = mesh->bones();
    bone_transforms_.assign(bones.size(), nullptr);
    prev_bone_world_.assign(bones.size(), Matrix4x4::identity);
    has_prev_ = false;

    if (!node_table_) {
        LOG_WARN("SkinnedMeshRenderer '{}': {} joints without a node table keep the bind pose",
            game_object() ? game_object()->name() : "<none>", bones.size());
        return;
    }

    size_t missing = 0;
    for (size_t i = 0; i < bones.size(); ++i) {
        //the importer already picked the node the joint belongs to, so the
        //indices are only checked against the table of this instance
        int32_t node = bones[i].node;
        if (node >= 0 && (size_t)node < node_table_->size()) {
            bone_transforms_[i] = (*node_table_)[node];
        }

        if (!bone_transforms_[i]) {
            ++missing;
        }
    }

    LOG_LOG("SkinnedMeshRenderer '{}': resolved {} of {} bones",
        game_object() ? game_object()->name() : "<none>", bones.size() - missing, bones.size());

    if (missing > 0) {
        LOG_WARN("SkinnedMeshRenderer '{}': {} of {} bones were not found in the hierarchy",
            game_object() ? game_object()->name() : "<none>", missing, bones.size());
    }
}

void SkinnedMeshRenderer::Render(CommandBuffer* cmd_buffer, Material* mat) const {
    auto mesh = skinned_mesh();
    if (!mesh || !mesh->IsSkinned()) {
        MeshRenderer::Render(cmd_buffer, mat);
        return;
    }

    if (!resolved_) {
        ResolveBones();
    }

    //the pass may hand us its own material (shadows, prepass, ...), so the
    //skinned variant is derived from whatever material is being bound
    auto* base = mat ? mat : GetMaterial().get();
    auto variant = base ? base->GetSkinnedVariant(Mesh::kSkinnedLayout) : nullptr;
    if (!variant) {
        MeshRenderer::Render(cmd_buffer, mat);
        return;
    }

    //UpdateRenderData computed the matrices for this frame; only a mesh that
    //appeared after the animation phase has to catch up here
    if (!bone_matrices_cached_) {
        UpdateBoneMatrices();
    }

    //the constant buffer is transient and shared by every skinned mesh, so each
    //draw still uploads the matrices it needs
    GetBoneData()->Update(&bone_matrices_);
    UpdatePerObjectData(cmd_buffer);

    cmd_buffer->BindMaterial(variant.get());
    mesh->Draw(cmd_buffer);
}

void SkinnedMeshRenderer::UpdateRenderData() const {
    Renderable::UpdateRenderData();

    if (!resolved_) {
        ResolveBones();
    }

    UpdateBoneMatrices();
}

void SkinnedMeshRenderer::UpdateBoneMatrices() const {
    auto mesh = skinned_mesh();
    if (!mesh || !mesh->IsSkinned()) {
        return;
    }

    PerfSample("Update bone matrices");

    const auto& bones = mesh->bones();
    size_t count = std::min(bones.size(), (size_t)kMaxBones);

    //the animator of the instance can hand over the pose as a flat set of bone
    //matrices, which keeps the Transform tree out of the loop entirely
    if (!animator_ && game_object()) {
        animator_ = const_cast<GameObject*>(game_object())->GetComponentInParent<Animator>();
    }

    const std::vector<Matrix4x4>* pose = animator_ ? animator_->bone_matrices() : nullptr;
    if (pose && pose->size() != animator_->bone_count()) {
        pose = nullptr;
    }

    const bool pose_based = pose != nullptr && node_index_ < pose->size();
    if (pose_based && animator_->bone_animated(node_index_)) {
        //the shader places the skinned vertices with the transform of this node,
        //so it has to follow the pose when the clip drives the node itself
        Vec3f position;
        Quaternion rotation;
        Vec3f scale;
        if (animator_->bone_pose(node_index_, position, rotation, scale)) {
            auto& tx = const_cast<Transform&>(transform());
            tx.local_position(position);
            tx.local_rotation(rotation);
            tx.local_scale(scale);
        }
    }

    //bone matrices are built in the object space of the mesh, so the skinning
    //matrices live in the same space the vertex shader expects
    Matrix4x4 space_to_mesh;
    if (pose_based) {
        auto mesh_to_space = (*pose)[node_index_].Inverted();
        space_to_mesh = mesh_to_space ? *mesh_to_space : Matrix4x4::identity;
    }
    else {
        space_to_mesh = transform().WorldToLocalMatrix();
    }

    bool has_prev = has_prev_ && prev_bone_world_.size() == bones.size() && prev_pose_based_ == pose_based;
    const Matrix4x4& prev_space_to_mesh = prev_world_to_local_;

    for (size_t i = 0; i < count; ++i) {
        Matrix4x4 pose_matrix = Matrix4x4::identity;
        if (pose_based) {
            int32_t node = bones[i].node;
            if (node >= 0 && (size_t)node < pose->size()) {
                pose_matrix = (*pose)[(size_t)node];
            }
        }
        else if (auto* bone = bone_transforms_[i]) {
            pose_matrix = bone->LocalToWorldMatrix();
        }

        //the mesh transform cancels out again in the vertex shader, so the
        //matrices are expressed in mesh space
        bone_matrices_.bones[i] = space_to_mesh * pose_matrix * bones[i].offset_matrix;
        bone_matrices_.prev_bones[i] = prev_space_to_mesh * (has_prev ? prev_bone_world_[i] : pose_matrix) * bones[i].offset_matrix;
    }

    for (size_t i = 0; i < count; ++i) {
        if (pose_based) {
            int32_t node = bones[i].node;
            prev_bone_world_[i] = node >= 0 && (size_t)node < pose->size() ? (*pose)[(size_t)node] : Matrix4x4::identity;
        }
        else {
            auto* bone = bone_transforms_[i];
            prev_bone_world_[i] = bone ? bone->LocalToWorldMatrix() : Matrix4x4::identity;
        }
    }

    prev_world_to_local_ = space_to_mesh;
    prev_pose_based_ = pose_based;
    has_prev_ = true;
    bone_matrices_cached_ = true;
}

void SkinnedMeshRenderer::DrawInspector() {
    MeshRenderer::DrawInspector();

    if (!ImGui::CollapsingHeader("SkinnedMeshRenderer")) {
        return;
    }

    if (!resolved_) {
        ResolveBones();
    }

    int missing = (int)std::count(bone_transforms_.begin(), bone_transforms_.end(), nullptr);
    ImGui::Text("bones: %d", (int)bone_transforms_.size());
    if (missing > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "unresolved: %d", missing);
    }
}

}
}
