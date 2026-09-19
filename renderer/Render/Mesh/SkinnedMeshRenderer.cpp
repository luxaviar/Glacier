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
#include "Render/Base/Program.h"
#include "Render/Skinning/GpuSkinning.h"
#include "Render/Skinning/InstanceBuffer.h"
#include "Common/Log.h"
#include "Inspect/Profiler.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(SkinnedMeshRenderer, SkinnedMeshRenderer)
LUX_FUNC(SkinnedMeshRenderer, SetGpuSkinning)
LUX_FUNC(SkinnedMeshRenderer, SetInstancing)
LUX_PROP_FUNC_GET(SkinnedMeshRenderer, gpu_skinning, gpu_skinning)
LUX_PROP_FUNC_GET(SkinnedMeshRenderer, instancing, instancing)
LUX_IMPL_END

SkinnedMeshRenderer::SkinnedMeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material) :
    MeshRenderer(mesh, material)
{
}

SkinnedMeshRenderer::~SkinnedMeshRenderer() {
    ReleaseBoneSlot();
    ReleaseSkinnedVertices();
}

void SkinnedMeshRenderer::SetGpuSkinning(bool on) {
    if (gpu_skinning_ == on) {
        return;
    }

    gpu_skinning_ = on;
    ReleaseSkinnedVertices();
    //the next update asks the pool for a region again when it is switched on
    resolved_ = false;
}

void SkinnedMeshRenderer::SetInstancing(bool on) {
    instancing_ = on;
    if (on) {
        //a batch draws the bind pose of the mesh and lets the vertex shader of
        //every pass skin its instances with their own bones, which is the
        //opposite of what the compute pass does
        SetGpuSkinning(false);
    }
}

bool SkinnedMeshRenderer::skinned_vertex_buffer() const {
    return gpu_skinning_ && skinned_vertex_offset_ != GpuSkinning::kInvalidOffset;
}

void SkinnedMeshRenderer::ReleaseSkinnedVertices() const {
    if (skinned_vertex_offset_ == GpuSkinning::kInvalidOffset) {
        return;
    }

    GpuSkinning::Instance()->Free(skinned_vertex_offset_);
    skinned_vertex_offset_ = GpuSkinning::kInvalidOffset;
}

uint32_t SkinnedMeshRenderer::bone_offset() const {
    auto pool = BoneMatrixPool::Instance();
    if (bone_slot_ == kInvalidBoneOffset) {
        //no room in the pool: draw the mesh in its bind pose
        return pool->IdentityBoneOffset();
    }

    return pool->FrameOffset(bone_slot_);
}

uint32_t SkinnedMeshRenderer::prev_bone_offset() const {
    auto pool = BoneMatrixPool::Instance();
    if (bone_slot_ == kInvalidBoneOffset) {
        return pool->IdentityBoneOffset();
    }

    return pool->PrevFrameOffset(bone_slot_, bone_slot_bones_);
}

void SkinnedMeshRenderer::ReleaseBoneSlot() const {
    if (bone_slot_ == kInvalidBoneOffset) {
        return;
    }

    BoneMatrixPool::Instance()->Free(bone_slot_, bone_slot_bones_);
    bone_slot_ = kInvalidBoneOffset;
    bone_slot_bones_ = 0;
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
    has_prev_ = false;

    //the pool is handed the bone count of this mesh, not the worst case, and
    //the slot is kept for as long as the mesh is skinned
    size_t count = std::min(bones.size(), (size_t)kMaxBones);
    if (bone_slot_ == kInvalidBoneOffset || bone_slot_bones_ != count) {
        auto pool = BoneMatrixPool::Instance();
        pool->Free(bone_slot_, bone_slot_bones_);

        bone_slot_ = pool->Allocate((uint32_t)count);
        bone_slot_bones_ = bone_slot_ == kInvalidBoneOffset ? 0 : (uint32_t)count;
    }

    bone_matrices_.assign(count, Matrix4x4::identity);
    prev_bone_matrices_.assign(count, Matrix4x4::identity);
    prev_bone_world_.assign(count, Matrix4x4::identity);

    //the compute skinning pass writes the deformed vertices into a region of a
    //shared pool; a mesh the pool has no room for is skinned by the vertex
    //shader of every pass instead
    if (gpu_skinning_ && skinned_vertex_offset_ == GpuSkinning::kInvalidOffset) {
        skinned_vertex_offset_ = GpuSkinning::Instance()->Allocate(mesh.get());
    }

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
    if (!base) {
        MeshRenderer::Render(cmd_buffer, mat);
        return;
    }

    const bool skinned_vertices = skinned_vertex_buffer();
    auto variant = skinned_vertices ?
        base->GetVariant({ "GLACIER_GPU_SKINNED" }, Mesh::kSkinnedVertexLayout) :
        base->GetVariant({ "GLACIER_SKINNING" }, Mesh::kSkinnedLayout);
    if (!variant) {
        MeshRenderer::Render(cmd_buffer, mat);
        return;
    }

    //UpdateRenderData computed the matrices for this frame; only a mesh that
    //appeared after the animation phase has to catch up here
    if (!bone_matrices_cached_) {
        UpdateBoneMatrices();
    }

    UpdatePerObjectData(cmd_buffer);

    cmd_buffer->BindMaterial(variant.get());

    //The bones belong to this object while the material is shared, so they are
    //bound for this draw instead of living in the material: binding a material
    //that did not change does not bind its properties again, and the matrices
    //of the pool are rewritten every frame. A program of the compute skinning
    //path has no such parameter, and binding one that is not there does nothing.
    variant->GetProgram()->BindBuffer(cmd_buffer, "_BoneMatrices", BoneMatrixPool::Instance()->buffer().get());

    if (skinned_vertices) {
        //the compute pass already deformed the vertices this pass draws
        mesh->Draw(cmd_buffer, GpuSkinning::Instance()->vertex_buffer().get(), skinned_vertex_offset_);
    }
    else {
        mesh->Draw(cmd_buffer);
    }
}

