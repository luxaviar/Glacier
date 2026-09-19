#pragma once

#include <memory>
#include <vector>
#include "Render/Mesh/MeshRenderer.h"
#include "Math/Mat4.h"

namespace glacier {

class Transform;

namespace render {

//Renders a skinned Mesh. The bone hierarchy is not owned by this component:
//bones are looked up by name in the GameObject hierarchy and driven by the
//Animator (or by script), exactly like any other node transform.
class SkinnedMeshRenderer : public MeshRenderer {
public:
    SkinnedMeshRenderer() {}
    SkinnedMeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material = {});

    void Render(CommandBuffer* cmd_buffer, Material* mat = nullptr) const override;
    //recomputes the bone matrices of this frame; the render path only uploads
    //what this produced
    void UpdateRenderData() const override;
    void DrawInspector() override;

    //re-resolves the bone transforms; the first Render does this automatically
    void RefreshBones();
    size_t bone_count() const;

private:
    std::shared_ptr<Mesh> skinned_mesh() const;
    void ResolveBones() const;
    void UpdateBoneMatrices() const;

    mutable bool resolved_ = false;
    mutable std::vector<Transform*> bone_transforms_;
    mutable std::vector<Matrix4x4> prev_bone_world_;
    mutable Matrix4x4 prev_world_to_local_ = Matrix4x4::identity;
    mutable bool has_prev_ = false;
    //object space skinning matrices of this frame, refreshed once per frame so
    //that every pass uploads the same values (and the same previous ones)
    mutable BoneMatrices bone_matrices_;
    mutable bool bone_matrices_cached_ = false;
};

}
}
