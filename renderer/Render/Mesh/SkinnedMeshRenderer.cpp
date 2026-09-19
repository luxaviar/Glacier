#include "SkinnedMeshRenderer.h"
#include <algorithm>
#include <imgui.h>
#include "Animation/NodeLookup.h"
#include "Core/Transform.h"
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

    for (size_t i = 0; i < bones.size(); ++i) {
        bone_transforms_[i] = FindNodeTransform(transform(), bones[i].name.c_str());
    }

    size_t missing = (size_t)std::count(bone_transforms_.begin(), bone_transforms_.end(), nullptr);
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
    const auto& world_to_local = transform().WorldToLocalMatrix();

    size_t count = std::min(bones.size(), (size_t)kMaxBones);
    bool has_prev = has_prev_ && prev_bone_world_.size() == bones.size();

    for (size_t i = 0; i < count; ++i) {
        auto* bone = bone_transforms_[i];
        Matrix4x4 world = bone ? bone->LocalToWorldMatrix() : Matrix4x4::identity;

        //the mesh transform cancels out again in the vertex shader, so the
        //matrices are expressed in mesh space
        bone_matrices_.bones[i] = world_to_local * world * bones[i].offset_matrix;
        bone_matrices_.prev_bones[i] = prev_world_to_local_ * (has_prev ? prev_bone_world_[i] : world) * bones[i].offset_matrix;
    }

    for (size_t i = 0; i < count; ++i) {
        auto* bone = bone_transforms_[i];
        prev_bone_world_[i] = bone ? bone->LocalToWorldMatrix() : Matrix4x4::identity;
    }

    prev_world_to_local_ = world_to_local;
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
