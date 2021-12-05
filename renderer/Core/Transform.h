#pragma once

#include <vector>
#include <list>
#include <stdint.h>
#include "Math/Vec3.h"
#include "Math/Quat.h"
#include "Math/Mat4.h"
#include "component.h"

namespace glacier {

class Transform : public Component {
public:
    Transform(const Vec3f& pos = Vec3f::zero, 
        const Quaternion& rot = Quaternion::identity, const Vec3f& scale = Vec3f::one);

    //make sure mat is transform matrix
    Transform(const Matrix4x4& mat);

    ~Transform();

    void SetParent(Transform* parent);
    void SetPositionAndRotation(const Vec3f& pos, const Quaternion& rot);

    //local -> world
    Vec3f ApplyTransform(const Vec3f& point) const;
    //world -> local
    Vec3f InverseTransform(const Vec3f& point) const;
    //local -> world
    Vec3f ApplyTransformVector(const Vec3f& dir) const;
    //world -> local
    Vec3f InverseTransformVector(const Vec3f& dir) const;
    const Matrix4x4& LocalToWorldMatrix() const;
    const Matrix4x4& WorldToLocalMatrix() const;
    Matrix4x4 LocalToParentMatrix() const;

    uint32_t version() const { return version_; }
    Transform* parent() const { return parent_; }

    const Vec3f& local_position() const { return local_position_; }
    void local_position(const Vec3f& pos);

    const Quaternion& local_rotation() const { return local_rotation_; }
    void local_rotation(const Quaternion& rot);

    Vec3f local_scale() const { return local_scale_; }
    void local_scale(const Vec3f& scale);

    //get/set in world space
    Vec3f position() const;
    void position(const Vec3f& pos);

    //get/set in world space
    Quaternion rotation() const;
    void rotation(const Quaternion& rot);

    //Vec3f scale() const;
    //void scale(const Vec3f& scale);

    Vec3f forward() const { return rotation() * Vec3f::forward; }
    void forward(const Vec3f& dir);

    Vec3f right() const { return rotation() * Vec3f::right; }
    void right(const Vec3f& dir);

    Vec3f up() const { return rotation() * Vec3f::up; }
    void up(const Vec3f& dir);

    const std::vector<Transform*>& children() const { return children_; }
    void DrawInspector() override;
    void OnDrawSelectedGizmos() override;

private:
    void AddChild(Transform* child);
    void RemoveChild(Transform* child);

    void MarkDirty();

private:
    bool noscale_ = true;
    mutable uint32_t version_;
    mutable uint32_t matrix_version_;
    mutable uint32_t inv_matrix_version_;

    Quaternion local_rotation_; //local -> parent
    Vec3f local_position_; //local -> parent
    Vec3f local_scale_;
    mutable Matrix4x4 local_to_world_;
    mutable Matrix4x4 world_to_local_;

    Transform* parent_;
    std::vector<Transform*> children_;
    int gizmo_op_;
};

}


