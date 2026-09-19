#pragma once

#include <memory>
#include <vector>
#include "Algorithm/Bvh.h"
#include "render/base/gfxdriver.h"
#include "render/material.h"
#include "Math/Vec3.h"
#include "Math/Quat.h"
#include "Math/Mat4.h"
#include "Core/Transform.h"
#include "geometry/aabb.h"
#include "core/component.h"
#include "common/list.h"
#include "Common/TypeTraits.h"
#include "core/linkable.h"
#include "core/identifiable.h"
#include "core/objectmanager.h"
#include "Core/GameObject.h"
#include "Render/Skinning/BoneMatrixPool.h"

namespace glacier {
namespace render {

class PassNode;
class RenderableManager;

enum class RenderableMask : uint32_t {
    kUnPickable = 1 << 0,
    kCastShadow = 1 << 1,
    kReciveShadow = 1 << 2,
};

struct PerObjectData
{
    Matrix4x4 m;
    Matrix4x4 mv;
    Matrix4x4 mvp;
    Matrix4x4 prev_m;
    Vec4f uv_st = { 1.0f, 1.0f, 0.0f, 0.0f };
    //where the bones of this object are in the shared bone matrix pool, and
    //where the records of a batched draw start in the instance buffer; both are
    //kInvalidBoneOffset (or 0) for an object that has neither
    uint32_t bone_offset = kInvalidBoneOffset;
    uint32_t prev_bone_offset = kInvalidBoneOffset;
    uint32_t instance_offset = 0;
    uint32_t padding = 0;
};

//bones past this are dropped at import; a mesh keeps its own count, so the
//limit is only what one vertex can hold
constexpr uint32_t kMaxBones = 128;

class Renderable;
using RenderableTree = BvhTree<Renderable*>;
using RenderableTreeNode = RenderableTree::NodeType;

class Renderable :
    public Component,
    public Identifiable<Renderable>,
    public Linkable<RenderableManager, Renderable> 
{
public:
    friend class RenderableManager;

    Renderable();
    virtual ~Renderable();

    static void Setup();

    virtual void Render(CommandBuffer* cmd_buffer, Material* mat = nullptr) const = 0;
    virtual void Draw(CommandBuffer* cmd_buffer) const = 0;

    //Refreshes the data every pass of one frame shares: the model matrix the
    //passes report as the previous one and, for skinned meshes, the bone
    //matrices. It runs once per frame between LateUpdate and the passes, so no
    //pass has to rebuild it and no pass can overwrite the previous frame data.
    virtual void UpdateRenderData() const;

    //GPU work a renderable has to do before the passes draw it; it runs once
    //per frame, from UpdateSkinning of the manager. Only the skinned meshes use
    //it today: they deform their vertices into a shared pool that the passes
    //then draw from.
    virtual void DispatchSkinning(CommandBuffer* cmd_buffer) const {}

    //Whether this renderable and `other` can be drawn with one instanced draw
    //call, which needs them to share every resource the draw binds. `mat` is
    //the material of the pass, or null when the material of the object is used.
    virtual bool CanBatchWith(const Renderable* other, Material* mat) const { return false; }
    //Draws the objects of a batch of which this renderable is the first; only
    //called for a batch this renderable accepted with CanBatchWith.
    virtual void RenderBatch(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat) const {}

    void UpdatePerObjectData(CommandBuffer* cmd_buffer) const;
    //writes the per object data of one draw of a batch: the object data of the
    //instances comes from the instance buffer, so only where the batch starts
    //matters for the draw
    void UpdateBatchData(CommandBuffer* cmd_buffer, uint32_t instance_offset) const;

    const AABB& local_bounds() const { return local_bounds_; }
    const AABB& world_bounds() const;

    uint32_t mask() const { return mask_; }

    bool IsPickable() const { return (mask_ & toUType(RenderableMask::kUnPickable)) == 0 && !game_object()->IsHidden(); }
    bool IsCastShadow() const { return (mask_ & toUType(RenderableMask::kCastShadow)) != 0; }
    bool IsReciveShadow() const { return (mask_ & toUType(RenderableMask::kReciveShadow)) != 0; }

    void SetCastShadow(bool on);
    void SetReciveShadow(bool on);
    void SetPickable(bool on);

    void SetMaterial(const std::shared_ptr<Material>& mat);
    const std::shared_ptr<Material>& GetMaterial() const { return material_; }

    void DrawInspectorBasic();

    static std::shared_ptr<Buffer>& GetPerObjectData();

    //Slot of the bones of this object in the shared bone matrix pool, and the
    //same for the pose it was drawn with in the previous frame.
    //kInvalidBoneOffset when the object is not skinned.
    virtual uint32_t bone_offset() const { return kInvalidBoneOffset; }
    virtual uint32_t prev_bone_offset() const { return kInvalidBoneOffset; }

protected:
    void UpdateWorldBounds() const;

    static std::shared_ptr<Buffer> per_object_data_; //shared by all material
    static int32_t id_counter_;

    uint32_t mask_ = 0;
    mutable uint32_t bounds_version_ = 0;

    AABB local_bounds_ = {Vector3::zero, Vector3::zero};
    mutable AABB world_bounds_;
    //the model matrix the passes report as the previous one; only
    //UpdateRenderData may write it, otherwise the last pass of the frame leaves
    //a stale value for the velocity buffer
    mutable Matrix4x4 prev_model_ = Matrix4x4::identity;
    //the model matrix captured during this frame, reported as the previous one
    //by the next frame
    mutable Matrix4x4 frame_model_ = Matrix4x4::identity;
    mutable bool frame_model_valid_ = false;

    std::shared_ptr<Material> material_;

    RenderableTreeNode* node_ = nullptr;

private:
    //snapshots the model matrix of this frame, so the next frame can report it
    //as the previous one
    void CaptureFrameModel() const;
};

class Camera;

class RenderableManager : public BaseManager<RenderableManager, Renderable> {
public:
    using CullFilter = std::function<bool(const Renderable*)>;

    RenderableManager() : tree_(4096) {}

    void UpdateBvhNode(Renderable* o);
    void RemoveBvhNode(Renderable* o);

    //refreshes the per frame render data of every renderable; call it once per
    //frame, after the update phase and before anything is rendered
    void UpdateRenderData();

    //Dispatches the GPU work of every renderable of the scene, once per frame
    //and before the passes. The shadow pass casts from objects outside the
    //camera frustum as well, so this cannot be limited to the visible ones.
    void UpdateSkinning(CommandBuffer* cmd_buffer);

    bool Cull(const Camera& camera, std::vector<Renderable*>& result, const CullFilter& filter = nullptr);
    bool Cull(const Frustum& frustum, std::vector<Renderable*>& result, const CullFilter& filter = nullptr);

    AABB SceneBounds();

    void OnDrawGizmos();

private:
    void DrawNode(const RenderableTreeNode* node);

    RenderableTree tree_;
};


}
}
