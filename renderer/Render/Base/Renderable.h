#pragma once

#include <memory>
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
    Vec4f uv_st = { 1.0f, 1.0f, 0.0f, 0.0f };
};

class Renderable :
    public Component,
    public Identifiable<Renderable>,
    public Linkable<RenderableManager, Renderable> 
{
public:
    Renderable();
    virtual ~Renderable();

    virtual void Render(GfxDriver* gfx) const = 0;

    void Bind(GfxDriver* gfx, Transform* tx = nullptr) const;

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

protected:
    void UpdateWorldBounds() const;

    static std::shared_ptr<ConstantBuffer> tx_buf_;
    static int32_t id_counter_;

    ConstantBuffer& GetTransformCBuffer(GfxDriver* gfx) const;

    uint32_t mask_ = 0;
    mutable uint32_t bounds_version_ = 0;

    //std::vector<PassNode*> passes_;
    AABB local_bounds_;
    mutable AABB world_bounds_;

    Material* material_ = nullptr;
};

class RenderableManager : public BaseManager<RenderableManager, Renderable> {

};


}
}