void SkinnedMeshRenderer::UpdateRenderData() const {
    Renderable::UpdateRenderData();

    if (!resolved_) {
        ResolveBones();
    }

    UpdateBoneMatrices();
}

void SkinnedMeshRenderer::DispatchSkinning(CommandBuffer* cmd_buffer) const {
    if (!skinned_vertex_buffer()) {
        return;
    }

    auto mesh = skinned_mesh();
    if (!mesh || !mesh->IsSkinned()) {
        return;
    }

    GpuSkinning::Instance()->Dispatch(cmd_buffer, mesh.get(), skinned_vertex_offset_,
        bone_offset(), prev_bone_offset());
}

bool SkinnedMeshRenderer::CanBatchWith(const Renderable* other, Material* mat) const {
    if (!instancing_) {
        return false;
    }

    auto* skinned = dynamic_cast<const SkinnedMeshRenderer*>(other);
    if (!skinned || !skinned->instancing_) {
        return false;
    }

    //one draw call binds the vertex and index buffer of one mesh
    auto mesh = skinned_mesh();
    if (!mesh || mesh != skinned->skinned_mesh()) {
        return false;
    }

    //a batch draws the bind pose and lets the vertex shader of every pass skin
    //it with the bones of the instance, so a mesh the compute pass deformed
    //cannot take part in one
    return !skinned_vertex_buffer() && !skinned->skinned_vertex_buffer();
}

void SkinnedMeshRenderer::RenderBatch(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat) const {
    auto mesh = skinned_mesh();
    auto variant = mat ? mat->GetVariant({ "GLACIER_SKINNING", "GLACIER_INSTANCING" }, Mesh::kSkinnedLayout) : nullptr;
    if (!mesh || !variant) {
        for (auto o : objs) {
            o->Render(cmd_buffer, mat);
        }
        return;
    }

    //one record per instance: everything of an object that the shared per object
    //constant buffer cannot carry for a whole batch
    std::vector<InstanceData> instances(objs.size());
    for (size_t i = 0; i < objs.size(); ++i) {
        auto* renderer = static_cast<const SkinnedMeshRenderer*>(objs[i]);
        auto& instance = instances[i];

        instance.model = renderer->transform().LocalToWorldMatrix();
        instance.prev_model = renderer->prev_model_;
        instance.tex_tile_scale = mat->GetTexTilingOffset();
        instance.bone_offset = renderer->bone_offset();
        instance.prev_bone_offset = renderer->prev_bone_offset();
    }

    uint32_t offset = InstanceBuffer::Instance()->Upload(instances.data(), (uint32_t)instances.size());
    if (offset == InstanceBuffer::kInvalidOffset) {
        for (auto o : objs) {
            o->Render(cmd_buffer, mat);
        }
        return;
    }

    if (batch_size_ != instances.size()) {
        batch_size_ = instances.size();
        LOG_LOG("instanced draw: {0} instances of mesh '{1}'", instances.size(), mesh->name());
    }

    //the batch is drawn from the shared bind pose of the mesh, and the pass
    //reads the object data and the bones of every instance from the pool
    objs.front()->UpdateBatchData(cmd_buffer, offset);
    cmd_buffer->BindMaterial(variant.get());

    auto* program = variant->GetProgram().get();
    program->BindBuffer(cmd_buffer, "_BoneMatrices", BoneMatrixPool::Instance()->buffer().get());
    program->BindBuffer(cmd_buffer, "_Instances", InstanceBuffer::Instance()->buffer().get());

    mesh->DrawInstanced(cmd_buffer, (uint32_t)objs.size());
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
        bone_matrices_[i] = space_to_mesh * pose_matrix * bones[i].offset_matrix;
        prev_bone_matrices_[i] = prev_space_to_mesh * (has_prev ? prev_bone_world_[i] : pose_matrix) * bones[i].offset_matrix;
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

    //hand the pose of this frame to the pool: every pass draws from it instead
    //of uploading the matrices again
    BoneMatrixPool::Instance()->Update(bone_slot_, bone_matrices_, prev_bone_matrices_);
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

    bool gpu_skinning = gpu_skinning_;
    if (ImGui::Checkbox("GPU skinning", &gpu_skinning)) {
        SetGpuSkinning(gpu_skinning);
    }

    if (!skinned_vertex_buffer()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "skinned by the vertex shader");
    }
}

}
}
