#pragma once

#include <memory>
#include <vector>
#include "Render/Mesh/MeshRenderer.h"
#include "Animation/NodeLookup.h"
#include "Render/Skinning/GpuSkinning.h"
#include "Math/Mat4.h"

namespace glacier {

class Animator;
class Transform;

namespace render {

//Renders a skinned Mesh. The bone hierarchy is not owned by this component:
//bones are looked up by name in the GameObject hierarchy and driven by the
//Animator (or by script), exactly like any other node transform.
class SkinnedMeshRenderer : public MeshRenderer {
public:
    SkinnedMeshRenderer() {}
    SkinnedMeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material = {});
    ~SkinnedMeshRenderer();

    void Render(CommandBuffer* cmd_buffer, Material* mat = nullptr) const override;
    //draws one batch of skinned meshes that share the mesh of this one
    bool CanBatchWith(const Renderable* other, Material* mat) const override;
    void RenderBatch(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat) const override;
    //recomputes the bone matrices of this frame; the render path only uploads
    //what this produced
    void UpdateRenderData() const override;
    //deforms the vertices of this mesh on the GPU, once per frame
    void DispatchSkinning(CommandBuffer* cmd_buffer) const override;
    void DrawInspector() override;

    //Skins the mesh with a compute pass into a shared pool of skinned vertices
    //instead of in the vertex shader of every pass. On by default, and ignored
    //for a mesh the pool has no room for.
    void SetGpuSkinning(bool on);
    bool gpu_skinning() const { return gpu_skinning_; }

    //Draws the meshes that share a mesh and a material with one instanced draw
    //call. A batch shares the bind pose of the mesh, so every instance brings
    //its own bones and the mesh is skinned by the vertex shader; a mesh that is
    //skinned by the compute pass is therefore never batched.
    void SetInstancing(bool on);
    bool instancing() const { return instancing_; }

    //where the bones of this mesh are in the shared bone matrix pool
    uint32_t bone_offset() const override;
    uint32_t prev_bone_offset() const override;

    //re-resolves the bone transforms; the first Render does this automatically
    void RefreshBones();
    //the node table of the instance the mesh belongs to; the bones of the mesh
    //address it by index, so nodes that share a name cannot be confused
    void SetNodeTable(std::shared_ptr<const NodeTransformTable> nodes);
    //index of the node of the instance this mesh hangs off, see Model::Node
    void SetNodeIndex(uint32_t index) { node_index_ = index; }
    size_t bone_count() const;

private:
    std::shared_ptr<Mesh> skinned_mesh() const;
    void ResolveBones() const;
    void UpdateBoneMatrices() const;
    //gives the slot of this mesh back to the pool
    void ReleaseBoneSlot() const;
    //gives the region of the shared pool of skinned vertices back
    void ReleaseSkinnedVertices() const;

    //whether the passes draw the skinned vertices instead of the bind pose
    bool skinned_vertex_buffer() const;

    mutable bool resolved_ = false;
    mutable std::shared_ptr<const NodeTransformTable> node_table_;
    //the animator this mesh is skinned by, when it drives the pose directly
    mutable Animator* animator_ = nullptr;
    uint32_t node_index_ = 0;
    mutable std::vector<Transform*> bone_transforms_;
    mutable std::vector<Matrix4x4> prev_bone_world_;
    mutable Matrix4x4 prev_world_to_local_ = Matrix4x4::identity;
    mutable bool has_prev_ = false;
    //whether the previous frame was skinned from the pose or from the transforms
    mutable bool prev_pose_based_ = false;
    //object space skinning matrices of this frame, refreshed once per frame so
    //that every pass uploads the same values (and the same previous ones); they
    //are copied into the slot of this mesh in the shared bone matrix pool
    mutable std::vector<Matrix4x4> bone_matrices_;
    mutable std::vector<Matrix4x4> prev_bone_matrices_;
    //room for the bones of this mesh in the shared bone matrix pool, by the
    //actual bone count of the mesh instead of the worst case a shader accepts
    mutable uint32_t bone_slot_ = kInvalidBoneOffset;
    mutable uint32_t bone_slot_bones_ = 0;
    //region of the shared pool of skinned vertices, see GpuSkinning
    bool gpu_skinning_ = true;
    bool instancing_ = false;
    mutable uint32_t skinned_vertex_offset_ = GpuSkinning::kInvalidOffset;
    //size of the last batch this mesh was drawn in, only used to report it once
    mutable size_t batch_size_ = 0;
    mutable bool bone_matrices_cached_ = false;
};

}
}
