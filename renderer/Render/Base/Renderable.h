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

namespace glacier {
namespace render {

class PassNode;
class RenderableManager;

enum class RenderableMask : uint32_t {
    kUnPickable = 1 << 0,
    kCastShadow = 1 << 1,
    kReciveShadow = 1 << 2,
};

struct RenderableTransform
{
    Matrix4x4 m;
    Matrix4x4 mv;
    Matrix4x4 mvp;
    Matrix4x4 prev_m;
    Vec4f uv_st = { 1.0f, 1.0f, 0.0f, 0.0f };
};

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

    virtual void Render(GfxDriver* gfx, Material* mat) const = 0;
    virtual void Draw(GfxDriver* gfx) const = 0;

    void UpdateTransform(GfxDriver* gfx) const;

    const AABB& local_bounds() const { return local_bounds_; }
    const AABB& world_bounds() const;

    uint32_t mask() const { return mask_; }

    bool IsPickable() const { return (mask_ & toUType(RenderableMask::kUnPickable)) == 0 && !game_object()->IsHidden(); }
    bool IsCastShadow() const { return (mask_ & toUType(RenderableMask::kCastShadow)) != 0; }
    bool IsReciveShadow() const { return (mask_ & toUType(RenderableMask::kReciveShadow)) != 0; }

    void SetCastShadow(bool on);
    void SetReciveShadow(bool on);
    void SetPickable(bool on);

    void SetMaterial(Material* mat);
    Material* GetMaterial() const { return material_; }

    void DrawInspectorBasic();

    static std::shared_ptr<ConstantBuffer>& GetTransformCBuffer(GfxDriver* gfx);

protected:
    void UpdateWorldBounds() const;

    static std::shared_ptr<ConstantBuffer> tx_buf_; //shared by all material
    static int32_t id_counter_;

    uint32_t mask_ = 0;
    mutable uint32_t bounds_version_ = 0;

    AABB local_bounds_ = {Vector3::zero, Vector3::zero};
    mutable AABB world_bounds_;
    mutable Matrix4x4 prev_model_ = Matrix4x4::identity;

    Material* material_ = nullptr;

    RenderableTreeNode* node_ = nullptr;
};

class Camera;

class RenderableManager : public BaseManager<RenderableManager, Renderable> {
public:
    using CullFilter = std::function<bool(const Renderable*)>;

    RenderableManager() : tree_(4096) {}

    void UpdateBvhNode(Renderable* o);
    void RemoveBvhNode(Renderable* o);

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
